import { test } from 'node:test';
import assert from 'node:assert/strict';
import { JSDOM } from 'jsdom';
import { isIgnoredNode } from '../../../src/core/vdom/isIgnoredNode.js';

// Node.js에는 브라우저 DOM이 없으므로 jsdom으로 만든 DOM을 전역에 넣는다
const jsdom = new JSDOM('');
globalThis.document = jsdom.window.document;
globalThis.Node = jsdom.window.Node;

test('주석 노드는 무시한다', () => {
  assert.equal(isIgnoredNode(document.createComment('주석')), true);
});

test('공백만 있는 텍스트와 빈 텍스트는 무시한다', () => {
  assert.equal(isIgnoredNode(document.createTextNode('  \n ')), true);
  assert.equal(isIgnoredNode(document.createTextNode('')), true);
});

test('글자가 있는 텍스트는 앞뒤에 공백이 있어도 무시하지 않는다', () => {
  assert.equal(isIgnoredNode(document.createTextNode(' 글자 ')), false);
});

test('script와 style 요소는 무시한다', () => {
  assert.equal(isIgnoredNode(document.createElement('script')), true);
  assert.equal(isIgnoredNode(document.createElement('style')), true);
});

test('그 밖의 요소는 무시하지 않는다', () => {
  assert.equal(isIgnoredNode(document.createElement('div')), false);
  assert.equal(isIgnoredNode(document.createElement('li')), false);
});
