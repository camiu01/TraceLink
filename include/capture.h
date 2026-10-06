/**
 * @file capture.h
 * @brief Capture record, source names, ring history, and HEX formatting.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief Bus sources. Add new sources before Count and register an adapter. */
enum class Source : uint8_t { Rs232, Rs485, Can, Count };
constexpr size_t sourceCount = static_cast<size_t>(Source::Count);

struct Capture {
	uint64_t sequence = 0;
	uint32_t timestamp = 0;
	Source source = Source::Rs232;
	uint32_t identifier = 0;
	bool extended = false;
	bool remote = false;
	bool transmitted = false;
	uint8_t dlc = 0;
	uint8_t length = 0;
	uint8_t data[64] = {};
};

/**
 * @brief Short display name of a bus source.
 * @param source Source to name.
 * @return Pointer to a static name string.
 */
inline const char* sourceName(Source source) {
	switch (source) {
		case Source::Rs232: return "RS232";
		case Source::Rs485: return "RS485";
		case Source::Can: return "CAN";
		case Source::Count: break;
	}
	return "?";
}

/**
 * @brief Fixed-capacity ring history with per-source counters.
 * @note External synchronization is required when shared across FreeRTOS tasks.
 */
template <size_t Capacity>
class CaptureHistory {
	static_assert(Capacity > 0, "History must have at least one slot");
public:
	/**
	 * @brief Store an event, assigning the next sequence number.
	 * @param event Event to store (sequence is overwritten).
	 */
	void push(Capture event) {
		event.sequence = ++latest_;
		events_[(latest_ - 1) % Capacity] = event;
		++counts_[static_cast<size_t>(event.source)];
	}

	/**
	 * @brief Read events after a sequence number.
	 * @param after Last sequence already seen.
	 * @param output Destination buffer.
	 * @param limit Maximum events to copy.
	 * @param lost Set to the number of skipped (overwritten) events.
	 * @return Number of events copied.
	 */
	size_t read(uint64_t after, Capture* output, size_t limit,
				uint64_t& lost) const {
		lost = 0;
		if (after >= latest_ || limit == 0) return 0;
		const uint64_t oldest = latest_ > Capacity ? latest_ - Capacity + 1 : 1;
		uint64_t next = after + 1;
		if (next < oldest) {
			lost = oldest - next;
			next = oldest;
		}
		size_t size = 0;
		while (next <= latest_ && size < limit) {
			output[size++] = events_[(next - 1) % Capacity];
			++next;
		}
		return size;
	}

	uint64_t latest() const { return latest_; }
	uint64_t count(Source source) const { return counts_[static_cast<size_t>(source)]; }
	uint64_t overwritten() const { return latest_ > Capacity ? latest_ - Capacity : 0; }

private:
	Capture events_[Capacity] = {};
	uint64_t latest_ = 0;
	uint64_t counts_[sourceCount] = {};
};

/**
 * @brief Format event bytes as space-separated uppercase HEX.
 * @param event Event to format.
 * @param output Destination buffer (at least 192 bytes).
 */
inline void formatHex(const Capture& event, char* output) {
	constexpr char digits[] = "0123456789ABCDEF";
	size_t cursor = 0;
	for (size_t i = 0; i < event.length; ++i) {
		if (i != 0) output[cursor++] = ' ';
		output[cursor++] = digits[event.data[i] >> 4];
		output[cursor++] = digits[event.data[i] & 15];
	}
	output[cursor] = '\0'; // Caller provides at least 192 bytes.
}
