import {
  createElement,
  useState,
  useDebouncedValue,
  FunctionComponent,
} from '../../../src/core/index.js';
import { installFakeTimers } from '../../fakeTimers.js';
import { test, assertEqual, assertThrows } from '../../testRunner.js';

// 가짜 타이머를 설치하고 값 value, 지연 delayMs를 바꿀 수 있는 루트를 mount해서 테스트에 넘긴다.
function withDebouncedRoot(runTest) {
  const timers = installFakeTimers();
  const handles = {
    setValue: null,
    setDelayMs: null,
    seenValues: [],
    instance: null,
    timers,
  };
  const root = () => {
    const [value, setValue] = useState('a');
    const [delayMs, setDelayMs] = useState(300);

    handles.setValue = setValue;
    handles.setDelayMs = setDelayMs;
    handles.seenValues.push(useDebouncedValue(value, delayMs));
    return createElement('p', null, value);
  };

  handles.instance = new FunctionComponent(root, {}, document.createElement('div'));
  handles.instance.mount();

  try {
    runTest(handles);
  } finally {
    timers.uninstall();
  }
}

function change(handles, setterName, value) {
  handles[setterName](value);
  handles.instance.flushUpdate();
}

// Debouncing
test('useDebouncedValue : 처음에는 받은 값을 그대로 돌려준다', () => {
  withDebouncedRoot((handles) => {
    assertEqual(handles.seenValues, ['a']);
  });
});

// Debouncing
test('useDebouncedValue : 값이 바뀌어도 지연 시간이 지나기 전에는 이전 값을 돌려준다', () => {
  withDebouncedRoot((handles) => {
    change(handles, 'setValue', 'b');
    assertEqual(handles.seenValues, ['a', 'a']);
    assertEqual(handles.timers.getPendingDelays(), [300]);
  });
});

// Debouncing
test('useDebouncedValue : 지연 시간이 지나면 바뀐 값을 돌려준다', () => {
  withDebouncedRoot((handles) => {
    change(handles, 'setValue', 'b');
    handles.timers.runPendingTimers();
    handles.instance.flushUpdate();

    assertEqual(handles.seenValues.at(-1), 'b');
  });
});

// Debouncing
test('useDebouncedValue : 계속 바뀌는 동안에는 이전 타이머를 지우고 마지막 값만 반영한다', () => {
  withDebouncedRoot((handles) => {
    change(handles, 'setValue', 'b');
    change(handles, 'setValue', 'c');
    change(handles, 'setValue', 'd');
    assertEqual(handles.timers.getPendingDelays(), [300], '이전 타이머가 남아 있음');
    assertEqual(handles.seenValues.at(-1), 'a');

    handles.timers.runPendingTimers();
    handles.instance.flushUpdate();
    assertEqual(handles.seenValues.at(-1), 'd');
    assertEqual(handles.seenValues.includes('b') || handles.seenValues.includes('c'), false);
  });
});

// Debouncing
test('useDebouncedValue : 값이 반영된 뒤에는 새 타이머를 걸지 않는다', () => {
  withDebouncedRoot((handles) => {
    change(handles, 'setValue', 'b');
    handles.timers.runPendingTimers();
    handles.instance.flushUpdate();

    assertEqual(handles.timers.getPendingDelays(), []);
  });
});

// Debouncing
test('useDebouncedValue : 지연 시간이 바뀌면 새 지연 시간으로 타이머를 다시 건다', () => {
  withDebouncedRoot((handles) => {
    change(handles, 'setValue', 'b');
    change(handles, 'setDelayMs', 50);
    assertEqual(handles.timers.getPendingDelays(), [50]);
  });
});

// Debouncing
test('useDebouncedValue : 해제하면 남은 타이머를 지운다', () => {
  withDebouncedRoot((handles) => {
    change(handles, 'setValue', 'b');
    handles.instance.unmount();
    assertEqual(handles.timers.getPendingDelays(), []);
  });
});

// Debouncing
test('useDebouncedValue : 지연 시간이 0 이상의 숫자가 아니면 TypeError', () => {
  const message = '[useDebouncedValue] 지연 시간은 0 이상의 숫자여야 합니다';

  assertThrows(() => useDebouncedValue('a', -1), TypeError, message);
  assertThrows(() => useDebouncedValue('a', '300'), TypeError, message);
  assertThrows(() => useDebouncedValue('a', NaN), TypeError, message);
  assertThrows(() => useDebouncedValue('a'), TypeError, message);
});

// Debouncing
test('useDebouncedValue : 렌더링 밖에서 호출하면 Error', () => {
  assertThrows(
    () => useDebouncedValue('a', 300),
    Error,
    '루트 컴포넌트 렌더링 중에만 호출할 수 있습니다',
  );
});
