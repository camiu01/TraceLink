/**
 * @file config.h
 * @brief Build-time settings and GPIO map for TraceLink.
 */
#pragma once

#include <Arduino.h>

// Optional local overrides, excluded from version control.
#if __has_include("local_config.h")
#include "local_config.h"
#endif

#ifndef TRACELINK_AP_PASSWORD
#define TRACELINK_AP_PASSWORD "change-this-password"
#endif

namespace config {
constexpr char apSsid[] = "TraceLink-ESP32";
constexpr char apPassword[] = TRACELINK_AP_PASSWORD;
constexpr uint32_t usbBaud = 115200;
constexpr uint32_t rs232Baud = 9600;
constexpr uint32_t rs485Baud = 9600;
constexpr uint32_t serialFormat = SERIAL_8N1;
constexpr int rs232Rx = 16; // RX2, ESP32-WROOM-32 (not WROVER).
constexpr int rs232Tx = 17; // TX2, transmission requires explicit arming.
constexpr int rs485Rx = 26;
constexpr int rs485Tx = 27;
constexpr int rs485De = 25; // Both DE and /RE, LOW except during armed TX. Leave unconnected for auto-direction modules.
constexpr int canTx = 32;
constexpr int canRx = 33;
constexpr uint32_t canBitrate = 500000; // 125000, 250000, 500000 or 1000000.
constexpr size_t historyCapacity = 128;
constexpr size_t apiBatchSize = 32;
constexpr size_t uartRxBufferSize = 2048;
constexpr uint32_t transmitWindowMs = 60000; // Auto-disarm after 60 s without a send.
static_assert(sizeof(apPassword) >= 9 && sizeof(apPassword) <= 64,
			  "Wi-Fi AP password must contain 8 to 63 characters");
} // namespace config
