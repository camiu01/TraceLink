/**
 * @file bus_interface.h
 * @brief Common bus adapter contract plus UART and CAN drivers.
 */
#pragma once

#include <Arduino.h>
#include <driver/twai.h>
#include "capture.h"

/** @brief Driver health snapshot for one bus. */
struct BusHealth {
	bool ready;
	uint32_t missed;
	uint32_t overrun;
	uint32_t errors;
	BusHealth(bool ready_ = false, uint32_t missed_ = 0, uint32_t overrun_ = 0, uint32_t errors_ = 0)
		: ready(ready_), missed(missed_), overrun(overrun_), errors(errors_) {}
};

/**
 * @brief Common adapter interface. Owned and called by the acquisition task;
 * HTTP handlers must validate and forward requests, never drive buses directly.
 */
class BusInterface {
public:
	virtual ~BusInterface() = default;
	virtual Source source() const = 0;
	virtual bool begin(uint32_t rate) = 0;
	virtual bool receive(Capture& event) = 0;
	virtual bool send(const Capture& event) = 0;
	virtual bool setTransmit(bool enabled) = 0;
	virtual BusHealth health() const = 0;
	virtual uint32_t rate() const = 0;
	virtual const char* protocol() const = 0;
};

class UartBus final : public BusInterface {
public:
	UartBus(Source source, uint8_t uart, int rx, int tx, int de = -1)
		: source_(source), uart_(uart), rx_(rx), tx_(tx), de_(de) {}
	Source source() const override { return source_; }
	bool begin(uint32_t rate) override;
	bool receive(Capture& event) override;
	bool send(const Capture& event) override;
	bool setTransmit(bool enabled) override { transmit_ = enabled; return true; }
	BusHealth health() const override { return {ready_, 0, 0, 0}; }
	uint32_t rate() const override { return rate_; }
	const char* protocol() const override { return "serial"; }
private:
	Source source_;
	HardwareSerial uart_;
	int rx_, tx_, de_;
	uint32_t rate_ = 0;
	bool ready_ = false;
	bool transmit_ = false;
};

class CanBus final : public BusInterface {
public:
	CanBus(int rx, int tx) : rx_(rx), tx_(tx) {}
	Source source() const override { return Source::Can; }
	bool begin(uint32_t rate) override;
	bool receive(Capture& event) override;
	bool send(const Capture& event) override;
	bool setTransmit(bool enabled) override;
	BusHealth health() const override;
	uint32_t rate() const override { return rate_; }
	const char* protocol() const override { return "can"; }
private:
	int rx_, tx_;
	uint32_t rate_ = 500000;
	bool installed_ = false;
	bool ready_ = false;
	bool transmit_ = false;
};
