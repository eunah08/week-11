import { test } from 'node:test';
import assert from 'node:assert/strict';
import { JSDOM } from 'jsdom';
import { domToVNode, NODE_TYPES } from '../../../src/core/index.js';

// Node.js에는 브라우저 DOM이 없으므로 jsdom으로 만든 DOM을 전역에 넣는다
const jsdom = new JSDOM('');
globalThis.document = jsdom.window.document;
globalThis.Node = jsdom.window.Node;

function createRootNode(html) { // HTML 문자열을 DOM으로 만들고 첫 요소를 돌려준다
  const container = document.createElement('div');
  container.innerHTML = html;
  return container.firstElementChild;
}

test('요소·텍스트·속성·중첩 구조를 변환한다', () => {
  const rootNode = createRootNode('<DIV class="box" id="main"><p>안녕<b>하세요</b></p></DIV>');

  const vnode = domToVNode(rootNode);

  const expectedVNode = {
    type: NODE_TYPES.ELEMENT,
    tag: 'div',
    props: { class: 'box', id: 'main' },
    children: [
      {
        type: NODE_TYPES.ELEMENT,
        tag: 'p',
        props: {},
        children: [
          { type: NODE_TYPES.TEXT, text: '안녕' },
          {
            type: NODE_TYPES.ELEMENT,
            tag: 'b',
            props: {},
            children: [{ type: NODE_TYPES.TEXT, text: '하세요' }],
          },
        ],
      },
    ],
  };
  assert.deepEqual(vnode, expectedVNode);
});

test('주석과 공백만 있는 텍스트는 무시한다', () => {
  const rootNode = createRootNode('<ul>\n  <!-- 주석 -->\n  <li>하나</li>\n  \n</ul>');

  const vnode = domToVNode(rootNode);

  assert.equal(vnode.children.length, 1);
  assert.equal(vnode.children[0].tag, 'li');

  const commentNode = document.createComment('주석');
  const spaceTextNode = document.createTextNode('  \n ');
  const emptyTextNode = document.createTextNode('');
  assert.equal(domToVNode(commentNode), null);
  assert.equal(domToVNode(spaceTextNode), null);
  assert.equal(domToVNode(emptyTextNode), null);
});

test('텍스트 앞뒤 공백은 그대로 둔다', () => {
  const textNode = document.createTextNode('  글자 ');

  const vnode = domToVNode(textNode);

  assert.deepEqual(vnode, { type: NODE_TYPES.TEXT, text: '  글자 ' });
});

test('텍스트 노드만 있는 트리도 변환한다', () => {
  const rootNode = createRootNode('<p>글자만 있음</p>');

  const vnode = domToVNode(rootNode);

  assert.deepEqual(vnode.children, [{ type: NODE_TYPES.TEXT, text: '글자만 있음' }]);
});

test('script와 style 태그는 null로 처리한다', () => {
  const rootNode = createRootNode('<div><script>1</script><style>p{}</style><span>남음</span></div>');

  const vnode = domToVNode(rootNode);

  assert.equal(vnode.children.length, 1);
  assert.equal(vnode.children[0].tag, 'span');

  const scriptNode = document.createElement('script');
  assert.equal(domToVNode(scriptNode), null);
});

test('자식이 없는 요소는 빈 children 배열을 가진다', () => {
  const sectionNode = document.createElement('section');

  const vnode = domToVNode(sectionNode);

  const expectedVNode = {
    type: NODE_TYPES.ELEMENT,
    tag: 'section',
    props: {},
    children: [],
  };
  assert.deepEqual(vnode, expectedVNode);
});

test('Node가 아니면 TypeError를 던진다', () => {
  const expectedError = {
    name: 'TypeError',
    message: '[domToVNode] Node가 필요합니다',
  };

  assert.throws(() => domToVNode({}), expectedError);
  assert.throws(() => domToVNode(null), expectedError);
});
