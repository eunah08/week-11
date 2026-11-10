import {
  createElement,
  memo,
  useState,
  FunctionComponent,
} from '../../../src/core/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

// 상태 count, other를 가진 루트를 만든다. renderChild가 자식 컴포넌트를 사용하는 VNode를 돌려준다.
function mountRoot(renderChild) {
  const handles = { setCount: null, setOther: null, instance: null };
  const root = () => {
    const [count, setCount] = useState(0);
    const [other, setOther] = useState(0);

    handles.setCount = setCount;
    handles.setOther = setOther;
    return createElement('div', null, `${other}`, renderChild({ count, other }));
  };
  const container = document.createElement('div');

  handles.instance = new FunctionComponent(root, {}, container);
  handles.instance.mount();
  handles.container = container;
  return handles;
}

function change(handles, setterName, value) {
  handles[setterName](value);
  handles.instance.flushUpdate();
}

// Memoization
test('memo : 같은 함수를 돌려주고, 함수가 아니면 TypeError', () => {
  const component = () => createElement('p', null);

  assertTrue(memo(component) === component, '새 함수를 만듦');
  assertThrows(() => memo('x'), TypeError, '[memo] 컴포넌트 함수가 필요합니다');
});

// Memoization
test('memo : props가 같으면 자식 컴포넌트를 다시 실행하지 않는다', () => {
  let callCount = 0;
  const badge = memo(({ label }) => {
    callCount += 1;
    return createElement('b', null, label);
  });
  const handles = mountRoot(() => createElement(badge, { label: '고정' }));

  change(handles, 'setOther', 1);
  change(handles, 'setOther', 2);
  assertEqual(callCount, 1);
  assertEqual(handles.container.textContent, '2고정');
});

// Memoization
test('memo : props가 바뀌면 다시 실행한다', () => {
  let callCount = 0;
  const badge = memo(({ label }) => {
    callCount += 1;
    return createElement('b', null, label);
  });
  const handles = mountRoot(({ count }) => createElement(badge, { label: `값${count}` }));

  change(handles, 'setCount', 1);
  change(handles, 'setOther', 5);
  change(handles, 'setCount', 2);
  assertEqual(callCount, 3);
  assertEqual(handles.container.textContent, '5값2');
});

// Memoization
test('memo : memo가 아닌 자식 컴포넌트는 매번 실행한다', () => {
  let callCount = 0;
  const badge = ({ label }) => {
    callCount += 1;
    return createElement('b', null, label);
  };
  const handles = mountRoot(() => createElement(badge, { label: '고정' }));

  change(handles, 'setOther', 1);
  change(handles, 'setOther', 2);
  assertEqual(callCount, 3);
});

// Memoization
test('memo : 이전과 같은 VNode 객체를 그대로 돌려준다', () => {
  const badge = memo(() => createElement('b', null, '글'));
  const handles = mountRoot(() => createElement(badge, null));
  const firstBadge = handles.instance.currentVNode.children[1];

  change(handles, 'setOther', 1);
  assertTrue(handles.instance.currentVNode.children[1] === firstBadge, '새 VNode를 만듦');
});

// Memoization
test('memo : 같은 컴포넌트를 여러 번 부르면 호출 순번별로 따로 기억한다', () => {
  const calledLabels = [];
  const item = memo(({ label }) => {
    calledLabels.push(label);
    return createElement('li', null, label);
  });
  const handles = mountRoot(({ count }) => createElement(
    'ul',
    null,
    createElement(item, { label: 'A' }),
    createElement(item, { label: count === 0 ? 'B' : 'B2' }),
    createElement(item, { label: 'C' }),
  ));

  change(handles, 'setCount', 1);
  assertEqual(calledLabels, ['A', 'B', 'C', 'B2']);
  assertEqual(handles.container.textContent, '0AB2C');
});

// Memoization
test('memo : memo가 아닌 자식 안쪽의 memo 손자 컴포넌트도 재사용한다', () => {
  const calledLabels = [];
  const item = memo(({ label }) => {
    calledLabels.push(label);
    return createElement('li', null, label);
  });
  const list = ({ labels }) => createElement(
    'ul',
    null,
    labels.map((label) => createElement(item, { label })),
  );
  const handles = mountRoot(({ count }) => {
    return createElement(list, { labels: count === 0 ? ['A', 'B'] : ['A', 'B', 'C'] });
  });

  change(handles, 'setCount', 1);
  assertEqual(calledLabels, ['A', 'B', 'C']);
  assertEqual(handles.container.textContent, '0ABC');
});

// Memoization
test('memo : 배열 props는 항목이 같으면 같은 값으로 본다', () => {
  let callCount = 0;
  const sharedTodo = { id: 1 };
  const board = memo(({ todos }) => {
    callCount += 1;
    return createElement('p', null, `${todos.length}`);
  });
  const handles = mountRoot(({ count }) => {
    return createElement(board, { todos: count < 2 ? [sharedTodo] : [sharedTodo, { id: 2 }] });
  });

  change(handles, 'setCount', 1);
  assertEqual(callCount, 1);
  change(handles, 'setCount', 2);
  assertEqual(callCount, 2);
});

// Memoization
test('memo : 함수 props는 참조가 같아야 같은 값이다', () => {
  let callCount = 0;
  const stableHandler = () => {};
  const button = memo(() => {
    callCount += 1;
    return createElement('button', null, '버튼');
  });
  const handles = mountRoot(({ count }) => {
    return createElement(button, { onClick: count === 0 ? stableHandler : () => {} });
  });

  change(handles, 'setOther', 1);
  assertEqual(callCount, 1);
  change(handles, 'setCount', 1);
  assertEqual(callCount, 2);
});

// Memoization
test('memo : 렌더링 밖에서 createElement로 부르면 캐시 없이 그냥 실행한다', () => {
  let callCount = 0;
  const badge = memo(() => {
    callCount += 1;
    return createElement('b', null);
  });

  createElement(badge, null);
  createElement(badge, null);
  assertEqual(callCount, 2);
});

// Memoization
test('memo : 렌더링이 실패해도 마지막으로 성공한 결과를 기준으로 삼는다', () => {
  let callCount = 0;
  let shouldBreak = false;
  const badge = memo(({ label }) => {
    callCount += 1;
    return createElement('b', null, label);
  });
  const handles = mountRoot(({ count }) => {
    const vnode = createElement(badge, { label: `값${count}` });

    if (shouldBreak) {
      throw new Error('렌더링 실패');
    }

    return vnode;
  });

  shouldBreak = true;
  handles.setCount(1);
  assertThrows(() => handles.instance.flushUpdate(), Error, '렌더링 실패');
  shouldBreak = false;
  handles.setCount(0);
  handles.instance.flushUpdate();
  assertEqual(callCount, 2, '값0은 처음 성공한 결과를 재사용해야 한다');
  assertEqual(handles.container.textContent, '0값0');
});

// Memoization
test('memo : unmount 뒤 다시 mount하면 이전 결과를 쓰지 않는다', () => {
  let callCount = 0;
  const badge = memo(() => {
    callCount += 1;
    return createElement('b', null);
  });
  const handles = mountRoot(() => createElement(badge, null));

  handles.instance.unmount();
  handles.instance.mount();
  assertEqual(callCount, 2);
});
