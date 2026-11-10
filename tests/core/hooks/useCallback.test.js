import {
  createElement,
  useState,
  useCallback,
  FunctionComponent,
} from '../../../src/core/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

// 상태 count를 가진 루트를 만들고, 렌더링마다 useCallbackCall이 돌려준 함수를 모은다.
function mountRoot(useCallbackCall) {
  const handles = { setCount: null, instance: null, callbacks: [] };
  const root = () => {
    const [count, setCount] = useState(0);

    handles.setCount = setCount;
    handles.callbacks.push(useCallbackCall(count));
    return createElement('p', null, count);
  };

  handles.instance = new FunctionComponent(root, {}, document.createElement('div'));
  handles.instance.mount();
  return handles;
}

function change(handles, value) {
  handles.setCount(value);
  handles.instance.flushUpdate();
}

// Memoization
test('useCallback : 의존성이 같으면 같은 함수를 돌려준다', () => {
  const handles = mountRoot(() => useCallback(() => 1, []));

  change(handles, 1);
  change(handles, 2);
  assertTrue(handles.callbacks.every((callback) => callback === handles.callbacks[0]), '함수가 바뀜');
});

// Memoization
test('useCallback : 의존성이 바뀌면 새로 받은 함수로 바꾼다', () => {
  const handles = mountRoot((count) => useCallback(() => count, [count]));

  change(handles, 1);
  assertTrue(handles.callbacks[0] !== handles.callbacks[1], '함수가 그대로임');
  assertEqual(handles.callbacks[1](), 1);
});

// Memoization
test('useCallback : 의존성을 생략하면 렌더링마다 새 함수를 쓴다', () => {
  const handles = mountRoot(() => useCallback(() => 1));

  change(handles, 1);
  assertTrue(handles.callbacks[0] !== handles.callbacks[1], '함수가 그대로임');
});

// Memoization
test('useCallback : 콜백이 함수가 아니면 TypeError', () => {
  assertThrows(() => useCallback('x', []), TypeError, '[useCallback] 콜백 함수가 필요합니다');
});

// Memoization
test('useCallback : 렌더링 밖에서 호출하면 Error', () => {
  assertThrows(
    () => useCallback(() => {}, []),
    Error,
    '[useCallback] 루트 컴포넌트 렌더링 중에만 호출할 수 있습니다',
  );
});
