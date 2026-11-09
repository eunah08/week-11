import { StateHistory } from '../../../src/demos/phase_one/StateHistory.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

function text(value) {
  return { type: 'text', text: value };
}

test('StateHistory : push 후 back, forward로 이동한다', () => {
  const history = new StateHistory(text('1'));

  history.push(text('2'));
  history.push(text('3'));
  assertEqual(history.back(), text('2'));
  assertEqual(history.back(), text('1'));
  assertEqual(history.forward(), text('2'));
  assertEqual(history.getCurrent(), text('2'));
});

test('StateHistory : 끝에 닿으면 null이고 버튼 판단 값이 false다', () => {
  const history = new StateHistory(text('1'));

  assertTrue(!history.canGoBack() && !history.canGoForward(), '처음에 이동할 수 있다고 나옴');
  assertEqual(history.back(), null);
  assertEqual(history.forward(), null);
  assertEqual(history.getCurrent(), text('1'));

  history.push(text('2'));
  assertTrue(history.canGoBack() && !history.canGoForward(), '끝에서의 판단 값이 틀림');
});

test('StateHistory : 뒤로 간 뒤 push하면 이후 이력을 제거한다', () => {
  const history = new StateHistory(text('1'));

  history.push(text('2'));
  history.push(text('3'));
  history.back();
  history.back();
  history.push(text('새'));
  assertTrue(!history.canGoForward(), '앞으로 가기 기록이 남음');
  assertEqual(history.getCount(), 2);
  assertEqual(history.back(), text('1'));
});

test('StateHistory : 저장한 VNode를 바꾸지 않고 그대로 돌려준다', () => {
  const firstVNode = text('1');
  const history = new StateHistory(firstVNode);

  history.push(text('2'));
  assertTrue(history.back() === firstVNode, '저장한 VNode가 다른 객체가 됨');
  assertEqual(firstVNode, text('1'));
});

test('StateHistory : VNode가 아니면 TypeError', () => {
  assertThrows(() => new StateHistory(null), TypeError, '[StateHistory] VNode가 필요합니다');
  assertThrows(() => new StateHistory({ type: 'x' }), TypeError, '[StateHistory] VNode가 필요합니다');
  // diff와 같은 기준 : 요소는 props와 children이 있어야 VNode다.
  assertThrows(
    () => new StateHistory({ type: 'element', tag: 'div' }),
    TypeError,
    '[StateHistory] VNode가 필요합니다',
  );
});

test('StateHistory : 기록과 위치는 바깥에서 직접 바꿀 수 없다', () => {
  const history = new StateHistory(text('1'));

  assertEqual(history.vnodes, undefined);
  assertEqual(history.currentIndex, undefined);
});
