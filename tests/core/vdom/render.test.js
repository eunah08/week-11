import { render, domToVNode } from '../../../src/core/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

test('render : domToVNode와 왕복 변환하면 같은 구조가 된다', () => {
  const template = document.createElement('template');

  template.innerHTML = '<ul class="list"><li>하나</li><li><b>둘</b> 셋</li><li></li></ul>';

  const vnode = domToVNode(template.content.firstChild);

  assertEqual(domToVNode(render(vnode)), vnode);
});

test('render : 텍스트 VNode는 텍스트 노드가 된다', () => {
  const domNode = render({ type: 'text', text: '글자' });

  assertTrue(domNode.nodeType === Node.TEXT_NODE, '텍스트 노드가 아님');
  assertEqual(domNode.textContent, '글자');
});

test('render : document에 붙이지 않은 노드를 반환한다', () => {
  const domNode = render({ type: 'element', tag: 'div', props: {}, children: [] });

  assertEqual(domNode.parentNode, null);
});

test('render : 불리언 속성과 value, checked 프로퍼티를 처리한다', () => {
  const domNode = render({
    type: 'element',
    tag: 'input',
    props: { type: 'checkbox', checked: true, disabled: false, value: 'a' },
    children: [],
  });

  assertTrue(domNode.hasAttribute('checked') && domNode.checked, 'checked가 아님');
  assertTrue(!domNode.hasAttribute('disabled'), 'false인데 disabled 속성이 있음');
  assertEqual(domNode.value, 'a');
});

test('render : 이벤트 props를 리스너로 단다', () => {
  let clickCount = 0;
  const domNode = render({
    type: 'element',
    tag: 'button',
    props: { onClick: () => { clickCount += 1; } },
    children: [],
  });

  domNode.click();
  assertEqual(clickCount, 1);
  assertTrue(!domNode.hasAttribute('onClick'), '이벤트가 속성으로 들어감');
});

test('render : 알 수 없는 VNode 타입이면 TypeError', () => {
  assertThrows(() => render({ type: 'comment' }), TypeError, '[render] 알 수 없는 VNode 타입입니다');
  assertThrows(() => render(null), TypeError, '[render] 알 수 없는 VNode 타입입니다');
  // 객체가 원래 가진 이름(toString 등)도 알 수 없는 타입으로 본다.
  assertThrows(() => render({ type: 'toString' }), TypeError, '[render] 알 수 없는 VNode 타입입니다');
});

test('render : select의 value는 option이 붙은 뒤에도 유지된다', () => {
  const option = (value) => ({
    type: 'element',
    tag: 'option',
    props: { value },
    children: [{ type: 'text', text: value }],
  });
  const domNode = render({
    type: 'element',
    tag: 'select',
    props: { value: 'b' },
    children: [option('a'), option('b'), option('c')],
  });

  assertEqual(domNode.value, 'b');
});
