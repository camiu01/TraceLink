/**
 * @file control_usb_test.cpp
 * @brief Shared HTTP/USB validation, CRC framing and fail-closed input buffering.
 */
#include <assert.h>
#include <stdio.h>
#include <string>
#include "usb_protocol.h"

static bool form(const std::string& text, Command& command) {
	char buffer[2048];
	assert(text.size() < sizeof(buffer));
	memcpy(buffer, text.c_str(), text.size() + 1);
	ControlInput input;
	return parseControlForm(buffer, input) && parseControl(input, command) == nullptr;
}

static void controlTests() {
	Command command;
	assert(form("action=arm", command) && command.action == Action::Arm);
	assert(form("action=disarm", command) && command.action == Action::Disarm);
	assert(form("action=scan", command) && command.action == Action::Scan);
	assert(!form("action=unknown", command));
	assert(!form("action=arm&action=send", command));
	assert(!form("action=arm&unexpected=x", command));
	assert(!form("action=arm&", command));
	assert(!form("action", command));
	assert(!form("act%GGion=arm", command));
	assert(!form("action=arm%00", command));
	assert(!form("action=arm%0", command));
	assert(!form("action=arm%", command));
	for (const auto* bus : {"RS232", "RS485"}) {
		const std::string prefix = std::string("action=send&bus=") + bus;
		assert(form(prefix + "&encoding=hex&data=01+03%09FF", command));
		assert(command.event.length == 3 && command.event.data[2] == 255);
		assert(form(prefix + "&encoding=text&data=%C3%A0%26%3D%2B%0D%0A", command));
		assert(command.event.length == 7 && command.event.data[0] == 0xC3);
		assert(command.event.data[2] == '&' && command.event.data[3] == '=');
		assert(form(prefix + "&encoding=hex&data=" + std::string(128, 'F'), command));
		assert(command.event.length == 64);
		assert(!form(prefix + "&encoding=hex&data=" + std::string(130, 'F'), command));
		assert(!form(prefix + "&encoding=hex&data=0+1", command));
		assert(!form(prefix + "&encoding=hex&data=0G", command));
		assert(!form(prefix + "&encoding=text&data=" + std::string(65, 'a'), command));
		assert(form(prefix + "&encoding=text&data=" + std::string(64, 'a'), command));
		assert(!form(prefix + "&encoding=hex&data=", command));
	}
	const std::string can = "action=send&bus=CAN&encoding=hex&";
	assert(form(can + "id=7FF&data=" + std::string(16, 'F'), command));
	assert(command.event.dlc == 8);
	assert(!form(can + "id=800&data=FF", command));
	assert(!form(can + "id=0x123&data=FF", command));
	assert(form(can + "id=1FFFFFFF&extended=1&data=", command));
	assert(!form(can + "id=20000000&extended=1&data=", command));
	assert(!form(can + "id=123&data=" + std::string(18, 'F'), command));
	assert(form(can + "id=123&rtr=1&data=&dlc=8", command));
	assert(command.event.remote && command.event.dlc == 8 && !command.event.length);
	assert(!form(can + "id=123&rtr=1&data=01&dlc=8", command));
	assert(!form(can + "id=123&rtr=1&data=&dlc=9", command));
	assert(!form(can + "id=123&rtr=1&data=", command));
	assert(!form("action=send&bus=invalid&encoding=hex&data=01", command));
	assert(form("action=rate&bus=CAN&rate=250000", command));
	assert(command.rate == 250000 && command.event.source == Source::Can);
	assert(!form("action=rate&bus=CAN&rate=9600", command));
	assert(form("action=rate&bus=RS232&rate=115200", command));
	assert(!form("action=rate&bus=RS232&rate=115201", command));
}

static void stateTests() {
	ControlInput input;
	assert(input.set(ActionField, "arm", 3));
	assert(!input.set(ActionField, "arm\0send", 8));
	assert(!input.set(DataField, "A\0B", 3));
	assert(!input.set(ControlFieldCount, "arm", 3));
	for (const auto action : {Action::Arm, Action::Disarm, Action::Send, Action::Scan, Action::Rate}) {
		for (unsigned mask = 0; mask < 8; ++mask) {
			const bool busy = mask & 1, scanning = mask & 2, armed = mask & 4;
			const bool expected = !busy && !scanning &&
				(action != Action::Send || armed) && (action != Action::Scan || !armed);
			assert(controlAllowed(action, busy, scanning, armed) == expected);
		}
	}
	uint64_t cursor;
	assert(parseCursor("999999999999999", cursor) && cursor == 999999999999999ULL);
	assert(parseCursor("", cursor) && cursor == 0);
	assert(!parseCursor("1000000000000000", cursor));
	assert(!parseCursor("-1", cursor));
	assert(!parseCursor("1x", cursor));
}

static void frameTests() {
	assert(usbCrc("123456789", 9) == 0xCBF43926);
	// Independently generated with Python's zlib.crc32.
	char golden[] = "TL1 0123456789abcdef events 0 *18aab641";
	char* id;
	char* payload;
	assert(parseUsbEnvelope(golden, id, payload));
	assert(strcmp(id, "0123456789abcdef") == 0 && strcmp(payload, "events 0") == 0);
	for (const auto* bad : {
		"TL1 0123456789abcdef events 1 *18aab641",
		"TL1 0123456789abcdef events 0 *18aab64",
		"TL1 0123456789abcdeg events 0 *18aab641", "action=arm", ""
	}) {
		char buffer[128];
		strcpy(buffer, bad);
		assert(!parseUsbEnvelope(buffer, id, payload));
	}
}

static void lineTests() {
	UsbLine line;
	const std::string frame = "TL1 0123456789abcdef events 0 *18aab641";
	for (char byte : frame) assert(!line.push(byte, 0));
	assert(line.push('\n', 1) && strcmp(line.data(), frame.c_str()) == 0);
	for (size_t i = 0; i < usbRequestLimit; ++i) assert(!line.push('A', 2));
	assert(line.push('\n', 3) && strlen(line.data()) == usbRequestLimit);
	for (size_t i = 0; i <= usbRequestLimit; ++i) assert(!line.push('A', 4));
	for (char byte : frame) assert(!line.push(byte, 4));
	assert(!line.push('\n', 5));
	for (char byte : frame) line.push(byte, 10);
	line.expire(1010);
	assert(!line.push('\n', 1011));
	for (char bad : {'\0', '\r', static_cast<char>(0xFF)}) {
		for (char byte : frame) line.push(byte, 1100);
		line.push(bad, 1100);
		assert(!line.push('\n', 1101));
	}
	for (char byte : frame) line.push(byte, UINT32_MAX - 500);
	line.expire(499); // millis() wraparound still expires after 1000 ms.
	assert(!line.push('\n', 500));
	for (char byte : frame) line.push(byte, 2000);
	assert(line.push('\n', 2001));
}

int main() {
	controlTests();
	stateTests();
	frameTests();
	lineTests();
	puts("Shared control validation and USB protocol tests passed.");
}
