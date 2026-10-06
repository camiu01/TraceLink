/**
 * @file history_test.cpp
 * @brief Host-side checks for history, HEX, parsers, rates, and baud estimate.
 */
#include <assert.h>
#include <string.h>
#include "capture.h"
#include "commands.h"

int main() {
	CaptureHistory<3> history;
	Capture output[3];
	uint64_t lost = 99;
	assert(history.read(0, output, 3, lost) == 0 && lost == 0);
	Capture event;
	event.source = Source::Rs485;
	event.length = 3;
	event.data[0] = 0;
	event.data[1] = 0xAF;
	event.data[2] = 0xFF;
	char hex[192];
	formatHex(event, hex);
	assert(strcmp(hex, "00 AF FF") == 0);
	event.length = 0;
	formatHex(event, hex);
	assert(strcmp(hex, "") == 0);
	for (int i = 0; i < 5; ++i) history.push(event);
	assert(history.latest() == 5);
	assert(history.count(Source::Rs485) == 5);
	assert(history.count(Source::Can) == 0);
	assert(history.overwritten() == 2);
	assert(history.read(0, output, 2, lost) == 2 && lost == 2);
	assert(output[0].sequence == 3 && output[1].sequence == 4);
	assert(history.read(4, output, 3, lost) == 1 && lost == 0);
	assert(output[0].sequence == 5);
	assert(history.read(5, output, 3, lost) == 0 && lost == 0);
	assert(history.read(UINT64_MAX, output, 3, lost) == 0 && lost == 0);
	assert(history.read(0, output, 0, lost) == 0 && lost == 0);
	CaptureHistory<1> single;
	single.push(event);
	single.push(event);
	assert(single.read(0, output, 3, lost) == 1 && lost == 1);
	event.length = 64;
	memset(event.data, 0xAB, sizeof(event.data));
	formatHex(event, hex);
	assert(strlen(hex) == 191);
	assert(strcmp(sourceName(Source::Can), "CAN") == 0);
	uint8_t bytes[64], length = 0;
	assert(parseHex("01 03 FF", bytes, 64, length) && length == 3 && bytes[2] == 255);
	assert(parseHex("0103ff", bytes, 64, length) && length == 3);
	assert(parseHex("", bytes, 64, length) && length == 0);
	assert(!parseHex("0 1", bytes, 64, length));
	assert(!parseHex("0G", bytes, 64, length));
	assert(!parseHex("123", bytes, 64, length));
	assert(!parseHex("00 01", bytes, 1, length));
	uint32_t number;
	assert(parseUnsigned("115200", 1000000, number) && number == 115200);
	assert(parseUnsigned("7FF", 0x7FF, number, 16) && number == 0x7FF);
	assert(!parseUnsigned("800", 0x7FF, number, 16));
	assert(!parseUnsigned("4294967296", UINT32_MAX, number));
	assert(!parseUnsigned("-1", UINT32_MAX, number));
	assert(!parseUnsigned("", UINT32_MAX, number));
	assert(!parseUnsigned("0x123", 0x7FF, number, 16));
	assert(supportedRate(Source::Can, 250000));
	assert(!supportedRate(Source::Can, 9600));
	assert(supportedRate(Source::Rs232, 115200));
	PulseHistogram pulses;
	assert(estimateBaud(pulses) == 0);
	pulses.edges = 50;
	pulses.bins[104] = 20;
	pulses.bins[208] = 10;
	assert(estimateBaud(pulses) == 9600);
	pulses.bins[9] = 1; // Isolated short glitch must not suggest 115200.
	assert(estimateBaud(pulses) == 9600);
	pulses.bins[9] = 6;
	assert(estimateBaud(pulses) == 115200);
}
