'use strict';

/*
 * Plain node:test, no framework, matching webroot's no-build-step rule
 * (see docs/BUILDING.md). Run with `node --test module/webroot/app.test.js`.
 * Covers normalizeExecResult (regression for the APatch bug report where
 * ksu.exec() returning a bare string crashed every caller) and
 * checkCompatibility (previously untested, see docs/CONTRIBUTING.md).
 */
const test = require('node:test');
const assert = require('node:assert/strict');
const { checkCompatibility, normalizeExecResult } = require('./app.js');

test('normalizeExecResult passes through the KernelSU/Magisk object shape', () => {
  const result = normalizeExecResult({ errno: 0, stdout: 'arm64-v8a\n', stderr: '' });
  assert.equal(result.stdout, 'arm64-v8a\n');
  assert.equal(result.errno, 0);
});

test('normalizeExecResult wraps a bare string, like APatch exec() returns', () => {
  const result = normalizeExecResult('arm64-v8a,armeabi-v7a\n');
  assert.equal(result.stdout, 'arm64-v8a,armeabi-v7a\n');
  assert.equal(result.errno, 0);
  assert.equal(result.stderr, '');
});

test('normalizeExecResult never throws on undefined', () => {
  const result = normalizeExecResult(undefined);
  assert.equal(result.stdout, '');
  assert.equal(result.errno, 0);
});

test('normalizeExecResult keeps a nonzero errno', () => {
  const result = normalizeExecResult({ errno: 1, stdout: '', stderr: 'not found' });
  assert.equal(result.errno, 1);
  assert.equal(result.stderr, 'not found');
});

test('checkCompatibility: no native libs runs natively', () => {
  const report = checkCompatibility([], ['arm64-v8a'], false);
  assert.equal(report.verdict, 'RUNS_NATIVELY');
});

test('checkCompatibility: already has a supported ABI runs natively', () => {
  const report = checkCompatibility(['arm64-v8a', 'armeabi-v7a'], ['arm64-v8a'], false);
  assert.equal(report.verdict, 'RUNS_NATIVELY');
});

test('checkCompatibility: 32-bit-only with the bridge active needs the bridge', () => {
  const report = checkCompatibility(['armeabi-v7a'], ['arm64-v8a'], true);
  assert.equal(report.verdict, 'NEEDS_BRIDGE');
});

test('checkCompatibility: 32-bit-only without the bridge is blocked', () => {
  const report = checkCompatibility(['armeabi-v7a'], ['arm64-v8a'], false);
  assert.equal(report.verdict, 'BLOCKED');
});

test('checkCompatibility: an ABI Mango does not handle is blocked', () => {
  const report = checkCompatibility(['mips'], ['arm64-v8a'], true);
  assert.equal(report.verdict, 'BLOCKED');
});
