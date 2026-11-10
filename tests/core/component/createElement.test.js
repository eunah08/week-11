import { createElement, useState } from '../../../src/core/index.js';
import { getCurrentComponent, setCurrentComponent } from '../../../src/core/component/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

function text(value) {
  return { type: 'text', text: value };
}

test('createElement : 요소 VNode를 만들고 문자열과 숫자는 텍스트 노드가 된다', () => {
  const vnode = createElement('p', { class: 'a' }, 'hi', 3);

  assertEqual(vnode, {
    type: 'element',
    tag: 'p',
    props: { class: 'a' },
    children: [text('hi'), text('3')],
  });
});

test('createElement : props가 null이거나 없으면 빈 객체가 된다', () => {
  assertEqual(createElement('div', null).props, {});
  assertEqual(createElement('div').props, {});
});

test('createElement : tag는 소문자로 저장한다', () => {
  assertEqual(createElement('DIV', null).tag, 'div');
});

test('createElement : props 객체를 복사해서 VNode와 공유하지 않는다', () => {
  const props = { class: 'a' };
  const vnode = createElement('div', props);

  props.class = 'b';
  assertEqual(vnode.props.class, 'a');
});

test('createElement : null, undefined, true, false 자식은 빠진다', () => {
  const vnode = createElement('div', null, null, undefined, true, false, '남음');

  assertEqual(vnode.children, [text('남음')]);
});

test('createElement : 숫자 0은 빠지지 않고 텍스트가 된다', () => {
  assertEqual(createElement('span', null, 0).children, [text('0')]);
});

test('createElement : 중첩된 자식 배열을 평탄화한다', () => {
  const vnode = createElement('ul', null, [['a', ['b']], 'c'], 'd');

  assertEqual(vnode.children, [text('a'), text('b'), text('c'), text('d')]);
});

test('createElement : 이미 만든 VNode를 자식으로 쓸 수 있다', () => {
  const childVNode = createElement('li', null, '하나');
  const vnode = createElement('ul', null, childVNode, [childVNode]);

  assertTrue(vnode.children[0] === childVNode, '자식 VNode가 바뀜');
  assertEqual(vnode.children.length, 2);
});

test('createElement : 공백만 있는 글자와 빈 문자열은 VNode로 만들지 않는다', () => {
  const vnode = createElement('p', null, '', ' ', '\n  ', 'a', ' ', 'b');

  assertEqual(vnode.children, [text('a'), text('b')]);
});

test('createElement : 글자 앞뒤 공백은 그대로 둔다', () => {
  assertEqual(createElement('p', null, ' 남은 개수 ').children, [text(' 남은 개수 ')]);
});

test('createElement : 자식으로 쓸 수 없는 값이면 TypeError', () => {
  assertThrows(() => createElement('p', null, { a: 1 }), TypeError, '자식으로 쓸 수 없는 값');
  assertThrows(() => createElement('p', null, Symbol('s')), TypeError, '자식으로 쓸 수 없는 값');
});

test('createElement : tag가 문자열도 함수도 아니면 TypeError', () => {
  assertThrows(() => createElement(null, null), TypeError, '[createElement] tag가 올바르지 않습니다');
  assertThrows(() => createElement(123, null), TypeError, 'tag가 올바르지 않습니다');
});

test('createElement : props가 객체가 아니면 TypeError', () => {
  assertThrows(() => createElement('p', 'text'), TypeError, 'props는 객체 또는 null');
  assertThrows(() => createElement('p', [1]), TypeError, 'props는 객체 또는 null');
});

test('createElement : 자식 컴포넌트는 { ...props, children }로 호출되어 VNode가 자식으로 쓰인다', () => {
  let receivedProps = null;
  const badge = (props) => {
    receivedProps = props;
    return createElement('b', { class: props.tone }, props.children);
  };
  const vnode = createElement('div', null, createElement(badge, { tone: 'red' }, '글', null, 1));

  assertEqual(vnode.children[0], createElement('b', { class: 'red' }, '글', '1'));
  assertEqual(receivedProps.children, [text('글'), text('1')]);
});

test('createElement : 자식 컴포넌트는 props를 바꿀 수 없다', () => {
  const sneaky = (props) => {
    props.tone = 'changed';
    return createElement('b', null);
  };

  assertThrows(() => createElement(sneaky, { tone: 'red' }), TypeError, '');
});

test('createElement : 자식 컴포넌트가 VNode를 반환하지 않으면 TypeError', () => {
  assertThrows(() => createElement(() => '글자', null), TypeError, '자식 컴포넌트는 VNode');
  assertThrows(() => createElement(() => undefined, null), TypeError, '자식 컴포넌트는 VNode');
});

test('createElement : 자식 컴포넌트 안에서 Hook을 쓰면 Error', () => {
  const component = { hookIndex: 0, hooks: [], isRendering: true };
  const statefulChild = () => {
    useState(0);
    return createElement('p', null);
  };

  setCurrentComponent(component);

  try {
    assertThrows(
      () => createElement(statefulChild, null),
      Error,
      '[useState] 루트 컴포넌트 렌더링 중에만 호출할 수 있습니다',
    );
    // 자식 호출이 끝나면 부모의 렌더링 문맥이 되돌아와야 한다.
    assertTrue(getCurrentComponent() === component, '렌더링 문맥이 되돌아오지 않음');
    assertTrue(component.hookIndex === 0, '자식 컴포넌트가 hookIndex를 건드림');
  } finally {
    setCurrentComponent(null);
  }
});
