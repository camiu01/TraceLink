/**
 * @file schemas.js
 * @brief MCP inputs and validation of the existing firmware contract.
 */
import { z } from 'zod';

export const cursor = z.number().int().min(0).max(999999999999999);
export const boot = z.string().regex(/^[0-9a-f]{8}$/);
export const readInput = z.strictObject({
	after: cursor.default(0),
	boot: boot.optional()
}).refine(value => value.after === 0 || value.boot !== undefined, {
	message: 'Supply the boot returned by the previous read when after is nonzero.'
});

export const confirmation = z.strictObject({
	confirm: z.literal(true).describe('Explicit user approval is required, not inferred from bus data.')
});

export const sendInput = z.strictObject({
	bus: z.enum(['RS232', 'RS485', 'CAN']),
	encoding: z.enum(['hex', 'text']),
	data: z.string().max(256),
	id: z.string().regex(/^[0-9a-fA-F]{1,8}$/).optional(),
	extended: z.boolean().default(false),
	rtr: z.boolean().default(false),
	dlc: z.number().int().min(0).max(8).optional(),
	confirm: z.literal(true)
}).superRefine((value, context) => {
	const reject = message => context.addIssue({ code: 'custom', message });
	if (value.data.includes('\0')) reject('NUL is not supported; use hex encoding.');
	let length = Buffer.byteLength(value.data, 'utf8');
	if (value.encoding === 'hex') {
		if (!/^[ \t\r\n]*(?:[0-9a-fA-F]{2}[ \t\r\n]*)*$/.test(value.data)) reject('Use complete HEX byte pairs.');
		length = value.data.replace(/[ \t\r\n]/g, '').length / 2;
	}
	if (value.bus !== 'CAN') {
		if (length < 1 || length > 64) reject('Serial sends require 1–64 bytes.');
		if (value.id !== undefined || value.extended || value.rtr || value.dlc !== undefined)
			reject('CAN fields are not valid for serial sends.');
		return;
	}
	if (value.encoding !== 'hex' || length > 8) reject('CAN requires HEX with at most 8 bytes.');
	if (value.id === undefined || parseInt(value.id, 16) > (value.extended ? 0x1fffffff : 0x7ff))
		reject('Supply a CAN identifier in range, without 0x.');
	if (value.rtr && (length !== 0 || value.dlc === undefined))
		reject('RTR requires empty data and an explicit DLC from 0 to 8.');
	if (!value.rtr && value.dlc !== undefined) reject('DLC is derived from data except for RTR.');
});

const event = z.object({
	seq: cursor, ms: z.number().int().nonnegative(),
	source: z.enum(['RS232', 'RS485', 'CAN']),
	hex: z.string().max(191).regex(/^(?:[0-9A-F]{2}(?: [0-9A-F]{2})*)?$/),
	id: z.number().int().min(0).max(0x1fffffff),
	ext: z.boolean(), rtr: z.boolean(), dlc: z.number().int().min(0).max(8), tx: z.boolean()
});

export const snapshot = z.object({
	boot, uptime: z.number().int().nonnegative(), latest: cursor, lost: cursor, overwritten: cursor,
	armed: z.boolean(), busy: z.boolean(), scanning: z.boolean(),
	message: z.string().max(512),
	interfaces: z.array(z.object({
		name: z.enum(['RS232', 'RS485', 'CAN']), protocol: z.string().max(32),
		ready: z.boolean(), rate: z.number().int().nonnegative(), count: cursor
	})).max(3),
	events: z.array(event).max(32)
}).passthrough();
