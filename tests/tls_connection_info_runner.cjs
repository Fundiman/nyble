// Runs test_tls_connection_info.mjs against a local TLS server so the mbedTLS
// backend (which has no server engine) can exercise client-side connection info.
const fs = require('node:fs');
const path = require('node:path');
const tls = require('node:tls');
const { spawn } = require('node:child_process');

const root = path.resolve(__dirname, '..');
const fixture = name => fs.readFileSync(path.join(root, 'tests/fixtures/tls', name));

const server = tls.createServer({
  key: fixture('server-key.pem'),
  // Send the full chain so getPeerCertificate(true) can be exercised.
  cert: Buffer.concat([fixture('server-cert.pem'), fixture('ca-cert.pem')]),
}, socket => {
  socket.on('error', () => {});
  socket.end('ok');
});

server.on('tlsClientError', () => {});

server.listen(0, '127.0.0.1', () => {
  const child = spawn(path.join(root, 'build-embed-zig', 'ant.exe'), [
    'tests/test_tls_connection_info.mjs',
  ], {
    cwd: root,
    stdio: 'inherit',
    env: { ...process.env, TLS_TEST_PORT: String(server.address().port) },
  });
  child.on('exit', code => server.close(() => process.exit(code == null ? 1 : code)));
});
