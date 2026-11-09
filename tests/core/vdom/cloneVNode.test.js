import { cloneVNode } from '../../../src/core/index.js';
import { test, assertEqual, assertTrue } from '../../testRunner.js';

const original = {
  type: 'element',
  tag: 'ul',
  props: { class: 'list' },
  children: [{ type: 'element', tag: 'li', props: {}, children: [{ type: 'text', text: '하나' }] }],
};

test('cloneVNode : 같은 구조의 복사본을 만든다', () => {
  assertEqual(cloneVNode(original), original);
});

test('cloneVNode : 복사본을 고쳐도 원본은 그대로다', () => {
  const copy = cloneVNode(original);

  copy.props.class = 'changed';
  copy.children[0].children[0].text = '바뀜';

  assertEqual(original.props.class, 'list');
  assertEqual(original.children[0].children[0].text, '하나');
  assertTrue(copy !== original && copy.children[0] !== original.children[0], '객체를 공유함');
});

test('cloneVNode : 함수 props는 같은 함수를 그대로 옮긴다', () => {
  const handler = () => {};
  const copy = cloneVNode({ type: 'element', tag: 'button', props: { onClick: handler }, children: [] });

  assertTrue(copy.props.onClick === handler, '함수가 바뀜');
});
