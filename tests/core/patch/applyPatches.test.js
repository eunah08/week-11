import { applyPatches, diff, domToVNode, PATCH_TYPES } from '../../../src/core/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

// 부모가 있는 루트를 만든다. (applyPatches는 루트에 부모가 있어야 한다)
function mountHtml(html) {
  const container = document.createElement('div');

  container.innerHTML = html;
  return container.firstChild;
}

function element(tag, props = {}, children = []) {
  return { type: 'element', tag, props, children };
}

function text(value) {
  return { type: 'text', text: value };
}

function propsPatch(path, set, remove = []) {
  return { type: PATCH_TYPES.UPDATE_PROPS, path, set, remove };
}

// 이전 DOM과 다음 VNode로 diff → applyPatches 후 결과 VNode를 돌려준다.
function patchTo(rootNode, nextVNode) {
  const nextRoot = applyPatches(rootNode, diff(domToVNode(rootNode), nextVNode));

  return { nextRoot, resultVNode: domToVNode(nextRoot) };
}

test('applyPatches : INSERT는 index 위치에 끼워 넣는다', () => {
  const rootNode = mountHtml('<ul><li>A</li><li>C</li></ul>');
  const newNode = element('li', {}, [text('B')]);

  applyPatches(rootNode, [{ type: PATCH_TYPES.INSERT, path: [], index: 1, node: newNode }]);
  assertEqual(rootNode.textContent, 'ABC');
});

test('applyPatches : REMOVE는 해당 노드를 지운다', () => {
  const rootNode = mountHtml('<ul><li>A</li><li>B</li></ul>');

  applyPatches(rootNode, [{ type: PATCH_TYPES.REMOVE, path: [0] }]);
  assertEqual(rootNode.textContent, 'B');
});

test('applyPatches : REPLACE는 노드를 교체한다', () => {
  const rootNode = mountHtml('<div><p>글</p></div>');

  const newNode = element('span', {}, [text('새')]);

  applyPatches(rootNode, [{ type: PATCH_TYPES.REPLACE, path: [0], node: newNode }]);
  assertEqual(rootNode.innerHTML, '<span>새</span>');
});

test('applyPatches : UPDATE_TEXT는 텍스트를 바꾼다', () => {
  const rootNode = mountHtml('<p>전</p>');

  applyPatches(rootNode, [{ type: PATCH_TYPES.UPDATE_TEXT, path: [0], text: '후' }]);
  assertEqual(rootNode.textContent, '후');
});

test('applyPatches : UPDATE_PROPS는 속성 추가·변경·삭제를 한 번에 처리한다', () => {
  const rootNode = mountHtml('<div class="a" id="x"></div>');
  const nextProps = { class: 'b', title: '제목', style: 'color: red' };

  applyPatches(rootNode, [propsPatch([], nextProps, ['id'])]);
  assertEqual(domToVNode(rootNode).props, nextProps);
});

test('applyPatches : value, checked, disabled는 프로퍼티까지 맞춘다', () => {
  const rootNode = mountHtml('<div><input type="checkbox"><input value="전"></div>');
  const checkbox = rootNode.children[0];
  const input = rootNode.children[1];

  input.value = '사용자가 입력함';
  applyPatches(rootNode, [
    propsPatch([0], { checked: true, disabled: true }),
    propsPatch([1], { value: '후' }),
  ]);
  assertTrue(checkbox.checked && checkbox.disabled, 'checked, disabled가 적용되지 않음');
  assertEqual(input.value, '후');

  applyPatches(rootNode, [propsPatch([0], {}, ['checked'])]);
  assertTrue(!checkbox.checked, 'checked가 제거되지 않음');
});

test('applyPatches : 루트가 교체되면 새 루트를 반환한다', () => {
  const rootNode = mountHtml('<div>글</div>');
  const container = rootNode.parentNode;
  const { nextRoot } = patchTo(rootNode, element('section', {}, [text('새')]));

  assertTrue(nextRoot !== rootNode, '새 루트가 아님');
  assertTrue(container.firstChild === nextRoot, '컨테이너에 새 루트가 없음');
  assertEqual(nextRoot.outerHTML, '<section>새</section>');
});

test('applyPatches : 연속된 형제 추가·삭제가 밀리지 않는다', () => {
  const rootNode = mountHtml('<ul><li>1</li><li>2</li><li>3</li><li>4</li></ul>');
  const shortVNode = element('ul', {}, [element('li', {}, [text('1')])]);
  const values = ['1', '2', '3', '4', '5'];
  const longVNode = element('ul', {}, values.map((value) => element('li', {}, [text(value)])));

  assertEqual(patchTo(rootNode, shortVNode).resultVNode, shortVNode);
  assertEqual(patchTo(rootNode, longVNode).resultVNode, longVNode);
});

test('applyPatches : 같은 부모에서 여러 자식이 동시에 바뀐다', () => {
  const rootNode = mountHtml('<div><p>A</p><b>B</b><i>C</i></div>');
  const nextVNode = element('div', {}, [
    element('p', {}, [text('A2')]),
    element('span', {}, [text('B')]),
    text('C'),
  ]);

  assertEqual(patchTo(rootNode, nextVNode).resultVNode, nextVNode);
});

test('applyPatches : 바뀌지 않은 DOM 노드는 그대로 유지한다', () => {
  const rootNode = mountHtml('<div><p>그대로</p><p>바뀜</p></div>');
  const keptNode = rootNode.children[0];
  const changedText = rootNode.children[1].firstChild;

  patchTo(rootNode, element('div', {}, [
    element('p', {}, [text('그대로')]),
    element('p', {}, [text('바꿈')]),
  ]));
  assertTrue(rootNode.children[0] === keptNode, '바뀌지 않은 노드가 교체됨');
  assertTrue(rootNode.children[1].firstChild === changedText, '텍스트 노드가 교체됨');
});

test('applyPatches : 빈 Patch면 DOM을 건드리지 않는다', () => {
  const rootNode = mountHtml('<div><p>글</p></div>');
  const htmlBefore = rootNode.outerHTML;

  assertTrue(applyPatches(rootNode, []) === rootNode, '루트가 바뀜');
  assertEqual(rootNode.outerHTML, htmlBefore);
});

test('applyPatches : path는 주석·공백 텍스트를 건너뛰고 센다', () => {
  const rootNode = mountHtml('<ul>\n  <!-- 주석 -->\n  <li>A</li>\n  <li>B</li>\n</ul>');

  applyPatches(rootNode, [{ type: PATCH_TYPES.UPDATE_TEXT, path: [1, 0], text: 'B2' }]);
  assertEqual(rootNode.children[1].textContent, 'B2');
});

test('이벤트 : 추가·교체·제거 후 중복 실행되지 않는다', () => {
  const rootNode = mountHtml('<button>버튼</button>');
  const calls = [];
  const handleFirst = () => calls.push('first');
  const handleSecond = () => calls.push('second');

  applyPatches(rootNode, [propsPatch([], { onClick: handleFirst })]);
  applyPatches(rootNode, [propsPatch([], { onClick: handleFirst })]);
  rootNode.click();
  assertEqual(calls, ['first'], '같은 핸들러가 중복 등록됨');

  applyPatches(rootNode, [propsPatch([], { onClick: handleSecond })]);
  rootNode.click();
  assertEqual(calls, ['first', 'second'], '이전 리스너가 남음');

  applyPatches(rootNode, [propsPatch([], {}, ['onClick'])]);
  rootNode.click();
  assertEqual(calls, ['first', 'second'], '리스너가 제거되지 않음');
});

test('applyPatches : 알 수 없는 type은 TypeError, 없는 경로는 RangeError', () => {
  const rootNode = mountHtml('<div><p>글</p></div>');

  const unknownPatch = { type: 'MOVE', path: [] };

  assertThrows(() => applyPatches(rootNode, [unknownPatch]), TypeError, '[applyPatches]');
  assertThrows(
    () => applyPatches(rootNode, [{ type: PATCH_TYPES.REMOVE, path: [5] }]),
    RangeError,
    '[applyPatches] 경로를 찾을 수 없습니다',
  );
});
