import { test } from 'node:test';
import assert from 'node:assert/strict';
import { isVNode } from '../../../src/core/index.js';

test('올바른 텍스트·요소 VNode는 true다', () => {
  const textVNode = { type: 'text', text: '글' };
  const emptyElementVNode = { type: 'element', tag: 'div', props: {}, children: [] };
  const nestedVNode = {
    type: 'element',
    tag: 'ul',
    props: { class: 'list' },
    children: [{ type: 'element', tag: 'li', props: {}, children: [textVNode] }],
  };

  assert.equal(isVNode(textVNode), true);
  assert.equal(isVNode(emptyElementVNode), true);
  assert.equal(isVNode(nestedVNode), true);
});

test('빈 문자열 텍스트도 문자열이므로 true다', () => {
  assert.equal(isVNode({ type: 'text', text: '' }), true);
});

test('객체가 아니거나 알 수 없는 type이면 false다', () => {
  assert.equal(isVNode(null), false);
  assert.equal(isVNode(undefined), false);
  assert.equal(isVNode('text'), false);
  assert.equal(isVNode({}), false);
  assert.equal(isVNode({ type: 'comment' }), false);
});

test('텍스트 VNode에 text가 없거나 문자열이 아니면 false다', () => {
  assert.equal(isVNode({ type: 'text' }), false);
  assert.equal(isVNode({ type: 'text', text: 3 }), false);
});

test('요소 VNode에 tag, props, children이 없거나 잘못되면 false다', () => {
  assert.equal(isVNode({ type: 'element' }), false);
  assert.equal(isVNode({ type: 'element', props: {}, children: [] }), false);
  assert.equal(isVNode({ type: 'element', tag: 'div', children: [] }), false);
  assert.equal(isVNode({ type: 'element', tag: 'div', props: null, children: [] }), false);
  assert.equal(isVNode({ type: 'element', tag: 'div', props: {} }), false);
  assert.equal(isVNode({ type: 'element', tag: 'div', props: {}, children: 'x' }), false);
});

test('자식 중 하나라도 VNode가 아니면 false다', () => {
  const brokenChild = { type: 'text' };
  const deepBrokenVNode = {
    type: 'element',
    tag: 'div',
    props: {},
    children: [{ type: 'element', tag: 'p', props: {}, children: [brokenChild] }],
  };

  assert.equal(isVNode(deepBrokenVNode), false);
});
