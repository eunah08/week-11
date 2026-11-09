import { test } from 'node:test';
import assert from 'node:assert/strict';
import { diff, NODE_TYPES, PATCH_TYPES } from '../../../src/core/index.js';

function element(tag, props, children) { // 테스트용 요소 VNode를 만든다
  return { type: NODE_TYPES.ELEMENT, tag: tag, props: props, children: children };
}

function text(value) { // 테스트용 텍스트 VNode를 만든다
  return { type: NODE_TYPES.TEXT, text: value };
}

test('바뀐 게 없으면 빈 배열을 돌려준다', () => {
  const previousVNode = element('div', { class: 'a' }, [text('글')]);
  const nextVNode = element('div', { class: 'a' }, [text('글')]);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, []);
});

test('자식이 늘어나면 INSERT를 index 오름차순으로 만든다', () => {
  const secondItem = element('li', {}, [text('2')]);
  const thirdItem = element('li', {}, [text('3')]);
  const previousVNode = element('ul', {}, [element('li', {}, [])]);
  const nextVNode = element('ul', {}, [element('li', {}, []), secondItem, thirdItem]);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, [
    { type: PATCH_TYPES.INSERT, path: [], index: 1, node: secondItem },
    { type: PATCH_TYPES.INSERT, path: [], index: 2, node: thirdItem },
  ]);
});

test('자식이 줄어들면 REMOVE를 index 내림차순으로 만든다', () => {
  const previousVNode = element('ul', {}, [text('1'), text('2'), text('3')]);
  const nextVNode = element('ul', {}, [text('1')]);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, [
    { type: PATCH_TYPES.REMOVE, path: [2] },
    { type: PATCH_TYPES.REMOVE, path: [1] },
  ]);
});

test('태그가 바뀌면 REPLACE 하나만 만들고 그 아래는 보지 않는다', () => {
  const spanVNode = element('span', { id: 'y' }, [text('b')]);
  const previousVNode = element('div', {}, [element('p', { id: 'x' }, [text('a')])]);
  const nextVNode = element('div', {}, [spanVNode]);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, [{ type: PATCH_TYPES.REPLACE, path: [0], node: spanVNode }]);
});

test('element와 text가 서로 바뀌면 REPLACE를 만든다', () => {
  const italicVNode = element('i', {}, []);
  const textVNode = text('글');
  const previousVNode = element('div', {}, [text('글'), element('b', {}, [])]);
  const nextVNode = element('div', {}, [italicVNode, textVNode]);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, [
    { type: PATCH_TYPES.REPLACE, path: [0], node: italicVNode },
    { type: PATCH_TYPES.REPLACE, path: [1], node: textVNode },
  ]);
});

test('루트 태그가 바뀌면 path가 []인 REPLACE를 만든다', () => {
  const previousVNode = element('div', {}, []);
  const nextVNode = element('section', {}, []);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, [{ type: PATCH_TYPES.REPLACE, path: [], node: nextVNode }]);
});

test('텍스트가 바뀌면 UPDATE_TEXT를 만든다', () => {
  const patches = diff(text('전'), text('후'));

  assert.deepEqual(patches, [{ type: PATCH_TYPES.UPDATE_TEXT, path: [], text: '후' }]);
});

test('속성 추가·변경·삭제를 UPDATE_PROPS 하나로 만든다', () => {
  const previousVNode = element('p', { class: 'a', id: 'x', title: 't' }, []);
  const nextVNode = element('p', { class: 'b', title: 't', hidden: true }, []);

  const patches = diff(previousVNode, nextVNode);

  const expectedPatch = {
    type: PATCH_TYPES.UPDATE_PROPS,
    path: [],
    set: { class: 'b', hidden: true },
    remove: ['id'],
  };
  assert.deepEqual(patches, [expectedPatch]);
});

test('이벤트 함수는 참조가 같으면 같은 값, 다르면 다른 값으로 본다', () => {
  function handleClick() {}
  function handleNextClick() {}
  const previousVNode = element('button', { onClick: handleClick }, []);
  const sameVNode = element('button', { onClick: handleClick }, []);
  const changedVNode = element('button', { onClick: handleNextClick }, []);

  const samePatches = diff(previousVNode, sameVNode);
  const changedPatches = diff(previousVNode, changedVNode);

  assert.deepEqual(samePatches, []);
  assert.deepEqual(changedPatches, [
    { type: PATCH_TYPES.UPDATE_PROPS, path: [], set: { onClick: handleNextClick }, remove: [] },
  ]);
});

test('깊게 중첩된 노드의 변경은 전체 경로를 가진다', () => {
  const previousList = element('ul', {}, [element('li', {}, [text('x')])]);
  const nextList = element('ul', {}, [element('li', {}, [text('y')])]);
  const previousVNode = element('div', {}, [text('a'), previousList]);
  const nextVNode = element('div', {}, [text('a'), nextList]);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, [{ type: PATCH_TYPES.UPDATE_TEXT, path: [1, 0, 0], text: 'y' }]);
});

test('중간 자식을 지우면 뒤 자식들이 변경으로 보이고 마지막이 REMOVE된다', () => {
  const previousVNode = element('ul', {}, [text('1'), text('2'), text('3')]);
  const nextVNode = element('ul', {}, [text('1'), text('3')]);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, [
    { type: PATCH_TYPES.UPDATE_TEXT, path: [1], text: '3' },
    { type: PATCH_TYPES.REMOVE, path: [2] },
  ]);
});

test('같은 부모의 여러 자식 변경을 모두 만든다', () => {
  const previousVNode = element('div', {}, [text('a'), text('b'), text('c')]);
  const nextVNode = element('div', {}, [text('A'), text('b'), text('C')]);

  const patches = diff(previousVNode, nextVNode);

  assert.deepEqual(patches, [
    { type: PATCH_TYPES.UPDATE_TEXT, path: [0], text: 'A' },
    { type: PATCH_TYPES.UPDATE_TEXT, path: [2], text: 'C' },
  ]);
});

test('입력 VNode를 변경하지 않는다', () => {
  const previousVNode = element('div', { id: 'a' }, [text('1'), text('2')]);
  const nextVNode = element('div', { id: 'b' }, [text('3')]);
  // 이 테스트의 VNode에는 함수가 없으므로 문자열로 바꿔 전후를 비교할 수 있다
  const previousBefore = JSON.stringify(previousVNode);
  const nextBefore = JSON.stringify(nextVNode);

  diff(previousVNode, nextVNode);

  assert.equal(JSON.stringify(previousVNode), previousBefore);
  assert.equal(JSON.stringify(nextVNode), nextBefore);
});

test('VNode가 아니면 TypeError를 던진다', () => {
  const expectedError = {
    name: 'TypeError',
    message: '[diff] VNode가 필요합니다',
  };

  assert.throws(() => diff(null, text('a')), expectedError);
  assert.throws(() => diff(text('a'), { type: 'comment' }), expectedError);
});

test('type만 맞고 필요한 값이 없는 VNode도 TypeError를 던진다', () => {
  const expectedError = {
    name: 'TypeError',
    message: '[diff] VNode가 필요합니다',
  };
  const withoutProps = { type: 'element', tag: 'div' };
  const withoutText = { type: 'text' };
  const withBrokenChild = element('div', {}, [{ type: 'text' }]);

  assert.throws(() => diff(withoutProps, withoutProps), expectedError);
  assert.throws(() => diff(withoutText, withoutText), expectedError);
  assert.throws(() => diff(text('a'), { type: 'element' }), expectedError);
  assert.throws(() => diff(withBrokenChild, element('div', {}, [])), expectedError);
});
