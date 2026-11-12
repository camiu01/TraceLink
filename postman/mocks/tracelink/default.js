/**
 * @file default.js
 * @brief Local-only TraceLink mock. Never connects to hardware.
 */
const http = require('node:http');
const { createMock } = require('./handler.cjs');
const fixture = createMock({ store: pm.state });
http.createServer(fixture.handler).listen(process.env.PORT || 4010);
