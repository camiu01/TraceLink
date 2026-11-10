/**
 * @file runtime.h
 * @brief Shared acquisition state and API/USB entry points. Buses belong to acquire().
 */
#pragma once
#include <Arduino.h>
#include <freertos/semphr.h>
#include "bus_interface.h"
#include "config.h"
#include "control_request.h"

namespace tracelink {
constexpr size_t busCount = sourceCount;
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
extern BusInterface* buses[];
extern CaptureHistory<config::historyCapacity> history;
extern SemaphoreHandle_t mutex;
extern SemaphoreHandle_t usbMutex;
extern char bootId[9];
extern Status status;
extern Command pendingCommand;

String eventsJson(uint64_t after);
int submitControl(const ControlInput& input, const char* token, const char*& response);
void setupHttp();
void pollHttp();
void pollUsb();
void usbOutput(void*);
}
