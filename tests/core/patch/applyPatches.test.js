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

// 형제 목록을 기억해 두는 방식이, patch마다 목록을 새로 세는 방식과 같은 결과를 내는지 확인한다.
// patch를 하나씩 따로 applyPatches에 넘기면 매번 새로 세게 된다.
const SAME_RESULT_CASES = [
  {
    name: '삭제·교체·삽입이 섞인 목록',
    html: '<ul>\n  <li>A</li><!-- 주석 --><li>B</li>\n  <li>C</li><li>D</li></ul>',
    patches: [
      { type: PATCH_TYPES.REMOVE, path: [3] },
      { type: PATCH_TYPES.REPLACE, path: [0], node: element('p', {}, [text('A2')]) },
      { type: PATCH_TYPES.INSERT, path: [], index: 1, node: element('li', {}, [text('새')]) },
      { type: PATCH_TYPES.UPDATE_TEXT, path: [1, 0], text: '새2' },
      { type: PATCH_TYPES.REMOVE, path: [0] },
      propsPatch([1], { class: '끝' }),
    ],
  },
  {
    // 무시하는 노드(공백 텍스트·script)가 새로 생기면 뒤 형제의 번호가 당겨진다.
    name: '무시하는 노드가 새로 생기는 경우',
    html: '<div>A<b>B</b><i>C</i></div>',
    patches: [
      { type: PATCH_TYPES.UPDATE_TEXT, path: [0], text: '  ' },
      propsPatch([0], { class: 'x' }),
      { type: PATCH_TYPES.REPLACE, path: [1], node: text('\n') },
      { type: PATCH_TYPES.INSERT, path: [], index: 1, node: element('script', {}, []) },
      { type: PATCH_TYPES.INSERT, path: [], index: 1, node: element('u', {}, []) },
      propsPatch([1], { title: 't' }),
      // 요소에 UPDATE_TEXT를 하면 자식이 통째로 바뀐다.
      { type: PATCH_TYPES.UPDATE_TEXT, path: [0], text: 'B2' },
      { type: PATCH_TYPES.INSERT, path: [0], index: 1, node: element('s', {}, [text('끝')]) },
    ],
  },
  {
    // output의 value를 바꾸면 브라우저가 자식을 글자 하나로 갈아 끼운다.
    name: '속성 변경이 자식을 바꾸는 경우',
    html: '<div><output><b>a</b></output></div>',
    patches: [
      { type: PATCH_TYPES.UPDATE_TEXT, path: [0, 0], text: 'a2' },
      propsPatch([0], { value: 'hi' }),
      { type: PATCH_TYPES.UPDATE_TEXT, path: [0, 0], text: 'z' },
    ],
  },
];

for (const { name, html, patches } of SAME_RESULT_CASES) {
  test(`applyPatches : 한 번에 적용한 결과가 patch를 하나씩 적용한 결과와 같다 (${name})`, () => {
    const allAtOnce = mountHtml(html);
    const oneByOne = mountHtml(html);

    applyPatches(allAtOnce, patches);
    for (const patch of patches) {
      applyPatches(oneByOne, [patch]);
    }

    assertEqual(allAtOnce.outerHTML, oneByOne.outerHTML);
  });
}

test('applyPatches : 같은 부모 아래 patch 5,000개도 1초 안에 적용한다', () => {
  const count = 5000;
  const list = (label) => element('ul', {}, Array.from({ length: count }, (_, i) => element('li', {}, [text(`${label} ${i}`)])));
  const rootNode = mountHtml('<ul></ul>');
  const fullRoot = applyPatches(rootNode, [{ type: PATCH_TYPES.REPLACE, path: [], node: list('전') }]);
  const startedAt = performance.now();
  const { resultVNode } = patchTo(fullRoot, list('후'));
  const elapsedMs = performance.now() - startedAt;

  assertEqual(resultVNode, list('후'));
  assertTrue(elapsedMs < 1000, `너무 느림: ${elapsedMs.toFixed(0)}ms`);
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
  // 객체가 원래 가진 이름(constructor 등)이나 객체가 아닌 Patch도 알 수 없는 type으로 본다.
  assertThrows(
    () => applyPatches(rootNode, [{ type: 'constructor', path: [] }]),
    TypeError,
    '[applyPatches] 알 수 없는 Patch 타입입니다',
  );
  assertThrows(() => applyPatches(rootNode, [null]), TypeError, '[applyPatches]');
  assertThrows(
    () => applyPatches(rootNode, [{ type: PATCH_TYPES.REMOVE, path: [5] }]),
    RangeError,
    '[applyPatches] 경로를 찾을 수 없습니다',
  );
});

test('applyPatches : 음수·문자열 index나 path는 맨 뒤에 붙이지 않고 RangeError', () => {
  const rootNode = mountHtml('<ul><li>A</li></ul>');
  const node = element('li', {}, [text('B')]);
  const message = '[applyPatches] 경로를 찾을 수 없습니다';

  assertThrows(
    () => applyPatches(rootNode, [{ type: PATCH_TYPES.INSERT, path: [], index: -1, node }]),
    RangeError,
    message,
  );
  assertThrows(
    () => applyPatches(rootNode, [{ type: PATCH_TYPES.REMOVE, path: ['length'] }]),
    RangeError,
    message,
  );
  assertEqual(rootNode.textContent, 'A');
});

test('applyPatches : Patch 형태가 틀리면 DOM을 바꾸기 전에 TypeError', () => {
  const rootNode = mountHtml('<ul><li>A</li></ul>');
  const message = '[applyPatches] Patch 형태가 올바르지 않습니다';
  const validPatch = { type: PATCH_TYPES.UPDATE_TEXT, path: [0, 0], text: 'B' };

  assertThrows(() => applyPatches(rootNode, [validPatch, { type: PATCH_TYPES.UPDATE_TEXT, path: 'x', text: 'C' }]), TypeError, message);
  assertThrows(() => applyPatches(rootNode, [validPatch, { type: PATCH_TYPES.INSERT, path: [], index: 1, node: { type: 'text' } }]), TypeError, message);
  assertThrows(() => applyPatches(rootNode, [validPatch, { type: PATCH_TYPES.UPDATE_PROPS, path: [], set: null, remove: [] }]), TypeError, message);
  assertEqual(rootNode.textContent, 'A');
});

test('applyPatches : 알 수 없는 Patch 타입이 뒤에 있어도 앞 Patch를 적용하지 않는다', () => {
  const rootNode = mountHtml('<p>A</p>');
  const patches = [{ type: PATCH_TYPES.UPDATE_TEXT, path: [0], text: 'B' }, { type: 'MOVE', path: [] }];

  assertThrows(() => applyPatches(rootNode, patches), TypeError, '[applyPatches] 알 수 없는 Patch 타입입니다');
  assertEqual(rootNode.textContent, 'A');
});

test('applyPatches : option을 추가하는 Patch와 select value 변경이 함께 와도 value가 유지된다', () => {
  const rootNode = mountHtml('<select value="a"><option value="a">a</option></select>');
  const nextVNode = element('select', { value: 'b' }, [
    element('option', { value: 'a' }, [text('a')]),
    element('option', { value: 'b' }, [text('b')]),
  ]);
  const { nextRoot } = patchTo(rootNode, nextVNode);

  assertEqual(nextRoot.value, 'b');
});
