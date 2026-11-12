import {
  createElement,
  useState,
  useEffect,
  FunctionComponent,
} from '../../../src/core/index.js';
import { test, assertEqual, assertThrows } from '../../testRunner.js';

// Batching : 상태를 바꾸면 갱신이 microtask로 예약되므로, 테스트에서는 예약된 갱신을 바로 실행한다.
let lastInstance = null;

function flush() {
  lastInstance.flushUpdate();
}

function mountRoot(rootComponent) {
  const container = document.createElement('div');
  const instance = new FunctionComponent(rootComponent, {}, container);

  document.body.append(container);
  instance.mount();
  lastInstance = instance;
  return { container, instance };
}

// 상태 하나를 가진 루트를 만들고, Effect 등록은 registerEffect에 맡긴다.
function createRootWithEffect(registerEffect) {
  const handles = { setValue: null };
  const root = () => {
    const [value, setValue] = useState(0);

    handles.setValue = setValue;
    registerEffect(value);
    return createElement('p', null, value);
  };

  return { root, handles };
}

test('useEffect : DOM에 반영된 뒤에 실행한다', () => {
  let textWhenRun = null;
  let container = null;
  const root = () => {
    useEffect(() => {
      textWhenRun = container.textContent;
    }, []);
    return createElement('p', null, '반영됨');
  };

  container = document.createElement('div');
  new FunctionComponent(root, {}, container).mount();
  assertEqual(textWhenRun, '반영됨');
});

test('useEffect : 렌더링 중에는 실행하지 않고 pendingEffects에 넣어 둔다', () => {
  let pendingCountDuringRender = null;
  let runCount = 0;
  const root = () => {
    useEffect(() => {
      runCount += 1;
    });
    pendingCountDuringRender = runCount;
    return createElement('p', null);
  };

  mountRoot(root);
  assertEqual(pendingCountDuringRender, 0);
  assertEqual(runCount, 1);
});

test('useEffect : 의존성을 생략하면 렌더링마다 실행한다', () => {
  let runCount = 0;
  const { root, handles } = createRootWithEffect(() => {
    useEffect(() => {
      runCount += 1;
    });
  });

  mountRoot(root);
  handles.setValue(1);
  flush();
  handles.setValue(2);
  flush();
  assertEqual(runCount, 3);
});

test('useEffect : 빈 배열이면 최초 한 번만 실행한다', () => {
  let runCount = 0;
  const { root, handles } = createRootWithEffect(() => {
    useEffect(() => {
      runCount += 1;
    }, []);
  });

  mountRoot(root);
  handles.setValue(1);
  flush();
  handles.setValue(2);
  flush();
  assertEqual(runCount, 1);
});

test('useEffect : 의존성 값이 바뀔 때만 실행한다', () => {
  const seenValues = [];
  const { root, handles } = createRootWithEffect((value) => {
    useEffect(() => {
      seenValues.push(value);
    }, [value > 1]);
  });

  mountRoot(root);
  handles.setValue(1);
  flush();
  assertEqual(seenValues, [0]);
  handles.setValue(2);
  flush();
  handles.setValue(3);
  flush();
  assertEqual(seenValues, [0, 2]);
});

test('useEffect : 의존성 길이가 달라져도 바뀐 것으로 본다', () => {
  let runCount = 0;
  const { root, handles } = createRootWithEffect((value) => {
    useEffect(() => {
      runCount += 1;
    }, value === 0 ? [1] : [1, 2]);
  });

  mountRoot(root);
  handles.setValue(1);
  flush();
  assertEqual(runCount, 2);
});

test('useEffect : 다시 실행하기 전에 이전 cleanup을 먼저 호출한다', () => {
  const logs = [];
  const { root, handles } = createRootWithEffect((value) => {
    useEffect(() => {
      logs.push(`run ${value}`);
      return () => logs.push(`cleanup ${value}`);
    }, [value]);
  });

  mountRoot(root);
  handles.setValue(1);
  flush();
  handles.setValue(2);
  flush();
  assertEqual(logs, ['run 0', 'cleanup 0', 'run 1', 'cleanup 1', 'run 2']);
});

test('useEffect : 의존성이 같으면 cleanup도 호출하지 않는다', () => {
  const logs = [];
  const { root, handles } = createRootWithEffect(() => {
    useEffect(() => {
      logs.push('run');
      return () => logs.push('cleanup');
    }, []);
  });

  mountRoot(root);
  handles.setValue(1);
  flush();
  assertEqual(logs, ['run']);
});

test('useEffect : unmount할 때 남아 있는 cleanup을 호출한다', () => {
  const logs = [];
  const root = () => {
    useEffect(() => {
      logs.push('run A');
      return () => logs.push('cleanup A');
    }, []);
    useEffect(() => {
      logs.push('run B');
      return () => logs.push('cleanup B');
    }, []);
    return createElement('p', null);
  };
  const { instance } = mountRoot(root);

  instance.unmount();
  assertEqual(logs, ['run A', 'run B', 'cleanup A', 'cleanup B']);
  instance.unmount();
  assertEqual(logs, ['run A', 'run B', 'cleanup A', 'cleanup B'], 'cleanup이 두 번 호출됨');
});

test('useEffect : 함수가 아닌 반환값은 cleanup으로 쓰지 않는다', () => {
  const root = () => {
    useEffect(() => 'cleanup이 아님', []);
    useEffect(() => Promise.resolve(), []);
    return createElement('p', null);
  };
  const { instance } = mountRoot(root);

  instance.unmount();
});

test('useEffect : 여러 Effect는 등록한 순서대로 실행한다', () => {
  const logs = [];
  const root = () => {
    useEffect(() => logs.push('A'));
    useEffect(() => logs.push('B'));
    useEffect(() => logs.push('C'));
    return createElement('p', null);
  };

  mountRoot(root);
  assertEqual(logs, ['A', 'B', 'C']);
});

test('useEffect : Effect 안에서 update()가 다시 불려도 이어서 실행할 Effect를 낡은 값으로 다시 실행하지 않는다', () => {
  const logs = [];
  const handles = { instance: null };
  let flag = false;
  const root = () => {
    useEffect(() => {
      logs.push(`A ${flag}`);

      if (!flag) {
        flag = true;
        handles.instance.update();
      }
    }, [flag]);
    useEffect(() => {
      logs.push(`B ${flag}`);
    }, [flag]);
    return createElement('p', null);
  };

  handles.instance = new FunctionComponent(root, {}, document.createElement('div'));
  handles.instance.mount();
  assertEqual(logs, ['A false', 'A true', 'B true']);
});

test('useEffect : Effect 안에서 상태를 바꾸면 갱신을 예약하고, 이어서 실행할 Effect는 그대로 실행한다', () => {
  const logs = [];
  const root = () => {
    const [flag, setFlag] = useState(false);

    useEffect(() => {
      logs.push(`A ${flag}`);

      if (!flag) {
        setFlag(true);
      }
    }, [flag]);
    useEffect(() => {
      logs.push(`B ${flag}`);
    }, [flag]);
    return createElement('p', null);
  };

  mountRoot(root);
  assertEqual(logs, ['A false', 'B false']);
  flush();
  assertEqual(logs, ['A false', 'B false', 'A true', 'B true']);
});

test('useEffect : 콜백이 함수가 아니면 TypeError', () => {
  assertThrows(() => useEffect('x', []), TypeError, '[useEffect] 콜백 함수가 필요합니다');
  assertThrows(() => useEffect(undefined), TypeError, '콜백 함수가 필요합니다');
});

test('useEffect : 렌더링 밖에서 호출하면 Error', () => {
  assertThrows(
    () => useEffect(() => {}, []),
    Error,
    '[useEffect] 루트 컴포넌트 렌더링 중에만 호출할 수 있습니다',
  );
});

test('useEffect : cleanup 하나가 에러를 던져도 나머지 cleanup과 DOM 해제를 마친 뒤 그 에러를 던진다', () => {
  const logs = [];
  const root = () => {
    useEffect(() => () => {
      logs.push('cleanup A');
      throw new Error('A 실패');
    }, []);
    useEffect(() => () => logs.push('cleanup B'), []);
    return createElement('p', null);
  };
  const { container, instance } = mountRoot(root);

  assertThrows(() => instance.unmount(), Error, 'A 실패');
  assertEqual(logs, ['cleanup A', 'cleanup B']);
  assertEqual(container.childNodes.length, 0, 'DOM이 남아 있음');
  assertEqual(instance.isMounted, false);
});

test('useEffect : cleanup 안에서 상태를 바꿔도 다시 렌더링하지 않는다', () => {
  let renderCount = 0;
  const root = () => {
    const [, setValue] = useState(0);

    renderCount += 1;
    useEffect(() => () => setValue(1), []);
    return createElement('p', null);
  };
  const { instance } = mountRoot(root);

  instance.unmount();
  assertEqual(renderCount, 1);
});
