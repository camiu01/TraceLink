/**
 * @file bus_interface.cpp
 * @brief UART/RS485 and TWAI CAN adapter implementation.
 */
#include "bus_interface.h"
#include "config.h"
#include "commands.h"

bool UartBus::begin(uint32_t rate) {
	if (!supportedRate(source_, rate)) return false;
	if (de_ >= 0) {
		pinMode(de_, OUTPUT);
		digitalWrite(de_, LOW);
	}
	if (ready_) uart_.end();
	uart_.setRxBufferSize(config::uartRxBufferSize);
	uart_.begin(rate, config::serialFormat, rx_, tx_);
	rate_ = rate;
	ready_ = true;
	return true;
}

bool UartBus::receive(Capture& event) {
	event = Capture{};
	event.source = source_;
	while (event.length < sizeof(event.data) && uart_.available() > 0) {
		const int byte = uart_.read();
		if (byte < 0) break;
		event.data[event.length++] = static_cast<uint8_t>(byte);
	}
	return event.length > 0;
}

bool UartBus::send(const Capture& event) {
	if (!ready_ || !transmit_ || event.length == 0) return false;
	if (de_ >= 0) {
		digitalWrite(de_, HIGH); // /RE HIGH suppresses local echo.
		delayMicroseconds(10);
	}
	const size_t written = uart_.write(event.data, event.length);
	uart_.flush(true); // Wait for physical TX completion; do not discard RX.
	if (de_ >= 0) {
		delayMicroseconds(10);
		digitalWrite(de_, LOW);
	}
	return written == event.length;
}

bool CanBus::begin(uint32_t rate) {
	if (!supportedRate(Source::Can, rate)) return false;
	if (installed_) {
		if (ready_) twai_stop();
		twai_driver_uninstall();
		ready_ = false;
		installed_ = false;
	}
	rate_ = rate;
	twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
		static_cast<gpio_num_t>(tx_), static_cast<gpio_num_t>(rx_),
		transmit_ ? TWAI_MODE_NORMAL : TWAI_MODE_LISTEN_ONLY);
	general.tx_queue_len = transmit_ ? 4 : 0;
	general.rx_queue_len = 64;
	twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
	if (rate == 125000) timing = TWAI_TIMING_CONFIG_125KBITS();
	if (rate == 250000) timing = TWAI_TIMING_CONFIG_250KBITS();
	if (rate == 1000000) timing = TWAI_TIMING_CONFIG_1MBITS();
	twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
	const esp_err_t result = twai_driver_install(&general, &timing, &filter);
	if (result != ESP_OK) {
		Serial.printf("CAN install: %s\n", esp_err_to_name(result));
		return false;
	}
	installed_ = true;
	ready_ = twai_start() == ESP_OK;
	return ready_;
}

bool CanBus::receive(Capture& event) {
	twai_message_t message;
	if (!ready_ || twai_receive(&message, 0) != ESP_OK) return false;
	event = Capture{};
	event.source = Source::Can;
	event.identifier = message.identifier;
	event.extended = message.extd;
	event.remote = message.rtr;
	event.dlc = message.data_length_code;
	event.length = event.remote ? 0 : min(message.data_length_code, uint8_t(8));
	memcpy(event.data, message.data, event.length);
	return true;
}

bool CanBus::send(const Capture& event) {
	if (!ready_ || !transmit_) return false;
	twai_message_t message = {};
	message.identifier = event.identifier;
	message.extd = event.extended;
	message.rtr = event.remote;
	message.ss = 1; // Single-shot: no indefinite retry on missing ACK.
	message.data_length_code = event.dlc;
	memcpy(message.data, event.data, event.length);
	return twai_transmit(&message, pdMS_TO_TICKS(50)) == ESP_OK;
}

bool CanBus::setTransmit(bool enabled) {
	if (transmit_ == enabled && ready_) return true;
	transmit_ = enabled;
	return begin(rate_);
}

BusHealth CanBus::health() const {
	twai_status_info_t status = {};
	if (!ready_ || twai_get_status_info(&status) != ESP_OK) return {false, 0, 0, 0};
	return {status.state == TWAI_STATE_RUNNING, status.rx_missed_count,
			status.rx_overrun_count, status.bus_error_count};
}
