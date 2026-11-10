import { createElement, useState, useMemo, FunctionComponent } from '../../../src/core/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

function mountRoot(rootComponent) {
  const container = document.createElement('div');
  const instance = new FunctionComponent(rootComponent, {}, container);

  document.body.append(container);
  instance.mount();
  return { container, instance };
}

// 상태 두 개(count, other)를 가진 루트를 만들고, useMemo 사용은 useMemoCall에 맡긴다.
function createRootWithMemo(useMemoCall) {
  const handles = { setCount: null, setOther: null, results: [] };
  const root = () => {
    const [count, setCount] = useState(1);
    const [other, setOther] = useState(0);

    handles.setCount = setCount;
    handles.setOther = setOther;
    handles.results.push(useMemoCall(count, other));
    return createElement('p', null, `${count}-${other}`);
  };

  return { root, handles };
}

test('useMemo : 최초 렌더링에서는 계산한다', () => {
  let calculateCount = 0;
  const { root, handles } = createRootWithMemo((count) => useMemo(() => {
    calculateCount += 1;
    return count * 2;
  }, [count]));

  mountRoot(root);
  assertEqual(calculateCount, 1);
  assertEqual(handles.results, [2]);
});

test('useMemo : 의존성이 같으면 계산 함수를 다시 실행하지 않고 이전 결과를 돌려준다', () => {
  let calculateCount = 0;
  const { root, handles } = createRootWithMemo((count) => useMemo(() => {
    calculateCount += 1;
    return { doubled: count * 2 };
  }, [count]));

  mountRoot(root);
  handles.setOther(1);
  handles.setOther(2);
  assertEqual(calculateCount, 1);
  assertTrue(handles.results[0] === handles.results[2], '이전 결과를 재사용하지 않음');
});

test('useMemo : 의존성이 달라지면 다시 계산한다', () => {
  let calculateCount = 0;
  const { root, handles } = createRootWithMemo((count) => useMemo(() => {
    calculateCount += 1;
    return count * 2;
  }, [count]));

  mountRoot(root);
  handles.setCount(5);
  handles.setCount(6);
  assertEqual(calculateCount, 3);
  assertEqual(handles.results, [2, 10, 12]);
});

test('useMemo : 의존성을 생략하면 렌더링마다 계산한다', () => {
  let calculateCount = 0;
  const { root, handles } = createRootWithMemo(() => useMemo(() => {
    calculateCount += 1;
    return calculateCount;
  }));

  mountRoot(root);
  handles.setOther(1);
  handles.setOther(2);
  assertEqual(calculateCount, 3);
});

test('useMemo : 빈 배열이면 최초 한 번만 계산한다', () => {
  let calculateCount = 0;
  const { root, handles } = createRootWithMemo(() => useMemo(() => {
    calculateCount += 1;
    return calculateCount;
  }, []));

  mountRoot(root);
  handles.setCount(2);
  handles.setCount(3);
  assertEqual(calculateCount, 1);
});

test('useMemo : 계산이 에러를 던지면 다음 렌더링에서 다시 계산한다', () => {
  let shouldFail = true;
  let calculateCount = 0;
  const { root } = createRootWithMemo(() => useMemo(() => {
    calculateCount += 1;

    if (shouldFail) {
      throw new Error('계산 실패');
    }

    return 'ok';
  }, []));
  const container = document.createElement('div');
  const instance = new FunctionComponent(root, {}, container);

  assertThrows(() => instance.mount(), Error, '계산 실패');
  shouldFail = false;
  instance.mount();
  assertEqual(calculateCount, 2);
});

test('useMemo : 계산 함수가 함수가 아니면 TypeError', () => {
  assertThrows(() => useMemo('x', []), TypeError, '[useMemo] 계산 함수가 필요합니다');
  assertThrows(() => useMemo(undefined), TypeError, '계산 함수가 필요합니다');
});

test('useMemo : 렌더링 밖에서 호출하면 Error', () => {
  assertThrows(
    () => useMemo(() => 1, []),
    Error,
    '[useMemo] 루트 컴포넌트 렌더링 중에만 호출할 수 있습니다',
  );
});
