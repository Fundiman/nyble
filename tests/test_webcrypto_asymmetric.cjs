'use strict';

function assert(condition, message) {
  if (!condition) throw new Error(message || 'assertion failed');
}

function bytes(value) {
  return new Uint8Array(value);
}

function sameBytes(actual, expected, message) {
  const left = bytes(actual);
  const right = bytes(expected);
  assert(left.length === right.length, `${message}: byte length`);
  for (let i = 0; i < left.length; i++) {
    assert(left[i] === right[i], `${message}: byte ${i}`);
  }
}

function text(value) {
  return new TextEncoder().encode(value);
}

async function rejectsAsPromise(fn, name, message) {
  let error = null;
  try {
    await fn();
  } catch (caught) {
    error = caught;
  }
  assert(error instanceof Error, `${message}: expected an Error`);
  assert(error.name === name, `${message}: expected ${name}, got ${error.name}`);
}

async function testRsassaPkcs1() {
  const pair = await crypto.subtle.generateKey(
    {
      name: 'RSASSA-PKCS1-v1_5',
      modulusLength: 1024,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256'
    },
    true,
    ['sign', 'verify']
  );

  assert(pair.privateKey.type === 'private', 'RSA private type');
  assert(pair.publicKey.type === 'public', 'RSA public type');
  assert(pair.privateKey.usages.join(',') === 'sign', 'RSA private usages');
  assert(pair.publicKey.usages.join(',') === 'verify', 'RSA public usages');
  assert(pair.publicKey.algorithm.hash.name === 'SHA-256', 'RSA algorithm hash');

  const message = text('rsassa-pkcs1 message');
  const signature = await crypto.subtle.sign('RSASSA-PKCS1-v1_5', pair.privateKey, message);
  assert(signature.byteLength === 128, 'RSA signature length');
  assert(
    await crypto.subtle.verify('RSASSA-PKCS1-v1_5', pair.publicKey, signature, message),
    'RSA signature verifies'
  );
  assert(
    !(await crypto.subtle.verify('RSASSA-PKCS1-v1_5', pair.publicKey, signature, text('other'))),
    'RSA signature rejects different data'
  );

  const jwk = await crypto.subtle.exportKey('jwk', pair.publicKey);
  assert(jwk.kty === 'RSA', 'RSA JWK kty');
  assert(typeof jwk.n === 'string' && typeof jwk.e === 'string', 'RSA JWK values');
  assert(jwk.d === undefined, 'public JWK omits private members');
  const fromJwk = await crypto.subtle.importKey(
    'jwk', jwk, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['verify']
  );
  assert(
    await crypto.subtle.verify('RSASSA-PKCS1-v1_5', fromJwk, signature, message),
    'RSA JWK import verifies'
  );

  const privateJwk = await crypto.subtle.exportKey('jwk', pair.privateKey);
  assert(typeof privateJwk.d === 'string', 'private JWK includes d');
  const privateFromJwk = await crypto.subtle.importKey(
    'jwk', privateJwk, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['sign']
  );
  const jwkSignature = await crypto.subtle.sign('RSASSA-PKCS1-v1_5', privateFromJwk, message);
  assert(
    await crypto.subtle.verify('RSASSA-PKCS1-v1_5', pair.publicKey, jwkSignature, message),
    'private JWK import signs'
  );

  const spki = await crypto.subtle.exportKey('spki', pair.publicKey);
  const fromSpki = await crypto.subtle.importKey(
    'spki', spki, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['verify']
  );
  assert(
    await crypto.subtle.verify('RSASSA-PKCS1-v1_5', fromSpki, signature, message),
    'SPKI import verifies'
  );

  const pkcs8 = await crypto.subtle.exportKey('pkcs8', pair.privateKey);
  const fromPkcs8 = await crypto.subtle.importKey(
    'pkcs8', pkcs8, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['sign']
  );
  const pkcs8Signature = await crypto.subtle.sign('RSASSA-PKCS1-v1_5', fromPkcs8, message);
  assert(
    await crypto.subtle.verify('RSASSA-PKCS1-v1_5', pair.publicKey, pkcs8Signature, message),
    'PKCS#8 import signs'
  );
}

async function testRsaPss() {
  const pair = await crypto.subtle.generateKey(
    {
      name: 'RSA-PSS',
      modulusLength: 1024,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
      saltLength: 32
    },
    true,
    ['sign', 'verify']
  );
  assert(pair.privateKey.algorithm.saltLength === 32, 'RSA-PSS salt length');
  const message = text('rsa-pss message');
  const signature = await crypto.subtle.sign(
    { name: 'RSA-PSS', saltLength: 32 }, pair.privateKey, message
  );
  assert(
    await crypto.subtle.verify({ name: 'RSA-PSS', saltLength: 32 }, pair.publicKey, signature, message),
    'RSA-PSS verifies'
  );
  assert(
    !(await crypto.subtle.verify({ name: 'RSA-PSS', saltLength: 16 }, pair.publicKey, signature, message)),
    'RSA-PSS salt length mismatch'
  );
}

async function testRsaOaep() {
  const pair = await crypto.subtle.generateKey(
    {
      name: 'RSA-OAEP',
      modulusLength: 1024,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256'
    },
    true,
    ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey']
  );
  assert(pair.publicKey.usages.join(',') === 'encrypt,wrapKey', 'RSA-OAEP public usages');
  assert(pair.privateKey.usages.join(',') === 'decrypt,unwrapKey', 'RSA-OAEP private usages');

  const message = text('rsa-oaep message');
  const encrypted = await crypto.subtle.encrypt('RSA-OAEP', pair.publicKey, message);
  assert(encrypted.byteLength === 128, 'RSA-OAEP ciphertext length');
  const decrypted = await crypto.subtle.decrypt('RSA-OAEP', pair.privateKey, encrypted);
  assert(new TextDecoder().decode(decrypted) === 'rsa-oaep message', 'RSA-OAEP round trip');

  const aes = await crypto.subtle.generateKey(
    { name: 'AES-GCM', length: 128 }, true, ['encrypt', 'decrypt']
  );
  const wrapped = await crypto.subtle.wrapKey('raw', aes, pair.publicKey, 'RSA-OAEP');
  assert(wrapped.byteLength === 128, 'RSA-OAEP wrap length');
  const unwrapped = await crypto.subtle.unwrapKey(
    'raw', wrapped, pair.privateKey, 'RSA-OAEP',
    { name: 'AES-GCM', length: 128 }, true, ['encrypt', 'decrypt']
  );
  sameBytes(
    await crypto.subtle.exportKey('raw', aes),
    await crypto.subtle.exportKey('raw', unwrapped),
    'RSA-OAEP wrap/unwrap round trip'
  );
}

async function testEcdsa() {
  const pair = await crypto.subtle.generateKey(
    { name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']
  );
  assert(pair.privateKey.algorithm.namedCurve === 'P-256', 'ECDSA named curve');
  assert(pair.privateKey.usages.join(',') === 'sign', 'ECDSA private usages');
  assert(pair.publicKey.usages.join(',') === 'verify', 'ECDSA public usages');

  const message = text('ecdsa message');
  const signature = await crypto.subtle.sign(
    { name: 'ECDSA', hash: 'SHA-256' }, pair.privateKey, message
  );
  assert(signature.byteLength === 64, 'ECDSA raw signature length');
  assert(
    await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, pair.publicKey, signature, message),
    'ECDSA verifies'
  );
  assert(
    !(await crypto.subtle.verify(
      { name: 'ECDSA', hash: 'SHA-256' }, pair.publicKey, signature, text('other')
    )),
    'ECDSA rejects different data'
  );

  const jwk = await crypto.subtle.exportKey('jwk', pair.publicKey);
  assert(jwk.kty === 'EC' && jwk.crv === 'P-256', 'ECDSA JWK');
  const fromJwk = await crypto.subtle.importKey(
    'jwk', jwk, { name: 'ECDSA', namedCurve: 'P-256' }, true, ['verify']
  );
  assert(
    await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, fromJwk, signature, message),
    'ECDSA JWK import verifies'
  );

  const spki = await crypto.subtle.exportKey('spki', pair.publicKey);
  const fromSpki = await crypto.subtle.importKey(
    'spki', spki, { name: 'ECDSA', namedCurve: 'P-256' }, true, ['verify']
  );
  assert(
    await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, fromSpki, signature, message),
    'ECDSA SPKI import verifies'
  );

  const pkcs8 = await crypto.subtle.exportKey('pkcs8', pair.privateKey);
  const fromPkcs8 = await crypto.subtle.importKey(
    'pkcs8', pkcs8, { name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign']
  );
  const pkcs8Signature = await crypto.subtle.sign(
    { name: 'ECDSA', hash: 'SHA-256' }, fromPkcs8, message
  );
  assert(
    await crypto.subtle.verify(
      { name: 'ECDSA', hash: 'SHA-256' }, pair.publicKey, pkcs8Signature, message
    ),
    'ECDSA PKCS#8 import signs'
  );
}

async function testEcdh() {
  const alice = await crypto.subtle.generateKey(
    { name: 'ECDH', namedCurve: 'P-256' }, true, ['deriveBits', 'deriveKey']
  );
  const bob = await crypto.subtle.generateKey(
    { name: 'ECDH', namedCurve: 'P-256' }, true, ['deriveBits', 'deriveKey']
  );
  const bitsAlice = await crypto.subtle.deriveBits(
    { name: 'ECDH', public: bob.publicKey }, alice.privateKey, 256
  );
  const bitsBob = await crypto.subtle.deriveBits(
    { name: 'ECDH', public: alice.publicKey }, bob.privateKey, 256
  );
  assert(bitsAlice.byteLength === 32, 'ECDH deriveBits length');
  sameBytes(bitsAlice, bitsBob, 'ECDH shared secret');

  const aesAlice = await crypto.subtle.deriveKey(
    { name: 'ECDH', public: bob.publicKey }, alice.privateKey,
    { name: 'AES-GCM', length: 128 }, true, ['encrypt']
  );
  const aesBob = await crypto.subtle.deriveKey(
    { name: 'ECDH', public: alice.publicKey }, bob.privateKey,
    { name: 'AES-GCM', length: 128 }, true, ['decrypt']
  );
  sameBytes(
    await crypto.subtle.exportKey('raw', aesAlice),
    await crypto.subtle.exportKey('raw', aesBob),
    'ECDH deriveKey'
  );

  await rejectsAsPromise(
    () => crypto.subtle.deriveBits({ name: 'ECDH', public: bob.publicKey }, alice.publicKey, 256),
    'InvalidAccessError',
    'ECDH requires a private key'
  );
}

async function testAesKw() {
  const wrapping = await crypto.subtle.generateKey(
    { name: 'AES-KW', length: 128 }, true, ['wrapKey', 'unwrapKey']
  );
  assert(wrapping.algorithm.name === 'AES-KW', 'AES-KW algorithm');
  assert(wrapping.usages.join(',') === 'wrapKey,unwrapKey', 'AES-KW canonical usages');

  const raw = await crypto.subtle.exportKey('raw', wrapping);
  assert(raw.byteLength === 16, 'AES-KW raw export');
  const imported = await crypto.subtle.importKey(
    'raw', raw, 'AES-KW', true, ['wrapKey', 'unwrapKey']
  );

  const aes = await crypto.subtle.generateKey(
    { name: 'AES-GCM', length: 128 }, true, ['encrypt', 'decrypt']
  );
  const wrapped = await crypto.subtle.wrapKey('raw', aes, imported, 'AES-KW');
  assert(wrapped.byteLength === 24, 'AES-KW wrapped length');
  const unwrapped = await crypto.subtle.unwrapKey(
    'raw', wrapped, imported, 'AES-KW',
    { name: 'AES-GCM', length: 128 }, true, ['encrypt', 'decrypt']
  );
  sameBytes(
    await crypto.subtle.exportKey('raw', aes),
    await crypto.subtle.exportKey('raw', unwrapped),
    'AES-KW round trip'
  );

  await rejectsAsPromise(
    () => crypto.subtle.generateKey({ name: 'AES-KW', length: 64 }, true, ['wrapKey']),
    'OperationError',
    'AES-KW invalid length'
  );
}

async function main() {
  await testRsassaPkcs1();
  await testRsaPss();
  await testRsaOaep();
  await testEcdsa();
  await testEcdh();
  await testAesKw();
  console.log('webcrypto asymmetric tests passed');
}

main().catch((error) => {
  console.error(error && error.stack ? error.stack : String(error));
  process.exit(1);
});
