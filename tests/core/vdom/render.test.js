import { test } from 'node:test';
import assert from 'node:assert/strict';
import { JSDOM } from 'jsdom';
import { domToVNode, render, NODE_TYPES } from '../../../src/core/index.js';

// Node.js에는 브라우저 DOM이 없으므로 jsdom으로 만든 DOM을 전역에 넣는다
const jsdom = new JSDOM('');
globalThis.document = jsdom.window.document;
globalThis.Node = jsdom.window.Node;

test('domToVNode와 왕복 변환하면 같은 구조가 나온다', () => {
  const container = document.createElement('div');
  container.innerHTML = '<div class="a"><h1 title="t">제목</h1><ul><li>1</li><li>2</li></ul></div>';
  const vnode = domToVNode(container.firstElementChild);

  const renderedNode = render(vnode);
  const roundTripVNode = domToVNode(renderedNode);

  assert.deepEqual(roundTripVNode, vnode);
});

test('텍스트 VNode는 텍스트 노드가 된다', () => {
  const vnode = { type: NODE_TYPES.TEXT, text: '안녕' };

  const domNode = render(vnode);

  assert.equal(domNode.nodeType, Node.TEXT_NODE);
  assert.equal(domNode.nodeValue, '안녕');
});

test('document에 붙이지 않은 새 노드를 돌려준다', () => {
  const vnode = { type: NODE_TYPES.ELEMENT, tag: 'p', props: {}, children: [] };

  const domNode = render(vnode);

  assert.equal(domNode.parentNode, null);
  assert.equal(domNode.isConnected, false);
});

test('불리언·value·checked 속성을 규칙대로 넣는다', () => {
  const vnode = {
    type: NODE_TYPES.ELEMENT,
    tag: 'input',
    props: { disabled: true, hidden: false, value: '정글', checked: true, title: null },
    children: [],
  };

  const domNode = render(vnode);

  assert.equal(domNode.getAttribute('disabled'), '');
  assert.equal(domNode.hasAttribute('hidden'), false);
  assert.equal(domNode.hasAttribute('title'), false);
  assert.equal(domNode.value, '정글');
  assert.equal(domNode.checked, true);
});

test('on으로 시작하는 함수 props는 이벤트로 연결한다', () => {
  let clickCount = 0;
  function handleClick() {
    clickCount = clickCount + 1;
  }
  const vnode = {
    type: NODE_TYPES.ELEMENT,
    tag: 'button',
    props: { onClick: handleClick },
    children: [],
  };

  const domNode = render(vnode);
  domNode.click();

  assert.equal(clickCount, 1);
  assert.equal(domNode.hasAttribute('onClick'), false);
});

test('알 수 없는 VNode 타입이면 TypeError를 던진다', () => {
  const expectedError = {
    name: 'TypeError',
    message: '[render] 알 수 없는 VNode 타입입니다',
  };

  assert.throws(() => render({ type: 'comment' }), expectedError);
  assert.throws(() => render(null), expectedError);
});
