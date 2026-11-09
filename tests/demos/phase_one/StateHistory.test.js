import { test } from 'node:test';
import assert from 'node:assert/strict';
import { StateHistory } from '../../../src/demos/phase_one/StateHistory.js';

function text(value) { // 테스트용 텍스트 VNode를 만든다
  return { type: 'text', text: value };
}

test('뒤로·앞으로 이동하며 VNode를 돌려준다', () => {
  const history = new StateHistory(text('1'));
  history.push(text('2'));
  history.push(text('3'));

  assert.deepEqual(history.back(), text('2'));
  assert.deepEqual(history.back(), text('1'));
  assert.deepEqual(history.forward(), text('2'));
  assert.deepEqual(history.getCurrent(), text('2'));
});

test('끝에 닿으면 null을 돌려주고 위치를 바꾸지 않는다', () => {
  const history = new StateHistory(text('1'));

  assert.equal(history.canGoBack(), false);
  assert.equal(history.canGoForward(), false);
  assert.equal(history.back(), null);
  assert.equal(history.forward(), null);
  assert.deepEqual(history.getCurrent(), text('1'));
});

test('뒤로 간 뒤 push하면 이후 기록을 버린다', () => {
  const history = new StateHistory(text('1'));
  history.push(text('2'));
  history.push(text('3'));
  history.back();
  history.back();

  history.push(text('새 기록'));

  assert.equal(history.canGoForward(), false);
  assert.deepEqual(history.getPosition(), { index: 1, count: 2 });
  assert.deepEqual(history.back(), text('1'));
});

test('저장한 VNode를 그대로(같은 참조로) 돌려준다', () => {
  const initialVNode = text('1');

  const history = new StateHistory(initialVNode);

  assert.equal(history.getCurrent(), initialVNode);
});

test('VNode가 아니면 TypeError를 던진다', () => {
  const expectedError = {
    name: 'TypeError',
    message: '[StateHistory] VNode가 필요합니다',
  };
  const history = new StateHistory(text('1'));

  assert.throws(() => new StateHistory(null), expectedError);
  assert.throws(() => history.push({ type: 'comment' }), expectedError);
  assert.throws(() => new StateHistory({ type: 'element' }), expectedError);
  assert.throws(() => history.push({ type: 'text' }), expectedError);
});
