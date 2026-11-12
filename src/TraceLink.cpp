/**
 * @file TraceLink.cpp
 * @brief Acquisition tasks, interface registry, speed scan, and HTTP API.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <freertos/semphr.h>

#include "bus_interface.h"
#include "commands.h"
#include "config.h"
#include "runtime.h"

namespace tracelink {
UartBus rs232(Source::Rs232, 2, config::rs232Rx, config::rs232Tx);
UartBus rs485(Source::Rs485, 1, config::rs485Rx, config::rs485Tx, config::rs485De);
CanBus can(config::canRx, config::canTx);
// Extension point: register a new adapter here and add its Source in capture.h.
BusInterface* buses[] = {&rs232, &rs485, &can};
static_assert(sizeof(buses) / sizeof(buses[0]) == sourceCount, "Register every source exactly once");
CaptureHistory<config::historyCapacity> history;
SemaphoreHandle_t mutex = nullptr;
SemaphoreHandle_t usbMutex = nullptr;
char bootId[9] = {};
portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;
PulseHistogram pulses[2];

Status status;
Command pendingCommand;
bool scanning = false;
bool sampling = false;
uint32_t scanStarted = 0;
uint32_t canStepStarted = 0;
uint32_t savedCanRate = 0;
size_t canStep = 0;
size_t canFrames = 0;
bool canScanDone = false;
uint32_t lastTransmitActivity = 0;

void message(const char* text) {
	xSemaphoreTake(mutex, portMAX_DELAY);
	strlcpy(status.message, text, sizeof(status.message));
	xSemaphoreGive(mutex);
}

void record(Capture event) {
	event.timestamp = millis();
	xSemaphoreTake(mutex, portMAX_DELAY);
	history.push(event);
	xSemaphoreGive(mutex);
}

void IRAM_ATTR sampleEdge(void* argument) {
	auto* histogram = static_cast<PulseHistogram*>(argument);
	const uint32_t now = micros();
	portENTER_CRITICAL_ISR(&pulseMux);
	if (histogram->seen) {
		const uint32_t interval = now - histogram->previous;
		if (interval >= 6 && interval <= 1000 && histogram->bins[interval] < UINT16_MAX) {
			++histogram->bins[interval];
		}
	}
	histogram->previous = now;
	histogram->seen = true;
	++histogram->edges;
	portEXIT_CRITICAL_ISR(&pulseMux);
}

void startScan() {
	portENTER_CRITICAL(&pulseMux);
	for (auto& pulse : pulses) {
		memset(pulse.bins, 0, sizeof(pulse.bins));
		pulse.edges = pulse.previous = 0;
		pulse.seen = false;
	}
	portEXIT_CRITICAL(&pulseMux);
	attachInterruptArg(config::rs232Rx, sampleEdge, &pulses[0], CHANGE);
	attachInterruptArg(config::rs485Rx, sampleEdge, &pulses[1], CHANGE);
	sampling = true;
	scanning = true;
	scanStarted = millis();
	canStepStarted = scanStarted;
	savedCanRate = can.rate();
	canStep = 0;
	canFrames = 0;
	canScanDone = false;
	can.begin(canRates[canStep]);
	xSemaphoreTake(mutex, portMAX_DELAY);
	status.scanning = true;
	status.estimates[0] = status.estimates[1] = status.canDetected = 0;
	xSemaphoreGive(mutex);
	message("Controllo passivo in corso. Genera traffico sui bus.");
}

void advanceScan() {
	if (!scanning) return;
	const uint32_t now = millis();
	if (sampling && now - scanStarted >= 3000) {
		detachInterrupt(config::rs232Rx);
		detachInterrupt(config::rs485Rx);
		sampling = false;
		portENTER_CRITICAL(&pulseMux);
		const uint32_t first = estimateBaud(pulses[0]);
		const uint32_t second = estimateBaud(pulses[1]);
		portEXIT_CRITICAL(&pulseMux);
		xSemaphoreTake(mutex, portMAX_DELAY);
		status.estimates[0] = first;
		status.estimates[1] = second;
		xSemaphoreGive(mutex);
	}
	if (!canScanDone && now - canStepStarted >= 1200) {
		++canStep;
		if (canStep >= sizeof(canRates) / sizeof(canRates[0])) {
			can.begin(savedCanRate);
			canScanDone = true;
		} else {
			can.begin(canRates[canStep]);
			canFrames = 0;
			canStepStarted = now;
		}
	}
	if (!sampling && canScanDone) {
		scanning = false;
		xSemaphoreTake(mutex, portMAX_DELAY);
		status.scanning = false;
		xSemaphoreGive(mutex);
		message("Controllo finito. Conferma le stime seriali; zero indica nessun risultato.");
	}
}

void execute(const Command& command) {
	bool armed;
	xSemaphoreTake(mutex, portMAX_DELAY);
	armed = status.armed;
	xSemaphoreGive(mutex);
	if (command.action == Action::Scan) {
		if (armed || scanning) message("Disabilita TX prima del controllo velocita.");
		else startScan();
		return;
	}
	if (scanning) {
		message("Attendi la fine del controllo velocita.");
		return;
	}
	if (command.action == Action::Arm || command.action == Action::Disarm) {
		const bool enable = command.action == Action::Arm;
		bool ok = true;
		for (auto* bus : buses) if (!bus->setTransmit(enable)) ok = false;
		if (enable && !ok) {
			// Fail closed: a partial enable must not leave TX armed.
			for (auto* bus : buses) bus->setTransmit(false);
		}
		xSemaphoreTake(mutex, portMAX_DELAY);
		status.armed = enable && ok;
		xSemaphoreGive(mutex);
		lastTransmitActivity = millis();
		message(ok ? (enable ? "TX abilitata. CAN in modo normale: invia ACK."
							 : "TX disabilitata. CAN solo ascolto.")
				   : "Cambio modo fallito. TX disabilitata; verifica il driver CAN.");
	} else if (command.action == Action::Rate) {
		const bool ok = buses[command.bus]->begin(command.rate);
		message(ok ? "Velocita aggiornata (non persistente al riavvio)."
				   : "Impossibile impostare la velocita.");
	} else if (command.action == Action::Send) {
		if (!armed) {
			message("Invio rifiutato: TX disabilitata.");
			return;
		}
		lastTransmitActivity = millis();
		if (buses[command.bus]->send(command.event)) {
			Capture event = command.event;
			event.transmitted = true;
			record(event);
			message(event.source == Source::Can
				? "CAN accodato single-shot; non e una conferma di consegna."
				: "Byte inviati sulla UART; non e una conferma del dispositivo.");
		} else message("Invio fallito. Verifica driver, bus e stato TX.");
	}
}

void acquire(void*) {
	for (;;) {
		Command command;
		bool runCommand = false;
		xSemaphoreTake(mutex, portMAX_DELAY);
		if (status.pending) {
			command = pendingCommand;
			status.pending = false;
			runCommand = true;
		}
		xSemaphoreGive(mutex);
		if (runCommand) {
			execute(command);
			xSemaphoreTake(mutex, portMAX_DELAY);
			status.busy = false;
			xSemaphoreGive(mutex);
		}
		bool armed;
		xSemaphoreTake(mutex, portMAX_DELAY);
		armed = status.armed;
		xSemaphoreGive(mutex);
		if (armed && millis() - lastTransmitActivity >= config::transmitWindowMs) {
			Command disarm;
			disarm.action = Action::Disarm;
			execute(disarm);
			message("TX disabilitata automaticamente dopo 60 secondi senza invii.");
		}
		for (auto* bus : buses) {
			Capture event;
			const size_t limit = bus->source() == Source::Can ? 32 : 4;
			for (size_t i = 0; i < limit && bus->receive(event); ++i) {
				if (scanning && bus->source() == Source::Can && !canScanDone) {
					if (++canFrames >= 3) {
						canScanDone = true;
						xSemaphoreTake(mutex, portMAX_DELAY);
						status.canDetected = bus->rate();
						xSemaphoreGive(mutex);
					}
				}
				record(event);
			}
		}
		advanceScan();
		xSemaphoreTake(mutex, portMAX_DELAY);
		for (size_t i = 0; i < busCount; ++i) {
			status.rates[i] = buses[i]->rate();
			status.health[i] = buses[i]->health();
		}
		xSemaphoreGive(mutex);
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

} // namespace tracelink

void setup() {
	using namespace tracelink;
	Serial.begin(config::usbBaud);
	snprintf(bootId, sizeof(bootId), "%08lx", static_cast<unsigned long>(esp_random()));
	mutex = xSemaphoreCreateMutex();
	usbMutex = xSemaphoreCreateMutex();
	if (!mutex || !usbMutex) {
		Serial.println("Memoria insufficiente per il mutex");
		while (true) delay(1000);
	}
	rs232.begin(config::rs232Baud);
	rs485.begin(config::rs485Baud);
	can.begin(config::canBitrate);
	for (size_t i = 0; i < busCount; ++i) {
		status.rates[i] = buses[i]->rate();
		status.health[i] = buses[i]->health();
	}
	WiFi.mode(WIFI_AP);
	const bool wifiReady = WiFi.softAP(config::apSsid, config::apPassword, 1, false, 4);
	Serial.println("\nTraceLink | RX only al boot | TX da abilitare esplicitamente");
	if (wifiReady) {
		Serial.printf("Wi-Fi: %s | http://%s\n", config::apSsid, WiFi.softAPIP().toString().c_str());
		setupHttp();
	} else Serial.println("Avvio Wi-Fi fallito; acquisizione USB ancora disponibile");
	if (xTaskCreate(acquire, "capture", 6144, nullptr, 2, nullptr) != pdPASS) {
		Serial.println("Impossibile creare il task di acquisizione");
		while (true) delay(1000);
	}
	if (xTaskCreate(usbOutput, "usb-log", 4096, nullptr, 1, nullptr) != pdPASS) {
		Serial.println("Impossibile creare il task USB");
	}
}

void loop() {
	tracelink::pollUsb();
	tracelink::pollHttp();
	delay(2);
}
