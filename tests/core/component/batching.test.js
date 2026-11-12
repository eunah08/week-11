import { createElement, useState, FunctionComponent } from '../../../src/core/index.js';
import { test, assertEqual, assertThrows } from '../../testRunner.js';

// queueMicrotask를 가짜로 바꿔 "예약된 callback"을 모아 두고, 테스트가 원하는 때 실행한다.
function withStubbedMicrotask(runTest) {
  const originalQueueMicrotask = globalThis.queueMicrotask;
  const queuedCallbacks = [];

  globalThis.queueMicrotask = (callback) => queuedCallbacks.push(callback);

  try {
    runTest(queuedCallbacks);
  } finally {
    globalThis.queueMicrotask = originalQueueMicrotask;
  }
}

// 상태 두 개를 가진 루트. 렌더링 횟수와 상태 변경 함수를 handles로 꺼내 쓴다.
function createCounterRoot() {
  const handles = { renderCount: 0, setCount: null, setLabel: null };
  const root = () => {
    const [count, setCount] = useState(0);
    const [label, setLabel] = useState('a');

    handles.renderCount += 1;
    handles.setCount = setCount;
    handles.setLabel = setLabel;
    return createElement('p', null, `${label}${count}`);
  };

  return { root, handles };
}

function mountRoot(root) {
  const container = document.createElement('div');
  const instance = new FunctionComponent(root, {}, container);

  instance.mount();
  return { container, instance };
}

// Batching
test('Batching : 같은 틱에서 여러 번 바꿔도 microtask는 한 번만 예약하고 렌더링도 한 번이다', () => {
  withStubbedMicrotask((queuedCallbacks) => {
    const { root, handles } = createCounterRoot();
    const { container } = mountRoot(root);

    handles.setCount(1);
    handles.setCount(2);
    handles.setLabel('b');
    handles.setCount(3);

    assertEqual(queuedCallbacks.length, 1, 'microtask가 여러 번 예약됨');
    assertEqual(handles.renderCount, 1, '예약 전에 렌더링됨');
    assertEqual(container.textContent, 'a0', '예약 전에 화면이 바뀜');

    queuedCallbacks[0]();
    assertEqual(handles.renderCount, 2);
    assertEqual(container.textContent, 'b3');
  });
});

// Batching
test('Batching : 함수형 갱신은 쌓인 최신 값을 이어받고 렌더링은 한 번이다', () => {
  withStubbedMicrotask((queuedCallbacks) => {
    const { root, handles } = createCounterRoot();
    const { container, instance } = mountRoot(root);

    handles.setCount((previous) => previous + 1);
    handles.setCount((previous) => previous + 1);
    handles.setCount((previous) => previous * 10);
    instance.flushUpdate();

    assertEqual(container.textContent, 'a20');
    assertEqual(handles.renderCount, 2);
    assertEqual(queuedCallbacks.length, 1);
  });
});

// Batching
test('Batching : 같은 값으로 바꾸면 갱신을 예약하지 않는다', () => {
  withStubbedMicrotask((queuedCallbacks) => {
    const { root, handles } = createCounterRoot();

    mountRoot(root);
    handles.setCount(0);
    handles.setLabel('a');
    assertEqual(queuedCallbacks.length, 0);
  });
});

// Batching
test('Batching : flushUpdate로 먼저 실행하면 예약된 microtask는 아무 일도 하지 않는다', () => {
  withStubbedMicrotask((queuedCallbacks) => {
    const { root, handles } = createCounterRoot();
    const { instance } = mountRoot(root);

    handles.setCount(1);
    instance.flushUpdate();
    queuedCallbacks[0]();
    assertEqual(handles.renderCount, 2, '두 번 렌더링됨');

    instance.flushUpdate();
    assertEqual(handles.renderCount, 2, '예약이 없는데 렌더링됨');
  });
});

// Batching
test('Batching : 한 번 처리한 뒤에 바꾸면 다시 예약한다', () => {
  withStubbedMicrotask((queuedCallbacks) => {
    const { root, handles } = createCounterRoot();
    const { container, instance } = mountRoot(root);

    handles.setCount(1);
    instance.flushUpdate();
    handles.setCount(2);
    assertEqual(queuedCallbacks.length, 2);
    instance.flushUpdate();
    assertEqual(container.textContent, 'a2');
  });
});

// Batching
test('Batching : 예약한 뒤 unmount하면 갱신하지 않는다', () => {
  withStubbedMicrotask((queuedCallbacks) => {
    const { root, handles } = createCounterRoot();
    const { container, instance } = mountRoot(root);

    handles.setCount(1);
    instance.unmount();
    queuedCallbacks[0]();

    assertEqual(handles.renderCount, 1);
    assertEqual(container.childNodes.length, 0);
  });
});

// Batching
test('Batching : 예약된 갱신에서 난 에러는 onError로 넘기고, 없으면 그대로 던진다', () => {
  withStubbedMicrotask(() => {
    let shouldBreak = false;
    const handles = { setValue: null };
    const root = () => {
      const [value, setValue] = useState(0);

      handles.setValue = setValue;
      return shouldBreak ? '글자' : createElement('p', null, value);
    };
    const { instance } = mountRoot(root);
    const errors = [];

    shouldBreak = true;
    instance.onError = (error) => errors.push(error.message);
    handles.setValue(1);
    instance.flushUpdate();
    assertEqual(errors, ['[FunctionComponent] 루트는 element VNode여야 합니다']);

    instance.onError = null;
    handles.setValue(2);
    assertThrows(() => instance.flushUpdate(), TypeError, '루트는 element VNode여야 합니다');
  });
});
