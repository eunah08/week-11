import {
  createElement,
  useState,
  useEffect,
  useMemo,
  FunctionComponent,
} from '../../../src/core/index.js';
import { test, assertEqual, assertThrows } from '../../testRunner.js';
import { isDepsChanged } from '../../../src/core/hooks/isDepsChanged.js';

function mountRoot(rootComponent) {
  const container = document.createElement('div');
  const instance = new FunctionComponent(rootComponent, {}, container);

  document.body.append(container);
  instance.mount();
  return instance;
}

// 같은 칸에 렌더링마다 다른 Hook을 부르는 루트. 조건문 안의 Hook이 만드는 문제를 흉내 낸다.
function createRootWithSwitchingHook(firstHook, secondHook) {
  const control = { useFirst: true };
  const root = () => {
    if (control.useFirst) {
      firstHook();
    } else {
      secondHook();
    }

    return createElement('p', null);
  };

  return { root, control };
}

test('Hook 순서 : 같은 칸의 Hook이 state에서 effect로 바뀌면 Error', () => {
  const { root, control } = createRootWithSwitchingHook(
    () => useState(0),
    () => useEffect(() => {}, []),
  );
  const instance = mountRoot(root);

  control.useFirst = false;
  assertThrows(() => instance.update(), Error, '[useEffect] Hook 호출 순서가 바뀌었습니다');
});

test('Hook 순서 : 같은 칸의 Hook이 effect에서 memo로 바뀌면 Error', () => {
  const { root, control } = createRootWithSwitchingHook(
    () => useEffect(() => {}, []),
    () => useMemo(() => 1, []),
  );
  const instance = mountRoot(root);

  control.useFirst = false;
  assertThrows(() => instance.update(), Error, '[useMemo] Hook 호출 순서가 바뀌었습니다');
});

test('Hook 순서 : 조건문 안의 Hook 때문에 칸이 밀리면 Error', () => {
  let shouldUseFirst = true;
  const root = () => {
    if (shouldUseFirst) {
      useState('조건부');
    }

    useMemo(() => 1, []);
    return createElement('p', null);
  };
  const instance = mountRoot(root);

  shouldUseFirst = false;
  assertThrows(() => instance.update(), Error, '[useMemo] Hook 호출 순서가 바뀌었습니다');
});

test('isDepsChanged : 이전 의존성이 undefined이면 바뀐 것이다', () => {
  assertEqual(isDepsChanged(undefined, []), true);
  assertEqual(isDepsChanged(undefined, undefined), true);
});

test('isDepsChanged : 다음 의존성이 undefined(생략)이면 바뀐 것이다', () => {
  assertEqual(isDepsChanged([], undefined), true);
});

test('isDepsChanged : 길이가 다르면 바뀐 것이다', () => {
  assertEqual(isDepsChanged([1], [1, 2]), true);
});

test('isDepsChanged : 항목이 모두 Object.is로 같으면 바뀌지 않은 것이다', () => {
  const sharedObject = {};

  assertEqual(isDepsChanged([1, 'a', sharedObject, NaN], [1, 'a', sharedObject, NaN]), false);
  assertEqual(isDepsChanged([], []), false);
});

test('isDepsChanged : 항목 하나라도 다르면 바뀐 것이다', () => {
  assertEqual(isDepsChanged([1, 2], [1, 3]), true);
  assertEqual(isDepsChanged([{}], [{}]), true);
  assertEqual(isDepsChanged([0], [-0]), true);
});
