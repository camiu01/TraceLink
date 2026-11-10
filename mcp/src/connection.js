/**
 * @file connection.js
 * @brief Explicit HTTP or named USB serial selection, without probing hardware.
 */
import { TraceLinkBridge } from './bridge.js';
import { SerialTransport } from './serial.js';

export async function createBridge(env = process.env, makePort) {
	const allowTx = env.TRACELINK_ALLOW_TX;
	if (allowTx !== undefined && !['0', '1'].includes(allowTx))
		throw new Error('TRACELINK_ALLOW_TX must be 0 or 1.');
	const path = env.TRACELINK_SERIAL_PORT;
	if (path === undefined)
		return new TraceLinkBridge({ baseUrl: env.TRACELINK_URL, allowTx: allowTx === '1' });
	if (!path.trim() || env.TRACELINK_URL !== undefined)
		throw new Error('Set TRACELINK_SERIAL_PORT to one exact port and remove TRACELINK_URL.');
	if (!makePort) {
		const { SerialPort } = await import('serialport');
		makePort = options => new SerialPort(options);
	}
	const port = makePort({ path, baudRate: 115200, autoOpen: false, lock: true, hupcl: false });
	return new TraceLinkBridge({ allowTx: allowTx === '1', transport: new SerialTransport(port) });
}
