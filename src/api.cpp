/**
 * @file api.cpp
 * @brief HTTP endpoints and shared status/control entry points for HTTP and USB.
 */
#include <WebServer.h>
#include "runtime.h"
#include "usb_protocol.h"
#include "web_page.h"

namespace tracelink {
namespace { WebServer server(80); }

String eventsJson(uint64_t after) {
	static Capture apiEvents[config::apiBatchSize];
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
	return json;
}

int submitControl(const ControlInput& input, const char* token, const char*& response) {
	if (strcmp(token, bootId) != 0) {
		response = "Ricarica la pagina TraceLink (token di avvio non valido).";
		return 403;
	}
	Command command;
	response = parseControl(input, command);
	if (response) return 400;
	if (command.action == Action::Send || command.action == Action::Rate) {
		for (size_t i = 0; i < busCount; ++i)
			if (buses[i]->source() == command.event.source) { command.bus = i; break; }
	}
	xSemaphoreTake(mutex, portMAX_DELAY);
	if (!controlAllowed(command.action, status.busy, status.scanning, status.armed)) {
		xSemaphoreGive(mutex);
		response = "Operazione non consentita: controlla TX o attendi il controllo in corso.";
		return 409;
	}
	pendingCommand = command;
	status.pending = status.busy = true;
	xSemaphoreGive(mutex);
	response = "Richiesta accettata. Controlla l'esito nella pagina.";
	return 202;
}

void serveEvents() {
	uint64_t after;
	if (!parseCursor(server.arg("after").c_str(), after)) {
		server.send(400, "application/json", "{\"error\":\"invalid cursor\"}");
		return;
	}
	server.sendHeader("Cache-Control", "no-store");
	server.send(200, "application/json", eventsJson(after));
}

void control() {
	const String token = server.header("X-TraceLink");
	if (token.length() != 8 || strcmp(token.c_str(), bootId) != 0) {
		server.send(403, "text/plain", "Ricarica la pagina TraceLink (token di avvio non valido).");
		return;
	}
	String values[ControlFieldCount];
	ControlInput input;
	for (size_t i = 0; i < ControlFieldCount; ++i) {
		values[i] = server.arg(controlKeys[i]);
		if (!input.set(i, values[i].c_str(), values[i].length())) {
			server.send(400, "text/plain", "Byte NUL non valido nei campi; usa dati HEX.");
			return;
		}
	}
	const char* response;
	// Keep the custom-header check: cross-origin forms must not arm/send.
	const int code = submitControl(input, token.c_str(), response);
	server.send(code, "text/plain", response);
}

void setupHttp() {
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
}

void pollHttp() { server.handleClient(); }
} // namespace tracelink
