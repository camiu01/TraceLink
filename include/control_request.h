/**
 * @file control_request.h
 * @brief Shared, hardware-independent validation for HTTP and USB controls.
 */
#pragma once
#include "commands.h"

enum class Action : uint8_t { Rate, Scan, Arm, Disarm, Send };
struct Command {
	Action action = Action::Disarm;
	size_t bus = 0;
	uint32_t rate = 0;
	Capture event;
};
enum ControlField { ActionField, BusField, RateField, EncodingField, DataField,
	IdField, ExtendedField, RtrField, DlcField, ControlFieldCount };
static const char* const controlKeys[] = {
	"action", "bus", "rate", "encoding", "data", "id", "extended", "rtr", "dlc"
};
struct ControlInput {
	const char* values[ControlFieldCount];
	ControlInput() { for (auto& value : values) value = ""; }
	const char* operator[](ControlField field) const { return values[field]; }
	bool set(size_t key, const char* value, size_t length) {
		if (key >= ControlFieldCount || strlen(value) != length) return false;
		values[key] = value;
		return true;
	}
};

inline const char* parsePayload(const ControlInput& input, Capture& event) {
	const char* payload = input[DataField];
	const size_t length = strlen(payload);
	if (length > 256) return "Comando troppo lungo.";
	if (strcmp(input[EncodingField], "hex") == 0) {
		if (!parseHex(payload, event.data, event.source == Source::Can ? 8 : 64, event.length))
			return "HEX non valido; usa coppie come 01 03 FF.";
	} else if (strcmp(input[EncodingField], "text") == 0 && event.source != Source::Can) {
		if (length == 0 || length > 64) return "Il testo deve contenere da 1 a 64 byte UTF-8.";
		event.length = length;
		memcpy(event.data, payload, length);
	} else return "Codifica non valida.";
	if (event.source != Source::Can) return event.length ? nullptr : "Dati vuoti.";
	event.extended = strcmp(input[ExtendedField], "1") == 0;
	event.remote = strcmp(input[RtrField], "1") == 0;
	if (!parseUnsigned(input[IdField], event.extended ? 0x1FFFFFFF : 0x7FF, event.identifier, 16))
		return "ID CAN HEX fuori intervallo (senza prefisso 0x).";
	if (event.remote) {
		uint32_t dlc;
		if (event.length || !parseUnsigned(input[DlcField], 8, dlc))
			return "RTR richiede dati vuoti e DLC tra 0 e 8.";
		event.dlc = dlc;
	} else event.dlc = event.length;
	return nullptr;
}

inline const char* parseControl(const ControlInput& input, Command& command) {
	command = Command();
	const char* action = input[ActionField];
	if (strcmp(action, "scan") == 0) command.action = Action::Scan;
	else if (strcmp(action, "arm") == 0) command.action = Action::Arm;
	else if (strcmp(action, "disarm") == 0) command.action = Action::Disarm;
	else if (strcmp(action, "rate") == 0 || strcmp(action, "send") == 0) {
		size_t index = 0;
		for (; index < sourceCount; ++index)
			if (strcmp(input[BusField], sourceName(static_cast<Source>(index))) == 0) break;
		if (index == sourceCount) return "Interfaccia non valida.";
		command.event.source = static_cast<Source>(index);
		if (strcmp(action, "rate") == 0) {
			command.action = Action::Rate;
			if (!parseUnsigned(input[RateField], 1000000, command.rate) ||
				!supportedRate(command.event.source, command.rate)) return "Velocita non supportata.";
		} else {
			command.action = Action::Send;
			return parsePayload(input, command.event);
		}
	} else return "Azione non valida.";
	return nullptr;
}

inline bool controlAllowed(Action action, bool busy, bool scanning, bool armed) {
	return !busy && !scanning && !(action == Action::Send && !armed) &&
		!(action == Action::Scan && armed);
}
