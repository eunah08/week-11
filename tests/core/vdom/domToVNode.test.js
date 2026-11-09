import { domToVNode } from '../../../src/core/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

function createDomNode(html) {
  const template = document.createElement('template');

  template.innerHTML = html;
  return template.content.firstChild;
}

test('domToVNode : 요소, 텍스트, 속성, 중첩 구조를 변환한다', () => {
  const domNode = createDomNode('<DIV class="box" id="main"><p>안녕 <b>굵게</b></p></DIV>');

  assertEqual(domToVNode(domNode), {
    type: 'element',
    tag: 'div',
    props: { class: 'box', id: 'main' },
    children: [{
      type: 'element',
      tag: 'p',
      props: {},
      children: [
        { type: 'text', text: '안녕 ' },
        { type: 'element', tag: 'b', props: {}, children: [{ type: 'text', text: '굵게' }] },
      ],
    }],
  });
});

test('domToVNode : 주석, 공백 텍스트, script, style은 무시한다', () => {
  const html = '<div>\n  <!-- 주석 --><script>1</script><style>p{}</style><i>남음</i>\n</div>';
  const domNode = createDomNode(html);

  assertEqual(domToVNode(domNode).children, [
    { type: 'element', tag: 'i', props: {}, children: [{ type: 'text', text: '남음' }] },
  ]);
  assertEqual(domToVNode(document.createComment('x')), null);
  assertEqual(domToVNode(document.createTextNode('   ')), null);
  assertEqual(domToVNode(document.createTextNode('')), null);
});

test('domToVNode : 탭·줄바꿈만 있는 텍스트도 공백으로 보고 무시한다', () => {
  assertEqual(domToVNode(document.createTextNode('\n\t  \r\n')), null);
});

test('domToVNode : &nbsp;만 있는 텍스트는 내용으로 남긴다', () => {
  const domNode = createDomNode('<p><b>가</b>&nbsp;<i>나</i></p>');

  assertEqual(domToVNode(domNode).children[1], { type: 'text', text: ' ' });
});

test('domToVNode : 텍스트 앞뒤 공백은 그대로 둔다', () => {
  assertEqual(domToVNode(document.createTextNode('  글자 ')), { type: 'text', text: '  글자 ' });
});

test('domToVNode : 자식이 없는 요소는 빈 children을 가진다', () => {
  assertEqual(domToVNode(createDomNode('<span></span>')).children, []);
});

test('domToVNode : 특수 속성(checked, disabled, value, style)도 읽는다', () => {
  const html = '<input type="checkbox" checked disabled value="a" style="color: red">';
  const domNode = createDomNode(html);

  assertEqual(domToVNode(domNode).props, {
    type: 'checkbox',
    checked: '',
    disabled: '',
    value: 'a',
    style: 'color: red',
  });
});

test('domToVNode : DOM을 읽기만 한다', () => {
  const domNode = createDomNode('<div><!-- c --> <p>글</p></div>');
  const htmlBefore = domNode.outerHTML;

  domToVNode(domNode);
  assertTrue(domNode.outerHTML === htmlBefore, 'DOM이 바뀌었다');
});

test('domToVNode : Node가 아니면 TypeError', () => {
  assertThrows(() => domToVNode({}), TypeError, '[domToVNode] Node가 필요합니다');
  assertThrows(() => domToVNode(null), TypeError, '[domToVNode] Node가 필요합니다');
});
