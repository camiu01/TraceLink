/**
 * @file usb_console.cpp
 * @brief USB capture mirror and framed API requests on UART0, without bus ownership.
 */
#include "runtime.h"
#include "usb_protocol.h"

namespace tracelink {
namespace {
UsbLine inputLine;

void reply(const char* id, int code, const char* body) {
	xSemaphoreTake(usbMutex, portMAX_DELAY);
	Serial.printf("TL1 %s %d ", id, code);
	Serial.println(body);
	xSemaphoreGive(usbMutex);
}

void handleUsb(char* line) {
	char* id;
	char* payload;
	if (!parseUsbEnvelope(line, id, payload)) return;
	if (strncmp(payload, "events ", 7) == 0) {
		uint64_t after;
		if (!parseCursor(payload + 7, after)) { reply(id, 400, "invalid cursor"); return; }
		const String json = eventsJson(after);
		reply(id, 200, json.c_str());
		return;
	}
	if (strncmp(payload, "control ", 8) != 0) { reply(id, 400, "invalid operation"); return; }
	char* token = payload + 8;
	char* form = strchr(token, ' ');
	if (!form) { reply(id, 400, "missing form"); return; }
	*form++ = '\0';
	ControlInput input;
	if (!parseControlForm(form, input)) { reply(id, 400, "invalid form"); return; }
	const char* response;
	const int code = submitControl(input, token, response);
	reply(id, code, response);
}
} // namespace

void pollUsb() {
	inputLine.expire(millis());
	for (size_t budget = 0; budget < 128 && Serial.available(); ++budget) {
		if (inputLine.push(static_cast<char>(Serial.read()), millis())) handleUsb(inputLine.data());
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
		xSemaphoreTake(usbMutex, portMAX_DELAY);
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
		xSemaphoreGive(usbMutex);
		vTaskDelay(pdMS_TO_TICKS(5));
	}
}
} // namespace tracelink
