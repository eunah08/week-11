import { test } from 'node:test';
import assert from 'node:assert/strict';
import { JSDOM } from 'jsdom';
import {
  applyPatches,
  diff,
  domToVNode,
  NODE_TYPES,
  PATCH_TYPES,
} from '../../../src/core/index.js';

// Node.js에는 브라우저 DOM이 없으므로 jsdom으로 만든 DOM을 전역에 넣는다
const jsdom = new JSDOM('');
globalThis.document = jsdom.window.document;
globalThis.Node = jsdom.window.Node;

// 루트가 교체될 수 있도록 루트에는 항상 부모(컨테이너)가 있다
function createRootNode(html) { // HTML을 DOM으로 만들고 비교 대상 루트(첫 요소)를 돌려준다
  const container = document.createElement('div');
  container.innerHTML = html;
  return container.firstElementChild;
}

function patchHtml(beforeHtml, afterHtml) { // before를 after로 바꾸는 Patch를 적용하고 결과를 확인한다
  const rootNode = createRootNode(beforeHtml);
  const previousVNode = domToVNode(rootNode);
  const nextVNode = domToVNode(createRootNode(afterHtml));

  const patches = diff(previousVNode, nextVNode);
  const nextRootNode = applyPatches(rootNode, patches);

  const resultVNode = domToVNode(nextRootNode);
  assert.deepEqual(resultVNode, nextVNode);
  return nextRootNode;
}

function element(tag, props, children) { // 테스트용 요소 VNode를 만든다
  return { type: NODE_TYPES.ELEMENT, tag: tag, props: props, children: children };
}

function createPropsPatch(path, set, remove) { // UPDATE_PROPS Patch를 만든다
  return { type: PATCH_TYPES.UPDATE_PROPS, path: path, set: set, remove: remove };
}

test('INSERT : 부모의 index 위치에 노드를 끼워 넣는다', () => {
  const rootNode = createRootNode('<ul><li>1</li><li>3</li></ul>');
  const insertPatch = { type: PATCH_TYPES.INSERT, path: [], index: 1, node: element('li', {}, []) };

  applyPatches(rootNode, [insertPatch]);

  assert.equal(rootNode.outerHTML, '<ul><li>1</li><li></li><li>3</li></ul>');
});

test('REMOVE : 해당 노드를 지운다', () => {
  const rootNode = createRootNode('<ul><li>1</li><li>2</li></ul>');
  const removePatch = { type: PATCH_TYPES.REMOVE, path: [0] };

  applyPatches(rootNode, [removePatch]);

  assert.equal(rootNode.outerHTML, '<ul><li>2</li></ul>');
});

test('REPLACE : 새 노드로 교체한다', () => {
  const rootNode = createRootNode('<div><p>a</p></div>');
  const spanVNode = element('span', { id: 'x' }, []);
  const replacePatch = { type: PATCH_TYPES.REPLACE, path: [0], node: spanVNode };

  applyPatches(rootNode, [replacePatch]);

  assert.equal(rootNode.outerHTML, '<div><span id="x"></span></div>');
});

test('UPDATE_TEXT : 텍스트를 바꾼다', () => {
  const rootNode = createRootNode('<p>전</p>');
  const textPatch = { type: PATCH_TYPES.UPDATE_TEXT, path: [0], text: '후' };

  applyPatches(rootNode, [textPatch]);

  assert.equal(rootNode.outerHTML, '<p>후</p>');
});

test('UPDATE_PROPS : 속성을 설정하고 제거한다', () => {
  const rootNode = createRootNode('<p id="x" class="a"></p>');
  const propsPatch = createPropsPatch([], { class: 'b', hidden: true }, ['id']);

  applyPatches(rootNode, [propsPatch]);

  assert.equal(rootNode.getAttribute('class'), 'b');
  assert.equal(rootNode.getAttribute('hidden'), '');
  assert.equal(rootNode.hasAttribute('id'), false);
});

test('루트가 교체되면 새 루트를 돌려준다', () => {
  const rootNode = createRootNode('<div>a</div>');
  const container = rootNode.parentNode;
  const sectionVNode = element('section', {}, []);
  const replacePatch = { type: PATCH_TYPES.REPLACE, path: [], node: sectionVNode };

  const nextRootNode = applyPatches(rootNode, [replacePatch]);

  assert.notEqual(nextRootNode, rootNode);
  assert.equal(nextRootNode.tagName, 'SECTION');
  assert.equal(container.firstElementChild, nextRootNode);
});

test('부모가 없는 루트는 교체할 수 없다', () => {
  const rootNode = document.createElement('div');
  const replacePatch = { type: PATCH_TYPES.REPLACE, path: [], node: element('p', {}, []) };

  assert.throws(() => applyPatches(rootNode, [replacePatch]), {
    name: 'Error',
    message: '[applyPatches] 부모가 없는 루트는 교체할 수 없습니다',
  });
});

test('연속된 형제를 여러 개 추가·삭제해도 밀리지 않는다', () => {
  patchHtml('<ul><li>1</li></ul>', '<ul><li>1</li><li>2</li><li>3</li><li>4</li></ul>');
  patchHtml('<ul><li>1</li><li>2</li><li>3</li><li>4</li></ul>', '<ul><li>1</li></ul>');
  patchHtml('<ul><li>1</li><li>2</li><li>3</li></ul>', '<ul><li>1</li><li>3</li></ul>');
});

test('element와 text가 서로 바뀌어도 맞게 반영된다', () => {
  patchHtml('<div>글<b>굵게</b></div>', '<div><i>기울임</i>글</div>');
});

test('깊은 노드에서 여러 속성과 텍스트가 한 번에 바뀌어도 맞게 반영된다', () => {
  const beforeHtml = '<div><ul><li><span class="a" id="b">x</span></li></ul></div>';
  const afterHtml = '<div><ul><li><span class="c" title="t">y</span></li></ul></div>';

  patchHtml(beforeHtml, afterHtml);
});

test('바뀌지 않은 DOM 노드는 그대로 유지한다', () => {
  const rootNode = createRootNode('<div><h1>제목</h1><p>전</p><ul><li>1</li></ul></div>');
  const titleNode = rootNode.children[0];
  const paragraphNode = rootNode.children[1];
  const listNode = rootNode.children[2];
  const previousVNode = domToVNode(rootNode);
  const nextVNode = domToVNode(createRootNode('<div><h1>제목</h1><p>후</p><ul><li>1</li></ul></div>'));

  const patches = diff(previousVNode, nextVNode);
  applyPatches(rootNode, patches);

  assert.equal(rootNode.children[0], titleNode);
  assert.equal(rootNode.children[1], paragraphNode);
  assert.equal(rootNode.children[2], listNode);
});

test('바뀐 게 없으면 DOM을 건드리지 않는다', () => {
  const rootNode = createRootNode('<div><p class="a">그대로</p></div>');
  const paragraphNode = rootNode.firstElementChild;
  const htmlBefore = rootNode.outerHTML;

  const patches = diff(domToVNode(rootNode), domToVNode(rootNode));
  const resultRootNode = applyPatches(rootNode, patches);

  assert.deepEqual(patches, []);
  assert.equal(resultRootNode, rootNode);
  assert.equal(rootNode.firstElementChild, paragraphNode);
  assert.equal(rootNode.outerHTML, htmlBefore);
});

test('경로를 셀 때 주석·공백 텍스트·script·style을 건너뛴다', () => {
  const html = '<ul>\n  <!-- c -->\n  <script></script>\n  <li>1</li>\n  <li>2</li>\n</ul>';
  const rootNode = createRootNode(html);
  const textPatch = { type: PATCH_TYPES.UPDATE_TEXT, path: [1, 0], text: '바뀜' };

  applyPatches(rootNode, [textPatch]);

  const secondItem = rootNode.querySelectorAll('li')[1];
  assert.equal(secondItem.textContent, '바뀜');
});

test('특수 속성 class·style·value·checked·disabled를 처리한다', () => {
  const rootNode = createRootNode('<div><input class="a"><input type="checkbox" checked></div>');
  const textInput = rootNode.children[0];
  const checkbox = rootNode.children[1];
  // 사용자가 입력해서 속성과 현재 값이 달라진 상태를 만든다
  textInput.value = '사용자가 입력';
  const inputSet = { class: 'b', style: 'color: red;', value: '정글', disabled: true };
  const inputPatch = createPropsPatch([0], inputSet, []);
  const checkboxPatch = createPropsPatch([1], {}, ['checked']);

  applyPatches(rootNode, [inputPatch, checkboxPatch]);

  assert.equal(textInput.className, 'b');
  assert.equal(textInput.getAttribute('style'), 'color: red;');
  assert.equal(textInput.value, '정글');
  assert.equal(textInput.disabled, true);
  assert.equal(checkbox.checked, false);
});

test('이벤트 : 추가·교체·제거하고 중복 실행되지 않는다', () => {
  const calls = [];
  function handleFirstClick() {
    calls.push('first');
  }
  function handleSecondClick() {
    calls.push('second');
  }
  const rootNode = createRootNode('<div><button>버튼</button></div>');
  const buttonNode = rootNode.firstElementChild;

  // 1) 같은 핸들러를 두 번 넣어도 한 번만 실행된다
  applyPatches(rootNode, [createPropsPatch([0], { onClick: handleFirstClick }, [])]);
  applyPatches(rootNode, [createPropsPatch([0], { onClick: handleFirstClick }, [])]);
  buttonNode.click();
  assert.deepEqual(calls, ['first']);

  // 2) 핸들러를 바꾸면 이전 핸들러는 실행되지 않는다
  applyPatches(rootNode, [createPropsPatch([0], { onClick: handleSecondClick }, [])]);
  buttonNode.click();
  assert.deepEqual(calls, ['first', 'second']);

  // 3) 핸들러를 지우면 아무것도 실행되지 않는다
  applyPatches(rootNode, [createPropsPatch([0], {}, ['onClick'])]);
  buttonNode.click();
  assert.deepEqual(calls, ['first', 'second']);
});

test('경로를 찾을 수 없으면 RangeError를 던진다', () => {
  const rootNode = createRootNode('<div><p></p></div>');
  const removePatch = { type: PATCH_TYPES.REMOVE, path: [3] };
  const insertPatch = { type: PATCH_TYPES.INSERT, path: [], index: 5, node: element('p', {}, []) };
  const expectedError = {
    name: 'RangeError',
    message: '[applyPatches] 경로를 찾을 수 없습니다',
  };

  assert.throws(() => applyPatches(rootNode, [removePatch]), expectedError);
  assert.throws(() => applyPatches(rootNode, [insertPatch]), expectedError);
});

test('알 수 없는 Patch 타입이나 잘못된 인자는 TypeError를 던진다', () => {
  const rootNode = createRootNode('<div></div>');
  const unknownPatch = { type: 'MOVE', path: [] };

  assert.throws(() => applyPatches(rootNode, [unknownPatch]), TypeError);
  assert.throws(() => applyPatches(null, []), TypeError);
  assert.throws(() => applyPatches(rootNode, null), TypeError);
});

test('Patch의 형태가 올바르지 않으면 하나도 적용하지 않고 TypeError를 던진다', () => {
  const rootNode = createRootNode('<ul><li>1</li><li>2</li></ul>');
  const htmlBefore = rootNode.outerHTML;
  const validPatch = { type: PATCH_TYPES.REMOVE, path: [0] };
  const brokenPatches = [
    null,
    { type: PATCH_TYPES.REMOVE },
    { type: PATCH_TYPES.INSERT, path: [], index: 0 },
    { type: PATCH_TYPES.REPLACE, path: [], node: { type: 'element' } },
    { type: PATCH_TYPES.UPDATE_TEXT, path: [0] },
    { type: PATCH_TYPES.UPDATE_PROPS, path: [] },
  ];
  const expectedError = {
    name: 'TypeError',
    message: '[applyPatches] Patch 형태가 올바르지 않습니다',
  };

  for (const brokenPatch of brokenPatches) {
    assert.throws(() => applyPatches(rootNode, [validPatch, brokenPatch]), expectedError);
  }

  // 올바른 Patch가 앞에 있어도, 하나라도 틀리면 DOM은 그대로여야 한다
  assert.equal(rootNode.outerHTML, htmlBefore);
});
