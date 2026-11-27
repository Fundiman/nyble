import { test, testDeep, summary } from './helpers.js';

console.log('Crypto Tests\n');

test('crypto exists', typeof crypto, 'object');
test('crypto toStringTag', Object.prototype.toString.call(crypto), '[object Crypto]');

const rand1 = crypto.random();
const rand2 = crypto.random();
test('random returns number', typeof rand1, 'number');
test('random returns positive', rand1 >= 0, true);
test('random returns different values', rand1 !== rand2, true);

const bytes = crypto.randomBytes(16);
test('randomBytes returns object', typeof bytes, 'object');
test('randomBytes length', bytes.length, 16);
test('randomBytes values are numbers', typeof bytes[0], 'number');
test('randomBytes values in range', bytes[0] >= 0 && bytes[0] <= 255, true);

const uuid1 = crypto.randomUUID();
const uuid2 = crypto.randomUUID();
test('randomUUID returns string', typeof uuid1, 'string');
test('randomUUID length', uuid1.length, 36);
test('randomUUID format', /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(uuid1), true);
test('randomUUID unique', uuid1 !== uuid2, true);

const uuidv7_1 = crypto.randomUUIDv7();
const uuidv7_2 = crypto.randomUUIDv7();
test('randomUUIDv7 returns string', typeof uuidv7_1, 'string');
test('randomUUIDv7 length', uuidv7_1.length, 36);
test('randomUUIDv7 version 7', uuidv7_1[14], '7');
test('randomUUIDv7 unique', uuidv7_1 !== uuidv7_2, true);
test('randomUUIDv7 monotonic', uuidv7_1 < uuidv7_2, true);

const arr = new Uint8Array(8);
const result = crypto.getRandomValues(arr);
test('getRandomValues returns same array', result === arr, true);
test(
  'getRandomValues fills array',
  Array.from(arr).some(v => v !== 0),
  true
);

const arr32 = new Uint32Array(4);
crypto.getRandomValues(arr32);
test(
  'getRandomValues works with Uint32Array',
  Array.from(arr32).some(v => v !== 0),
  true
);

test('subtle.generateKey exists', typeof crypto.subtle.generateKey, 'function');
test('subtle.exportKey exists', typeof crypto.subtle.exportKey, 'function');

async function testRejects(name, call, errorName) {
  let promise;
  try {
    promise = call();
  } catch (error) {
    test(`${name} does not throw synchronously`, error.name, 'no synchronous exception');
    return;
  }
  test(`${name} returns a Promise`, promise instanceof Promise, true);
  let rejection;
  try {
    await promise;
  } catch (error) {
    rejection = error;
  }
  test(`${name} rejection`, rejection?.name, errorName);
}

const message = new Uint8Array([1, 2, 3, 4]);
for (const length of [128, 192, 256]) {
  const algorithm = { name: 'aes-gcm', length };
  const usages = ['decrypt', 'encrypt', 'encrypt'];
  const key = await crypto.subtle.generateKey(algorithm, true, usages);
  test(`AES-${length} key type`, key.type, 'secret');
  test(`AES-${length} extractable`, key.extractable, true);
  testDeep(`AES-${length} algorithm`, key.algorithm, { name: 'AES-GCM', length });
  testDeep(`AES-${length} usages`, key.usages, ['encrypt', 'decrypt']);

  algorithm.name = 'HMAC';
  usages[0] = 'sign';
  test(`AES-${length} algorithm snapshot`, key.algorithm.name, 'AES-GCM');
  testDeep(`AES-${length} usages snapshot`, key.usages, ['encrypt', 'decrypt']);

  const raw = await crypto.subtle.exportKey('raw', key);
  test(`AES-${length} export is ArrayBuffer`, raw instanceof ArrayBuffer, true);
  test(`AES-${length} export size`, raw.byteLength, length / 8);
  const imported = await crypto.subtle.importKey('raw', raw, 'AES-GCM', false, ['decrypt']);
  test(`AES-${length} imported extractability`, imported.extractable, false);
  test(`AES-${length} imported length`, imported.algorithm.length, length);

  const params = { name: 'AES-GCM', iv: new Uint8Array(12) };
  const encrypted = await crypto.subtle.encrypt(params, key, message);
  const decrypted = await crypto.subtle.decrypt(params, imported, encrypted);
  testDeep(`AES-${length} round trip`, Array.from(new Uint8Array(decrypted)), Array.from(message));
  await testRejects(`AES-${length} non-extractable export`, () => crypto.subtle.exportKey('raw', imported), 'InvalidAccessError');
  await testRejects(`AES-${length} usage enforcement`, () => crypto.subtle.encrypt(params, imported, message), 'InvalidAccessError');
}

for (const [hash, length] of [['SHA-1', 512], ['SHA-256', 512], ['SHA-384', 1024], ['SHA-512', 1024]]) {
  const key = await crypto.subtle.generateKey({ name: 'HMAC', hash }, true, new Set(['sign', 'verify']));
  test(`HMAC ${hash} algorithm`, key.algorithm.name, 'HMAC');
  test(`HMAC ${hash} hash`, key.algorithm.hash.name, hash);
  test(`HMAC ${hash} default length`, key.algorithm.length, length);
  const raw = await crypto.subtle.exportKey('raw', key);
  test(`HMAC ${hash} export size`, raw.byteLength, length / 8);
  const imported = await crypto.subtle.importKey('raw', raw, { name: 'HMAC', hash }, true, ['verify']);
  const exported = await crypto.subtle.exportKey('raw', imported);
  testDeep(`HMAC ${hash} raw round trip`, Array.from(new Uint8Array(exported)), Array.from(new Uint8Array(raw)));
  const signature = await crypto.subtle.sign('HMAC', key, message);
  test(`HMAC ${hash} verification`, await crypto.subtle.verify('HMAC', imported, signature, message), true);
  test(`HMAC ${hash} changed message`, await crypto.subtle.verify('HMAC', imported, signature, new Uint8Array([5])), false);
  await testRejects(`HMAC ${hash} usage enforcement`, () => crypto.subtle.sign('HMAC', imported, message), 'InvalidAccessError');
}

const shortKey = await crypto.subtle.generateKey({ name: 'HMAC', hash: 'SHA-256', length: 9 }, true, ['sign']);
const shortRaw = new Uint8Array(await crypto.subtle.exportKey('raw', shortKey));
test('HMAC explicit bit length', shortKey.algorithm.length, 9);
test('HMAC partial-byte size', shortRaw.length, 2);
test('HMAC unused bits cleared', shortRaw[1] & 0x7f, 0);

await testRejects('generateKey required arguments', () => crypto.subtle.generateKey('AES-GCM'), 'TypeError');
await testRejects('generateKey empty usages', () => crypto.subtle.generateKey({ name: 'AES-GCM', length: 128 }, true, []), 'SyntaxError');
await testRejects('generateKey invalid usage', () => crypto.subtle.generateKey({ name: 'AES-GCM', length: 128 }, true, ['sign']), 'SyntaxError');
await testRejects('generateKey invalid length', () => crypto.subtle.generateKey({ name: 'AES-GCM', length: 100 }, true, ['encrypt']), 'OperationError');
await testRejects('HMAC zero length', () => crypto.subtle.generateKey({ name: 'HMAC', hash: 'SHA-256', length: 0 }, true, ['sign']), 'OperationError');
await testRejects('HMAC unsupported hash', () => crypto.subtle.generateKey({ name: 'HMAC', hash: 'MD5' }, true, ['sign']), 'NotSupportedError');
await testRejects('exportKey invalid format', () => crypto.subtle.exportKey('RAW', shortKey), 'TypeError');
await testRejects('exportKey unsupported format', () => crypto.subtle.exportKey('jwk', shortKey), 'NotSupportedError');

// Asymmetric WebCrypto parity (mbedTLS backend).
const rsaAlgorithm = {
  name: 'RSASSA-PKCS1-v1_5',
  modulusLength: 1024,
  publicExponent: new Uint8Array([1, 0, 1]),
  hash: 'SHA-256'
};
const rsaPair = await crypto.subtle.generateKey(rsaAlgorithm, true, ['sign', 'verify']);
test('RSA private usages', rsaPair.privateKey.usages.join(','), 'sign');
test('RSA public usages', rsaPair.publicKey.usages.join(','), 'verify');
test('RSA algorithm hash', rsaPair.publicKey.algorithm.hash.name, 'SHA-256');
const rsaSignature = await crypto.subtle.sign('RSASSA-PKCS1-v1_5', rsaPair.privateKey, message);
test('RSA signature length', rsaSignature.byteLength, 128);
test('RSA verify', await crypto.subtle.verify('RSASSA-PKCS1-v1_5', rsaPair.publicKey, rsaSignature, message), true);
test('RSA verify changed message', await crypto.subtle.verify('RSASSA-PKCS1-v1_5', rsaPair.publicKey, rsaSignature, new Uint8Array([9])), false);

const rsaJwk = await crypto.subtle.exportKey('jwk', rsaPair.publicKey);
test('RSA JWK kty', rsaJwk.kty, 'RSA');
test('RSA JWK has n', typeof rsaJwk.n, 'string');
test('RSA JWK has e', typeof rsaJwk.e, 'string');
const rsaJwkKey = await crypto.subtle.importKey('jwk', rsaJwk, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['verify']);
test('RSA JWK verify', await crypto.subtle.verify('RSASSA-PKCS1-v1_5', rsaJwkKey, rsaSignature, message), true);
const rsaSpki = await crypto.subtle.exportKey('spki', rsaPair.publicKey);
const rsaSpkiKey = await crypto.subtle.importKey('spki', rsaSpki, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['verify']);
test('RSA SPKI verify', await crypto.subtle.verify('RSASSA-PKCS1-v1_5', rsaSpkiKey, rsaSignature, message), true);
const rsaPkcs8 = await crypto.subtle.exportKey('pkcs8', rsaPair.privateKey);
const rsaPkcs8Key = await crypto.subtle.importKey('pkcs8', rsaPkcs8, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['sign']);
const rsaPkcs8Signature = await crypto.subtle.sign('RSASSA-PKCS1-v1_5', rsaPkcs8Key, message);
test('RSA PKCS#8 sign', await crypto.subtle.verify('RSASSA-PKCS1-v1_5', rsaPair.publicKey, rsaPkcs8Signature, message), true);

const pssPair = await crypto.subtle.generateKey(
  { name: 'RSA-PSS', modulusLength: 1024, publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256', saltLength: 32 },
  true, ['sign', 'verify']
);
const pssSignature = await crypto.subtle.sign({ name: 'RSA-PSS', saltLength: 32 }, pssPair.privateKey, message);
test('RSA-PSS verify', await crypto.subtle.verify({ name: 'RSA-PSS', saltLength: 32 }, pssPair.publicKey, pssSignature, message), true);
test('RSA-PSS salt mismatch', await crypto.subtle.verify({ name: 'RSA-PSS', saltLength: 16 }, pssPair.publicKey, pssSignature, message), false);

const oaepPair = await crypto.subtle.generateKey(
  { name: 'RSA-OAEP', modulusLength: 1024, publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' },
  true, ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey']
);
test('RSA-OAEP public usages', oaepPair.publicKey.usages.join(','), 'encrypt,wrapKey');
test('RSA-OAEP private usages', oaepPair.privateKey.usages.join(','), 'decrypt,unwrapKey');
const oaepEncrypted = await crypto.subtle.encrypt('RSA-OAEP', oaepPair.publicKey, message);
const oaepDecrypted = await crypto.subtle.decrypt('RSA-OAEP', oaepPair.privateKey, oaepEncrypted);
testDeep('RSA-OAEP round trip', Array.from(new Uint8Array(oaepDecrypted)), Array.from(message));

const ecPair = await crypto.subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']);
test('ECDSA named curve', ecPair.privateKey.algorithm.namedCurve, 'P-256');
const ecSignature = await crypto.subtle.sign({ name: 'ECDSA', hash: 'SHA-256' }, ecPair.privateKey, message);
test('ECDSA signature length', ecSignature.byteLength, 64);
test('ECDSA verify', await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, ecPair.publicKey, ecSignature, message), true);
const ecJwk = await crypto.subtle.exportKey('jwk', ecPair.publicKey);
test('ECDSA JWK crv', ecJwk.crv, 'P-256');
const ecJwkKey = await crypto.subtle.importKey('jwk', ecJwk, { name: 'ECDSA', namedCurve: 'P-256' }, true, ['verify']);
test('ECDSA JWK verify', await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, ecJwkKey, ecSignature, message), true);
const ecSpki = await crypto.subtle.exportKey('spki', ecPair.publicKey);
const ecSpkiKey = await crypto.subtle.importKey('spki', ecSpki, { name: 'ECDSA', namedCurve: 'P-256' }, true, ['verify']);
test('ECDSA SPKI verify', await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, ecSpkiKey, ecSignature, message), true);
const ecPkcs8 = await crypto.subtle.exportKey('pkcs8', ecPair.privateKey);
const ecPkcs8Key = await crypto.subtle.importKey('pkcs8', ecPkcs8, { name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign']);
const ecPkcs8Signature = await crypto.subtle.sign({ name: 'ECDSA', hash: 'SHA-256' }, ecPkcs8Key, message);
test('ECDSA PKCS#8 sign', await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, ecPair.publicKey, ecPkcs8Signature, message), true);

const ecdhAlice = await crypto.subtle.generateKey({ name: 'ECDH', namedCurve: 'P-256' }, true, ['deriveBits', 'deriveKey']);
const ecdhBob = await crypto.subtle.generateKey({ name: 'ECDH', namedCurve: 'P-256' }, true, ['deriveBits', 'deriveKey']);
const ecdhAliceBits = await crypto.subtle.deriveBits({ name: 'ECDH', public: ecdhBob.publicKey }, ecdhAlice.privateKey, 256);
const ecdhBobBits = await crypto.subtle.deriveBits({ name: 'ECDH', public: ecdhAlice.publicKey }, ecdhBob.privateKey, 256);
testDeep('ECDH shared bits', Array.from(new Uint8Array(ecdhAliceBits)), Array.from(new Uint8Array(ecdhBobBits)));

const kwKey = await crypto.subtle.generateKey({ name: 'AES-KW', length: 128 }, true, ['wrapKey', 'unwrapKey']);
test('AES-KW name', kwKey.algorithm.name, 'AES-KW');
const aesForWrap = await crypto.subtle.generateKey({ name: 'AES-GCM', length: 128 }, true, ['encrypt']);
const wrappedKey = await crypto.subtle.wrapKey('raw', aesForWrap, kwKey, 'AES-KW');
test('AES-KW wrapped length', wrappedKey.byteLength, 24);
const unwrappedKey = await crypto.subtle.unwrapKey('raw', wrappedKey, kwKey, 'AES-KW', { name: 'AES-GCM', length: 128 }, true, ['encrypt']);
testDeep(
  'AES-KW round trip',
  Array.from(new Uint8Array(await crypto.subtle.exportKey('raw', unwrappedKey))),
  Array.from(new Uint8Array(await crypto.subtle.exportKey('raw', aesForWrap)))
);

summary();
