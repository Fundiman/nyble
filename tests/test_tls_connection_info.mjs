import assert from 'node:assert';
import fs from 'node:fs';
import tls from 'node:tls';

const ca = fs.readFileSync('tests/fixtures/tls/ca-cert.pem');
const port = Number(process.env.TLS_TEST_PORT);
assert(Number.isInteger(port) && port > 0, 'TLS_TEST_PORT must be set by the test runner');
const target = { host: '127.0.0.1', servername: 'localhost', port };

const once = (emitter, event) => new Promise(resolve => emitter.once(event, (...args) => resolve(args)));
const readAll = socket => new Promise(resolve => {
  let text = '';
  socket.on('data', chunk => (text += chunk));
  socket.on('close', () => resolve(text));
});

// A verified connection exposes the negotiated version, cipher, session and chain.
const verified = tls.connect({ ...target, ca });
await once(verified, 'secureConnect');
// TLS 1.3 session tickets arrive just after the handshake completes.
await new Promise(resolve => setTimeout(resolve, 100));
assert.match(verified.getProtocol(), /^TLSv1\.[23]$/);

const cipher = verified.getCipher();
assert.strictEqual(typeof cipher.name, 'string');
assert.ok(cipher.name.length > 0);
assert.match(cipher.version, /^TLSv1\.[23]$/);

const session = verified.getSession();
assert.ok(session && session.length > 0, 'getSession() should return serialized session bytes');

const leaf = verified.getPeerCertificate();
assert.strictEqual(typeof leaf.subject, 'string');
assert.ok(leaf.subject.length > 0);
assert.ok(leaf.raw && leaf.raw.length > 0);
assert.match(leaf.fingerprint256, /^[0-9a-f]{64}$/);
assert.strictEqual(typeof leaf.serialNumber, 'string');
assert.strictEqual(typeof leaf.valid_to, 'string');

const detailed = verified.getPeerCertificate(true);
assert.strictEqual(typeof detailed.issuerCertificate.subject, 'string');
assert.strictEqual(verified.authorized, true);
assert.strictEqual(verified.authorizationError, null);
assert.strictEqual(await readAll(verified), 'ok');

// rejectUnauthorized:false is per-connection: it connects, stays unauthorized,
// and must not mutate verification for later sockets on the default context.
const lax = tls.connect({ ...target, rejectUnauthorized: false });
await once(lax, 'secureConnect');
assert.strictEqual(lax.authorized, false);
assert.strictEqual(typeof lax.authorizationError, 'string');
assert.strictEqual(await readAll(lax), 'ok');

const strict = tls.connect(target);
const [error] = await once(strict, 'error');
assert.ok(error, 'untrusted server must still fail after a lax connection');

console.log('tls-connection-info:ok');
