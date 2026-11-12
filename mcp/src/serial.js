/**
 * @file serial.js
 * @brief Framed USB/COM transport. Never discovers ports, reconnects or repeats writes.
 */
import { randomBytes } from 'node:crypto';
import { setTimeout as delay } from 'node:timers/promises';

export function crc32(text) {
	let crc = 0xffffffff;
	for (const byte of Buffer.from(text)) {
		crc ^= byte;
		for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return ((~crc) >>> 0).toString(16).padStart(8, '0');
}

/** @brief Port is injected so the same serial stream can run against a mock binding. */
export class SerialTransport {
	#pending;
	#opening;
	#failed;
	#busy = false;
	#closed = false;
	#buffer = Buffer.alloc(65536);
	#length = 0;
	#discard = false;

	constructor(port, { timeoutMs = 3000, startupDelayMs = 2000 } = {}) {
		this.port = port;
		this.timeoutMs = timeoutMs;
		this.startupDelayMs = startupDelayMs;
		port.on('data', chunk => this.#consume(chunk));
		port.on('error', error => this.#fail(error));
		port.on('close', () => this.#fail(new Error('Serial port disconnected. Restart the bridge to reconnect.')));
	}

	#fail(error) {
		this.#failed = error;
		this.#pending?.finish(error);
	}

	async #open() {
		if (this.#failed) throw this.#failed;
		if (this.#closed) throw new Error('Serial transport is closed.');
		if (!this.#opening) {
			this.#opening = new Promise((resolve, reject) => {
				this.port.open(error => error ? reject(error) : resolve());
			}).then(() => delay(this.startupDelayMs));
		}
		await this.#opening;
		if (this.#failed || this.#closed) throw this.#failed ?? new Error('Serial transport is closed.');
	}

	#consume(chunk) {
		for (const byte of chunk) {
			if (byte === 10) {
				if (!this.#discard) this.#line(this.#buffer.subarray(0, this.#length).toString('utf8').replace(/\r$/, ''));
				this.#length = 0;
				this.#discard = false;
			} else if (!this.#discard) {
				if (this.#length === this.#buffer.length) {
					this.#discard = true;
					this.#pending?.finish(new Error('Serial response exceeds 64 KiB.'));
				} else this.#buffer[this.#length++] = byte;
			}
		}
	}

	#line(line) {
		const pending = this.#pending;
		if (!pending || !line.startsWith(`TL1 ${pending.id} `)) return;
		const match = line.match(/^TL1 [0-9a-f]{16} ([0-9]{3}) (.*)$/);
		if (!match) { pending.finish(new Error('Malformed serial response.')); return; }
		const status = Number(match[1]);
		try {
			if (pending.mutating ? status !== 202 : status !== 200)
				throw new Error(`TraceLink serial ${status}.`);
			const result = pending.mutating
				? { accepted: true, deliveryConfirmed: false, message: match[2] } : JSON.parse(match[2]);
			pending.finish(null, result);
		} catch (error) { pending.finish(error); }
	}

	async request(path, options = {}) {
		if (this.#busy) throw new Error('Another serial request is in progress.');
		this.#busy = true;
		try {
			await this.#open();
			const mutating = options.method === 'POST';
			let payload;
			if (mutating && path === 'api/control')
				payload = `control ${options.headers['X-TraceLink']} ${options.body}`;
			else if (!mutating && /^api\/events\?after=\d{1,15}$/.test(path))
				payload = `events ${path.split('=')[1]}`;
			else throw new Error('Unsupported serial operation.');
			const id = randomBytes(8).toString('hex');
			const message = `TL1 ${id} ${payload}`;
			const frame = `${message} *${crc32(message)}`;
			if (Buffer.byteLength(frame) > 1024) throw new Error('Serial request exceeds 1024 bytes.');
			return await this.#exchange(id, `${frame}\n`, mutating);
		} finally { this.#busy = false; }
	}

	#exchange(id, frame, mutating) {
		return new Promise((resolve, reject) => {
			const timer = setTimeout(() => finish(new Error('Serial response timed out. No retry was attempted.')), this.timeoutMs);
			const finish = (error, result) => {
				if (this.#pending?.id !== id) return;
				clearTimeout(timer);
				this.#pending = undefined;
				error ? reject(error) : resolve(result);
			};
			this.#pending = { id, mutating, finish };
			try {
				this.port.write(frame, error => { if (error) this.#fail(error); });
			} catch (error) { this.#fail(error); }
		});
	}

	async close() {
		this.#closed = true;
		this.#pending?.finish(new Error('Serial transport closed. Control outcome may be unknown.'));
		if (this.#opening) await this.#opening.catch(() => {});
		if (this.port.isOpen) await new Promise((resolve, reject) => {
			this.port.close(error => error ? reject(error) : resolve());
		});
	}
}
