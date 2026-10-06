/**
 * @file TraceLink.cpp
 * @brief Acquisition tasks, interface registry, speed scan, and HTTP API.
 */
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <freertos/semphr.h>

#include "bus_interface.h"
#include "commands.h"
#include "config.h"
#include "web_page.h"

namespace {
UartBus rs232(Source::Rs232, 2, config::rs232Rx, config::rs232Tx);
UartBus rs485(Source::Rs485, 1, config::rs485Rx, config::rs485Tx, config::rs485De);
CanBus can(config::canRx, config::canTx);
// Extension point: register a new adapter here and add its Source in capture.h.
BusInterface* buses[] = {&rs232, &rs485, &can};
constexpr size_t busCount = sizeof(buses) / sizeof(buses[0]);
static_assert(busCount == sourceCount, "Register every source exactly once");
WebServer server(80);
CaptureHistory<config::historyCapacity> history;
SemaphoreHandle_t mutex = nullptr;
char bootId[9] = {};
Capture apiEvents[config::apiBatchSize];
portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;
PulseHistogram pulses[2];

enum class Action : uint8_t { Rate, Scan, Arm, Disarm, Send };
struct Command {
	Action action = Action::Disarm;
	size_t bus = 0;
	uint32_t rate = 0;
	Capture event;
};
struct Status {
	bool armed = false;
	bool busy = false;
	bool scanning = false;
	bool pending = false;
	uint32_t rates[busCount] = {};
	BusHealth health[busCount];
	uint32_t estimates[2] = {};
	uint32_t canDetected = 0;
	char message[128] = "Pronto. Trasmissione disabilitata.";
};
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

void usbOutput(void*) {
	uint64_t cursor = 0;
	for (;;) {
		Capture events[4];
		uint64_t lost = 0;
		xSemaphoreTake(mutex, portMAX_DELAY);
		const size_t size = history.read(cursor, events, 4, lost);
		xSemaphoreGive(mutex);
		if (lost) Serial.printf("[USB] %llu record persi dalla cronologia\n",
								static_cast<unsigned long long>(lost));
		for (size_t i = 0; i < size; ++i) {
			const Capture& event = events[i];
			char hex[192];
			formatHex(event, hex);
			Serial.printf("#%llu %lu ms %-5s %s ",
						  static_cast<unsigned long long>(event.sequence),
						  static_cast<unsigned long>(event.timestamp),
						  sourceName(event.source), event.transmitted ? "TX" : "RX");
			if (event.source == Source::Can) {
				Serial.printf("ID=%08lX %s %s DLC=%u ",
							  static_cast<unsigned long>(event.identifier),
							  event.extended ? "EXT" : "STD",
							  event.remote ? "RTR" : "DATA", event.dlc);
			}
			Serial.println(hex);
			cursor = event.sequence;
		}
		vTaskDelay(pdMS_TO_TICKS(5));
	}
}

void serveEvents() {
	uint64_t after = 0;
	const String argument = server.arg("after");
	if (argument.length() > 15) {
		server.send(400, "application/json", "{\"error\":\"invalid cursor\"}");
		return;
	}
	for (size_t i = 0; i < argument.length(); ++i) {
		if (argument[i] < '0' || argument[i] > '9') {
			server.send(400, "application/json", "{\"error\":\"invalid cursor\"}");
			return;
		}
		after = after * 10 + (argument[i] - '0');
	}
	uint64_t lost = 0, counts[busCount], latest, overwritten;
	Status snapshot;
	xSemaphoreTake(mutex, portMAX_DELAY);
	const size_t size = history.read(after, apiEvents, config::apiBatchSize, lost);
	latest = history.latest();
	overwritten = history.overwritten();
	for (size_t i = 0; i < busCount; ++i) counts[i] = history.count(buses[i]->source());
	snapshot = status;
	xSemaphoreGive(mutex);

	String json;
	json.reserve(14000);
	char buffer[768];
	const BusHealth& canHealth = snapshot.health[2];
	snprintf(buffer, sizeof(buffer),
			 "{\"boot\":\"%s\",\"uptime\":%lu,\"latest\":%llu,\"lost\":%llu,"
			 "\"overwritten\":%llu,\"canReady\":%s,\"canMissed\":%lu,"
			 "\"canOverrun\":%lu,\"canErrors\":%lu,\"armed\":%s,\"busy\":%s,"
			 "\"scanning\":%s,\"estimates\":[%lu,%lu],\"canDetected\":%lu,\"message\":\"%s\",",
			 bootId, static_cast<unsigned long>(millis()),
			 static_cast<unsigned long long>(latest), static_cast<unsigned long long>(lost),
			 static_cast<unsigned long long>(overwritten), canHealth.ready ? "true" : "false",
			 static_cast<unsigned long>(canHealth.missed),
			 static_cast<unsigned long>(canHealth.overrun),
			 static_cast<unsigned long>(canHealth.errors), snapshot.armed ? "true" : "false",
			 snapshot.busy ? "true" : "false", snapshot.scanning ? "true" : "false",
			 static_cast<unsigned long>(snapshot.estimates[0]),
			 static_cast<unsigned long>(snapshot.estimates[1]),
			 static_cast<unsigned long>(snapshot.canDetected), snapshot.message);
	json += buffer;
	// Bus descriptors drive the dashboard; adding an adapter creates its controls.
	json += "\"interfaces\":[";
	for (size_t i = 0; i < busCount; ++i) {
		snprintf(buffer, sizeof(buffer),
				 "%s{\"name\":\"%s\",\"protocol\":\"%s\",\"ready\":%s,\"rate\":%lu,\"count\":%llu}",
				 i ? "," : "", sourceName(buses[i]->source()), buses[i]->protocol(),
				 snapshot.health[i].ready ? "true" : "false",
				 static_cast<unsigned long>(snapshot.rates[i]),
				 static_cast<unsigned long long>(counts[i]));
		json += buffer;
	}
	json += "],\"events\":[";
	for (size_t i = 0; i < size; ++i) {
		const Capture& event = apiEvents[i];
		char hex[192];
		formatHex(event, hex);
		snprintf(buffer, sizeof(buffer),
				 "%s{\"seq\":%llu,\"ms\":%lu,\"source\":\"%s\",\"hex\":\"%s\","
				 "\"id\":%lu,\"ext\":%s,\"rtr\":%s,\"dlc\":%u,\"tx\":%s}",
				 i ? "," : "", static_cast<unsigned long long>(event.sequence),
				 static_cast<unsigned long>(event.timestamp), sourceName(event.source), hex,
				 static_cast<unsigned long>(event.identifier),
				 event.extended ? "true" : "false", event.remote ? "true" : "false",
				 event.dlc, event.transmitted ? "true" : "false");
		json += buffer;
	}
	json += "]}";
	server.sendHeader("Cache-Control", "no-store");
	server.send(200, "application/json", json);
}

void control() {
	auto reject = [](int code, const char* error) { server.send(code, "text/plain", error); };
	// Custom header is required: cross-origin HTML forms cannot arm/send.
	// No CORS is enabled. Wi-Fi access is still the authentication boundary.
	if (server.header("X-TraceLink") != bootId) {
		reject(403, "Ricarica la pagina TraceLink (token di avvio non valido).");
		return;
	}
	Command command;
	const String action = server.arg("action");
	if (action == "scan") command.action = Action::Scan;
	else if (action == "arm") command.action = Action::Arm;
	else if (action == "disarm") command.action = Action::Disarm;
	else if (action == "rate" || action == "send") {
		bool found = false;
		for (size_t i = 0; i < busCount; ++i) {
			if (server.arg("bus") == sourceName(buses[i]->source())) {
				command.bus = i;
				found = true;
				break;
			}
		}
		if (!found) { reject(400, "Interfaccia non valida."); return; }
		command.event.source = buses[command.bus]->source();
		if (action == "rate") {
			command.action = Action::Rate;
			if (!parseUnsigned(server.arg("rate").c_str(), 1000000, command.rate) ||
				!supportedRate(command.event.source, command.rate)) {
				reject(400, "Velocita non supportata."); return;
			}
		} else {
			command.action = Action::Send;
			Capture& event = command.event;
			const String payload = server.arg("data");
			const String encoding = server.arg("encoding");
			if (payload.length() > 256) { reject(400, "Comando troppo lungo."); return; }
			if (encoding == "hex") {
				const size_t capacity = event.source == Source::Can ? 8 : 64;
				if (!parseHex(payload.c_str(), event.data, capacity, event.length)) {
					reject(400, "HEX non valido; usa coppie come 01 03 FF."); return;
				}
			} else if (encoding == "text" && event.source != Source::Can) {
				if (payload.length() == 0 || payload.length() > 64) {
					reject(400, "Il testo deve contenere da 1 a 64 byte UTF-8."); return;
				}
				event.length = payload.length();
				memcpy(event.data, payload.c_str(), event.length);
			} else { reject(400, "Codifica non valida."); return; }
			if (event.source == Source::Can) {
				event.extended = server.arg("extended") == "1";
				event.remote = server.arg("rtr") == "1";
				if (!parseUnsigned(server.arg("id").c_str(),
								   event.extended ? 0x1FFFFFFF : 0x7FF, event.identifier, 16)) {
					reject(400, "ID CAN HEX fuori intervallo (senza prefisso 0x)."); return;
				}
				if (event.remote) {
					uint32_t dlc;
					if (event.length != 0 || !parseUnsigned(server.arg("dlc").c_str(), 8, dlc)) {
						reject(400, "RTR richiede dati vuoti e DLC tra 0 e 8."); return;
					}
					event.dlc = dlc;
				} else event.dlc = event.length;
			} else if (!event.length) { reject(400, "Dati vuoti."); return; }
		}
	} else { reject(400, "Azione non valida."); return; }

	xSemaphoreTake(mutex, portMAX_DELAY);
	if (status.busy || status.scanning ||
		(command.action == Action::Send && !status.armed) ||
		(command.action == Action::Scan && status.armed)) {
		xSemaphoreGive(mutex);
		reject(409, "Operazione non consentita: controlla TX o attendi il controllo in corso.");
		return;
	}
	pendingCommand = command;
	status.pending = status.busy = true;
	xSemaphoreGive(mutex);
	server.send(202, "text/plain", "Richiesta accettata. Controlla l'esito nella pagina.");
}
} // namespace

void setup() {
	Serial.begin(config::usbBaud);
	snprintf(bootId, sizeof(bootId), "%08lx", static_cast<unsigned long>(esp_random()));
	mutex = xSemaphoreCreateMutex();
	if (!mutex) {
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
		server.on("/", HTTP_GET, []() {
			server.sendHeader("Cache-Control", "no-store");
			server.send_P(200, "text/html; charset=utf-8", webPage);
		});
		server.on("/api/events", HTTP_GET, serveEvents);
		server.on("/api/systems", HTTP_GET, []() {
			server.sendHeader("Cache-Control", "no-store");
			server.send_P(200, "application/json", systemsJson);
		});
		const char* headers[] = {"X-TraceLink"};
		server.collectHeaders(headers, 1);
		server.on("/api/control", HTTP_POST, control);
		server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
		server.begin();
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
	server.handleClient();
	delay(2);
}
