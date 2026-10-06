/**
 * @file commands.h
 * @brief HEX/number parsers, supported rates, and serial baud estimation.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "capture.h"

/**
 * @brief Value of one hexadecimal digit.
 * @param value Character to convert.
 * @return Digit value 0-15, or -1 when invalid.
 */
inline int hexDigit(char value) {
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

/**
 * @brief Strict HEX parser; separators allowed only between complete bytes.
 * @param text Input text such as "01 03 FF".
 * @param bytes Destination buffer.
 * @param capacity Destination capacity.
 * @param length Set to the parsed byte count.
 * @return True on success, false on malformed input or overflow.
 */
inline bool parseHex(const char* text, uint8_t* bytes, size_t capacity, uint8_t& length) {
	length = 0;
	int high = -1;
	for (; *text; ++text) {
		if (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
			if (high >= 0) return false;
			continue;
		}
		const int digit = hexDigit(*text);
		if (digit < 0) return false;
		if (high < 0) high = digit;
		else {
			if (length >= capacity) return false;
			bytes[length++] = static_cast<uint8_t>(high * 16 + digit);
			high = -1;
		}
	}
	return high < 0;
}

/**
 * @brief Overflow-checked unsigned integer parser.
 * @param text Input digits.
 * @param maximum Maximum accepted value.
 * @param value Set to the parsed value.
 * @param base Digit base (10 or 16).
 * @return True on success, false on empty, invalid, or overflowing input.
 */
inline bool parseUnsigned(const char* text, uint32_t maximum, uint32_t& value, unsigned base = 10) {
	value = 0;
	if (!*text) return false;
	for (; *text; ++text) {
		const int digit = hexDigit(*text);
		if (digit < 0 || static_cast<unsigned>(digit) >= base ||
			static_cast<uint32_t>(digit) > maximum ||
			value > (maximum - static_cast<uint32_t>(digit)) / base) return false;
		value = value * base + static_cast<unsigned>(digit);
	}
	return true;
}

constexpr uint32_t serialRates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
constexpr uint32_t canRates[] = {500000, 250000, 125000, 1000000};

/**
 * @brief Check a rate against the supported serial/CAN tables.
 * @param source Bus source selecting the table.
 * @param rate Rate to check.
 * @return True when supported.
 */
inline bool supportedRate(Source source, uint32_t rate) {
	if (source == Source::Can) {
		for (uint32_t candidate : canRates) if (candidate == rate) return true;
	} else {
		for (uint32_t candidate : serialRates) if (candidate == rate) return true;
	}
	return false;
}

struct PulseHistogram {
	uint16_t bins[1001] = {};
	uint32_t edges = 0;
	uint32_t previous = 0;
	bool seen = false;
};

// Highest candidate with at least six plausible one-bit pulses.
// Timing estimate only: data patterns can produce ambiguous results.
/**
 * @brief Estimate serial baud from an RX pulse histogram.
 * @param pulses Collected edge-interval histogram.
 * @return Estimated baud, or 0 when inconclusive.
 */
inline uint32_t estimateBaud(const PulseHistogram& pulses) {
	if (pulses.edges < 20) return 0;
	for (size_t i = sizeof(serialRates) / sizeof(serialRates[0]); i > 0; --i) {
		const uint32_t bit = (1000000 + serialRates[i - 1] / 2) / serialRates[i - 1];
		const uint32_t tolerance = bit / 6 + 1;
		uint32_t votes = 0;
		for (uint32_t bin = bit - tolerance; bin <= bit + tolerance && bin <= 1000; ++bin) {
			votes += pulses.bins[bin];
		}
		if (votes >= 6) return serialRates[i - 1];
	}
	return 0;
}
