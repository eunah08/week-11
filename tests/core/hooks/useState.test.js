import { createElement, useState, FunctionComponent } from '../../../src/core/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

function mountRoot(rootComponent) {
  const container = document.createElement('div');
  const instance = new FunctionComponent(rootComponent, {}, container);

  document.body.append(container);
  instance.mount();
  return { container, instance };
}

test('useState : 초기값을 돌려주고 렌더링이 끝나도 값을 유지한다', () => {
  const seenValues = [];
  let setCount = null;
  const root = () => {
    const [count, setter] = useState(10);

    setCount = setter;
    seenValues.push(count);
    return createElement('p', null, count);
  };
  const { container } = mountRoot(root);

  setCount(11);
  setCount(12);
  assertEqual(seenValues, [10, 11, 12]);
  assertEqual(container.textContent, '12');
});

test('useState : 초기값이 함수면 최초 한 번만 호출한다', () => {
  let callCount = 0;
  let setCount = null;
  const root = () => {
    const [count, setter] = useState(() => {
      callCount += 1;
      return 5;
    });

    setCount = setter;
    return createElement('p', null, count);
  };
  const { container } = mountRoot(root);

  setCount(6);
  setCount(7);
  assertEqual(callCount, 1);
  assertEqual(container.textContent, '7');
});

test('useState : 함수형 갱신에는 해당 상태의 최신 값을 넘긴다', () => {
  let setCount = null;
  const root = () => {
    const [count, setter] = useState(0);

    setCount = setter;
    return createElement('p', null, count);
  };
  const { container } = mountRoot(root);

  setCount((previous) => previous + 1);
  setCount((previous) => previous + 1);
  setCount((previous) => previous * 10);
  assertEqual(container.textContent, '20');
});

test('useState : 상태마다 독립적으로 유지한다', () => {
  let setFirst = null;
  let setSecond = null;
  const root = () => {
    const [first, firstSetter] = useState('a');
    const [second, secondSetter] = useState('x');

    setFirst = firstSetter;
    setSecond = secondSetter;
    return createElement('p', null, `${first}${second}`);
  };
  const { container } = mountRoot(root);

  setFirst('b');
  assertEqual(container.textContent, 'bx');
  setSecond('y');
  assertEqual(container.textContent, 'by');
});

test('useState : 상태 변경 함수는 렌더링이 바뀌어도 같은 함수다', () => {
  const setters = [];
  let setCount = null;
  const root = () => {
    const [, setter] = useState(0);

    setCount = setter;
    setters.push(setter);
    return createElement('p', null);
  };

  mountRoot(root);
  setCount(1);
  assertTrue(setters.length === 2 && setters[0] === setters[1], '상태 변경 함수가 바뀜');
});

test('useState : 같은 값으로 바꾸면 다시 렌더링하지 않는다', () => {
  let renderCount = 0;
  let setValue = null;
  const root = () => {
    const [value, setter] = useState('같음');

    renderCount += 1;
    setValue = setter;
    return createElement('p', null, value);
  };

  mountRoot(root);
  setValue('같음');
  setValue((previous) => previous);
  assertEqual(renderCount, 1);
  setValue('다름');
  assertEqual(renderCount, 2);
});

test('useState : Object.is 기준으로 비교한다 (NaN은 같고 새 객체는 다르다)', () => {
  let renderCount = 0;
  let setValue = null;
  const root = () => {
    const [, setter] = useState(NaN);

    renderCount += 1;
    setValue = setter;
    return createElement('p', null);
  };

  mountRoot(root);
  setValue(NaN);
  assertEqual(renderCount, 1);
  setValue({});
  setValue({});
  assertEqual(renderCount, 3);
});

test('useState : 렌더링 밖에서 호출하면 Error', () => {
  assertThrows(
    () => useState(0),
    Error,
    '[useState] 루트 컴포넌트 렌더링 중에만 호출할 수 있습니다',
  );
});

test('useState : 자식 컴포넌트 안에서 호출하면 Error', () => {
  const statefulChild = () => {
    useState(0);
    return createElement('span', null);
  };
  const instance = new FunctionComponent(
    () => createElement('div', null, createElement(statefulChild, null)),
    {},
    document.createElement('div'),
  );

  assertThrows(() => instance.mount(), Error, '루트 컴포넌트 렌더링 중에만 호출할 수 있습니다');
});

test('useState : 렌더링 함수 본문에서 상태를 바꾸면 Error', () => {
  const root = () => {
    const [, setValue] = useState(0);

    setValue(1);
    return createElement('p', null);
  };
  const instance = new FunctionComponent(root, {}, document.createElement('div'));

  assertThrows(() => instance.mount(), Error, '[useState] 렌더링 중에는 상태를 바꿀 수 없습니다');
});

test('useState : unmount 뒤에 상태 변경 함수를 불러도 갱신하지 않는다', () => {
  let renderCount = 0;
  let setValue = null;
  const root = () => {
    const [value, setter] = useState(0);

    renderCount += 1;
    setValue = setter;
    return createElement('p', null, value);
  };
  const { container, instance } = mountRoot(root);

  instance.unmount();
  setValue(1);
  assertEqual(renderCount, 1);
  assertEqual(container.childNodes.length, 0);
});

test('useState : 다시 mount한 뒤에는 이전 인스턴스 시절의 상태 변경 함수가 먹히지 않는다', () => {
  let setValue = null;
  const root = () => {
    const [value, setter] = useState(0);

    setValue = setter;
    return createElement('p', null, value);
  };
  const { container, instance } = mountRoot(root);
  const oldSetValue = setValue;

  instance.unmount();
  instance.mount();
  oldSetValue(99);
  assertEqual(container.textContent, '0');
  setValue(5);
  assertEqual(container.textContent, '5');
});
