/**
 * @file usb_protocol.h
 * @brief Bounded USB line framing, CRC32 and strict form decoding, testable on PC.
 */
#pragma once
#include "control_request.h"

constexpr size_t usbRequestLimit = 1024;

inline uint32_t usbCrc(const char* data, size_t length) {
	uint32_t crc = 0xFFFFFFFF;
	for (size_t i = 0; i < length; ++i) {
		crc ^= static_cast<uint8_t>(data[i]);
		for (unsigned bit = 0; bit < 8; ++bit)
			crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
	}
	return ~crc;
}

inline bool parseCursor(const char* text, uint64_t& value) {
	value = 0;
	if (strlen(text) > 15) return false;
	for (; *text; ++text) {
		if (*text < '0' || *text > '9') return false;
		value = value * 10 + (*text - '0');
	}
	return true;
}

/** @brief Discard an entire oversized, invalid or stale line, never execute a suffix. */
class UsbLine {
public:
	void expire(uint32_t now) {
		if (size_ && now - updated_ >= 1000) { size_ = 0; discard_ = true; }
	}
	bool push(char byte, uint32_t now) {
		expire(now);
		updated_ = now;
		if (byte == '\n') {
			const bool ready = !discard_ && size_ != 0;
			buffer_[size_] = '\0';
			size_ = 0;
			discard_ = false;
			return ready;
		}
		if (discard_) return false;
		const auto code = static_cast<unsigned char>(byte);
		if (code < 32 || code > 126 || size_ == usbRequestLimit) {
			size_ = 0;
			discard_ = true;
		} else buffer_[size_++] = byte;
		return false;
	}
	char* data() { return buffer_; }
private:
	char buffer_[usbRequestLimit + 1] = {};
	size_t size_ = 0;
	uint32_t updated_ = 0;
	bool discard_ = false;
};

/** @brief Validate checksum before exposing payload; mutates only a complete line. */
inline bool parseUsbEnvelope(char* line, char*& id, char*& payload) {
	const size_t length = strlen(line);
	if (length < 4 + 16 + 1 + 1 + 10 || strncmp(line, "TL1 ", 4) != 0 || line[20] != ' ')
		return false;
	for (size_t i = 4; i < 20; ++i) if (hexDigit(line[i]) < 0) return false;
	char* trailer = line + length - 10;
	uint32_t crc;
	if (trailer[0] != ' ' || trailer[1] != '*' || strlen(trailer + 2) != 8 ||
		!parseUnsigned(trailer + 2, UINT32_MAX, crc, 16) ||
		crc != usbCrc(line, length - 10)) return false;
	line[20] = '\0';
	*trailer = '\0';
	id = line + 4;
	payload = line + 21;
	return true;
}

inline bool decodeFormComponent(char* value) {
	char* output = value;
	for (char* input = value; *input; ++input) {
		char byte = *input;
		if (byte == '+') byte = ' ';
		else if (byte == '%') {
			if (!input[1] || !input[2] || hexDigit(input[1]) < 0 || hexDigit(input[2]) < 0)
				return false;
			byte = static_cast<char>((hexDigit(input[1]) << 4) | hexDigit(input[2]));
			input += 2;
			if (byte == '\0') return false;
		}
		*output++ = byte;
	}
	*output = '\0';
	return true;
}

inline bool parseControlForm(char* form, ControlInput& result) {
	bool seen[ControlFieldCount] = {};
	while (*form) {
		char* next = strchr(form, '&');
		if (next) *next++ = '\0';
		char* value = strchr(form, '=');
		if (!value) return false;
		*value++ = '\0';
		if (!decodeFormComponent(form) || !decodeFormComponent(value)) return false;
		size_t key = 0;
		for (; key < ControlFieldCount; ++key) if (strcmp(form, controlKeys[key]) == 0) break;
		if (key == ControlFieldCount || seen[key]) return false;
		seen[key] = true;
		result.values[key] = value;
		if (!next) break;
		if (!*next) return false;
		form = next;
	}
	return seen[ActionField];
}
