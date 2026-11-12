import { diff, isVNode, PATCH_TYPES } from '../../../src/core/index.js';
import { test, assertEqual, assertTrue } from '../../testRunner.js';

function text(value) {
  return { type: 'text', text: value };
}

function element(tag, props, children) {
  return { type: 'element', tag, props, children };
}

// props와 children을 읽은 횟수를 세는 VNode. diff가 안쪽을 들여다봤는지 알아내려고 쓴다.
function createCountingVNode(counter) {
  const vnode = { type: 'element', tag: 'li' };

  Object.defineProperty(vnode, 'props', {
    enumerable: true,
    get() {
      counter.readCount += 1;
      return {};
    },
  });
  Object.defineProperty(vnode, 'children', {
    enumerable: true,
    get() {
      counter.readCount += 1;
      return [text('같음')];
    },
  });

  return vnode;
}

// Selective Rendering
test('diff : 이전과 다음이 같은 VNode 객체면 빈 Patch이다', () => {
  const vnode = element('p', { class: 'a' }, [text('글')]);

  assertEqual(diff(vnode, vnode), []);
});

// Selective Rendering
test('diff : 같은 객체인 자식은 안쪽을 비교하지 않고 바뀐 곳의 Patch만 만든다', () => {
  const counter = { readCount: 0 };
  const sharedChild = createCountingVNode(counter);
  const previousVNode = element('ul', {}, [sharedChild, text('전')]);
  const nextVNode = element('ul', {}, [sharedChild, text('후')]);

  // 입력 검증(isVNode)이 읽는 횟수를 먼저 재 둔다. diff가 이보다 더 읽으면 안쪽을 비교한 것이다.
  isVNode(previousVNode);
  isVNode(nextVNode);

  const validationReadCount = counter.readCount;

  counter.readCount = 0;

  const patches = diff(previousVNode, nextVNode);

  assertEqual(patches, [{ type: PATCH_TYPES.UPDATE_TEXT, path: [1], text: '후' }]);
  assertEqual(counter.readCount, validationReadCount, '같은 객체인 자식의 안쪽을 비교함');
});

// Selective Rendering
test('diff : 내용은 같아도 다른 객체면 이전처럼 안쪽을 비교해 바뀐 곳을 찾는다', () => {
  const previousVNode = element('p', { class: 'a' }, [text('글')]);
  const nextVNode = element('p', { class: 'b' }, [text('글')]);

  assertTrue(previousVNode !== nextVNode, '준비');
  assertEqual(diff(previousVNode, nextVNode), [
    { type: PATCH_TYPES.UPDATE_PROPS, path: [], set: { class: 'b' }, remove: [] },
  ]);
});
