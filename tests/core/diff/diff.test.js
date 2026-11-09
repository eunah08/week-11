import { diff, PATCH_TYPES } from '../../../src/core/index.js';
import { test, assertEqual, assertThrows } from '../../testRunner.js';

function text(value) {
  return { type: 'text', text: value };
}

function element(tag, props = {}, children = []) {
  return { type: 'element', tag, props, children };
}

test('diff : 바뀐 게 없으면 빈 배열', () => {
  const vnode = element('div', { class: 'a' }, [text('글')]);

  assertEqual(diff(vnode, element('div', { class: 'a' }, [text('글')])), []);
});

test('diff : 자식 추가(맨 뒤)는 INSERT를 index 오름차순으로 만든다', () => {
  const previousVNode = element('ul', {}, [element('li')]);
  const nextVNode = element('ul', {}, [element('li'), element('li', { id: 'b' }), element('li')]);

  assertEqual(diff(previousVNode, nextVNode), [
    { type: PATCH_TYPES.INSERT, path: [], index: 1, node: nextVNode.children[1] },
    { type: PATCH_TYPES.INSERT, path: [], index: 2, node: nextVNode.children[2] },
  ]);
});

test('diff : 자식 삭제(맨 뒤)는 REMOVE를 인덱스가 큰 것부터 만든다', () => {
  const previousVNode = element('ul', {}, [element('li'), element('li'), element('li')]);

  assertEqual(diff(previousVNode, element('ul', {}, [element('li')])), [
    { type: PATCH_TYPES.REMOVE, path: [2] },
    { type: PATCH_TYPES.REMOVE, path: [1] },
  ]);
});

test('diff : 태그가 바뀌면 REPLACE 하나만 만든다 (div → span)', () => {
  const nextVNode = element('span', {}, [text('다름')]);

  assertEqual(diff(element('div', {}, [text('글')]), nextVNode), [
    { type: PATCH_TYPES.REPLACE, path: [], node: nextVNode },
  ]);
});

test('diff : element ↔ text가 바뀌면 REPLACE', () => {
  const previousVNode = element('p', {}, [element('b')]);
  const nextVNode = element('p', {}, [text('글')]);

  assertEqual(diff(previousVNode, nextVNode), [
    { type: PATCH_TYPES.REPLACE, path: [0], node: nextVNode.children[0] },
  ]);
});

test('diff : 텍스트가 바뀌면 UPDATE_TEXT (텍스트만 있는 트리 포함)', () => {
  assertEqual(diff(text('전'), text('후')), [
    { type: PATCH_TYPES.UPDATE_TEXT, path: [], text: '후' },
  ]);
});

test('diff : 속성 추가·변경·삭제를 UPDATE_PROPS 하나로 만든다', () => {
  const previousVNode = element('div', { class: 'a', id: 'x', title: '같음' });
  const nextVNode = element('div', { class: 'b', title: '같음', style: 'color: red' });

  assertEqual(diff(previousVNode, nextVNode), [{
    type: PATCH_TYPES.UPDATE_PROPS,
    path: [],
    set: { class: 'b', style: 'color: red' },
    remove: ['id'],
  }]);
});

test('diff : 이벤트 함수는 참조가 다르면 다른 값으로 본다', () => {
  const handleClick = () => {};
  const nextHandleClick = () => {};

  const previousVNode = element('button', { onClick: handleClick });

  assertEqual(diff(previousVNode, element('button', { onClick: handleClick })), []);
  assertEqual(diff(previousVNode, element('button', { onClick: nextHandleClick })), [
    { type: PATCH_TYPES.UPDATE_PROPS, path: [], set: { onClick: nextHandleClick }, remove: [] },
  ]);
});

test('diff : 깊게 중첩된 노드의 변경은 그 경로만 가리킨다', () => {
  const createTree = (value) => element('div', {}, [
    element('p'),
    element('p', {}, [element('b', {}, [text(value)])]),
  ]);
  const previousVNode = createTree('전');
  const nextVNode = createTree('후');

  assertEqual(diff(previousVNode, nextVNode), [
    { type: PATCH_TYPES.UPDATE_TEXT, path: [1, 0, 0], text: '후' },
  ]);
});

test('diff : 중간 자식 삭제는 인덱스 비교라 뒤 자식들이 변경으로 보인다', () => {
  const previousVNode = element('ul', {}, [text('A'), text('B'), text('C')]);
  const nextVNode = element('ul', {}, [text('A'), text('C')]);

  assertEqual(diff(previousVNode, nextVNode), [
    { type: PATCH_TYPES.UPDATE_TEXT, path: [1], text: 'C' },
    { type: PATCH_TYPES.REMOVE, path: [2] },
  ]);
});

test('diff : 입력 VNode를 수정하지 않는다', () => {
  const previousVNode = element('div', { class: 'a' }, [text('전')]);
  const nextVNode = element('div', { id: 'b' }, [text('후'), element('i')]);
  const previousCopy = structuredClone(previousVNode);
  const nextCopy = structuredClone(nextVNode);

  diff(previousVNode, nextVNode);
  assertEqual(previousVNode, previousCopy);
  assertEqual(nextVNode, nextCopy);
});

test('diff : VNode가 아니면 TypeError', () => {
  assertThrows(() => diff(null, text('a')), TypeError, '[diff] VNode가 필요합니다');
  assertThrows(() => diff(text('a'), { type: 'element' }), TypeError, '[diff] VNode가 필요합니다');
});

test('diff : toString 같은 기본 객체 이름의 속성도 삭제를 찾아낸다', () => {
  const previousVNode = element('div', { toString: 'a' });
  const nextVNode = element('div', {});

  assertEqual(diff(previousVNode, nextVNode), [
    { type: PATCH_TYPES.UPDATE_PROPS, path: [], set: {}, remove: ['toString'] },
  ]);
});
