import { FunctionComponent } from '../../../src/core/index.js';
import { todoApp } from '../../../src/demos/phase_two/components/todoApp.js';
import {
  filterTodos,
  searchTodos,
  FILTERS,
} from '../../../src/demos/phase_two/components/todoFilter.js';
import { startPhaseTwo } from '../../../src/demos/phase_two/app.js';
import { installFakeTimers } from '../../fakeTimers.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

// 데모가 탭 제목을 바꾸기 전의 제목. 테스트가 끝나면 이 값으로 돌아와야 한다.
const BASE_TITLE = document.title;

// Batching : 상태를 바꾸면 갱신이 microtask로 예약되므로, 조작 직후 예약된 갱신을 바로 실행한다.
let activeInstance = null;

function flush() {
  if (activeInstance !== null) {
    activeInstance.flushUpdate();
  }
}

// 데모를 문서에 붙여 실행하고, 테스트가 끝나면 해제해서 탭 제목과 문서를 되돌린다.
function withMountedApp(runTest) {
  const container = document.createElement('div');
  const errors = [];
  const showError = (error) => errors.push(error === null ? null : error.message);
  const instance = new FunctionComponent(todoApp, { showError }, container);

  instance.onError = (error) => errors.push(error.message);
  document.body.append(container);
  instance.mount();
  activeInstance = instance;

  try {
    runTest({ container, errors, instance });
  } finally {
    instance.unmount();
    container.remove();
    activeInstance = null;
  }
}

function type(input, value) {
  input.value = value;
  input.dispatchEvent(new Event('input', { bubbles: true }));
  flush();
}

function submit(container) {
  const form = container.querySelector('form');

  form.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  flush();
}

function click(domNode) {
  domNode.dispatchEvent(new MouseEvent('click', { bubbles: true, cancelable: true }));
  flush();
}

function getItemTexts(container) {
  return Array.from(container.querySelectorAll('.todo-text')).map((span) => span.textContent);
}

function getSummary(container) {
  return container.querySelector('.todo-summary').textContent;
}

test('todo 데모 : 처음에는 예시 할 일 3개와 남은 개수를 보여 준다', () => {
  withMountedApp(({ container }) => {
    assertEqual(getItemTexts(container), [
      'Virtual DOM 복습하기',
      'useState 구현하기',
      'Hook 규칙 읽어 보기',
    ]);
    assertEqual(getSummary(container), '남은 할 일 2개 / 전체 3개');
  });
});

test('todo 데모 : 글자를 입력하면 State가 바뀌고 입력창 DOM과 포커스가 유지된다', () => {
  withMountedApp(({ container }) => {
    const input = container.querySelector('.todo-input');

    input.focus();

    for (const typedText of ['새', '새 할', '새 할 일']) {
      type(input, typedText);
      assertTrue(container.querySelector('.todo-input') === input, '입력창 DOM이 교체됨');
      assertTrue(document.activeElement === input, '입력창 포커스를 잃음');
      assertEqual(input.value, typedText);
    }
  });
});

test('todo 데모 : 입력 중에는 목록과 제목 DOM을 건드리지 않는다 (바뀐 부분만 갱신)', () => {
  withMountedApp(({ container }) => {
    const input = container.querySelector('.todo-input');
    const list = container.querySelector('.todo-list');
    const title = container.querySelector('.todo-title');
    const observer = new MutationObserver(() => {});
    const watchAllChanges = {
      childList: true,
      subtree: true,
      characterData: true,
      attributes: true,
    };

    observer.observe(list, watchAllChanges);
    observer.observe(title, watchAllChanges);
    type(input, '글');

    assertEqual(observer.takeRecords().length, 0, '목록이나 제목이 바뀜');
    observer.disconnect();
  });
});

test('todo 데모 : 추가하면 목록과 개수가 바뀌고 입력창이 비워진다', () => {
  withMountedApp(({ container, errors }) => {
    const input = container.querySelector('.todo-input');
    const firstItem = container.querySelector('.todo-item');

    type(input, '  우유 사기  ');
    submit(container);

    assertEqual(getItemTexts(container).at(-1), '우유 사기');
    assertEqual(getSummary(container), '남은 할 일 3개 / 전체 4개');
    assertEqual(input.value, '');
    assertTrue(container.querySelector('.todo-input') === input, '입력창 DOM이 교체됨');
    assertTrue(container.querySelector('.todo-item') === firstItem, '기존 항목 DOM이 교체됨');
    assertEqual(errors.at(-1), null);
  });
});

test('todo 데모 : 빈 입력으로 추가하면 에러 메시지를 보여 주고 목록은 그대로다', () => {
  withMountedApp(({ container, errors }) => {
    submit(container);
    assertEqual(errors.at(-1), '[todoApp] 할 일을 입력해 주세요');
    assertEqual(getItemTexts(container).length, 3);

    type(container.querySelector('.todo-input'), '   ');
    submit(container);
    assertEqual(errors.at(-1), '[todoApp] 할 일을 입력해 주세요');
  });
});

test('todo 데모 : 체크박스를 누르면 완료가 바뀌고 남은 개수가 갱신된다', () => {
  withMountedApp(({ container }) => {
    const secondCheckbox = container.querySelectorAll('.todo-item input')[1];

    click(secondCheckbox);
    const secondItem = container.querySelectorAll('.todo-item')[1];

    assertTrue(secondItem.classList.contains('is-done'), '완료 표시 안 됨');
    assertEqual(getSummary(container), '남은 할 일 1개 / 전체 3개');

    click(container.querySelectorAll('.todo-item input')[1]);
    assertEqual(getSummary(container), '남은 할 일 2개 / 전체 3개');
  });
});

test('todo 데모 : 삭제하면 그 항목이 사라지고 나머지 완료 상태가 맞게 남는다', () => {
  withMountedApp(({ container }) => {
    click(container.querySelectorAll('.todo-remove-button')[0]);

    assertEqual(getItemTexts(container), ['useState 구현하기', 'Hook 규칙 읽어 보기']);
    assertEqual(container.querySelectorAll('.todo-item.is-done').length, 0);
    assertEqual(
      Array.from(container.querySelectorAll('.todo-item input')).map((box) => box.checked),
      [false, false],
    );
    assertEqual(getSummary(container), '남은 할 일 2개 / 전체 2개');
  });
});

test('todo 데모 : 필터 버튼으로 보이는 항목이 바뀌고, 없으면 안내 문구를 보여 준다', () => {
  withMountedApp(({ container }) => {
    const filterButton = (filter) => container.querySelector(`[data-filter="${filter}"]`);

    click(filterButton(FILTERS.DONE));
    assertEqual(getItemTexts(container), ['Virtual DOM 복습하기']);
    assertEqual(filterButton(FILTERS.DONE).getAttribute('aria-pressed'), 'true');
    assertEqual(filterButton(FILTERS.ALL).getAttribute('aria-pressed'), 'false');

    click(filterButton(FILTERS.ACTIVE));
    assertEqual(getItemTexts(container), ['useState 구현하기', 'Hook 규칙 읽어 보기']);

    click(container.querySelectorAll('.todo-item input')[0]);
    assertEqual(getItemTexts(container), ['Hook 규칙 읽어 보기']);
    assertEqual(container.querySelector('.todo-item input').checked, false);
    assertEqual(container.querySelector('.todo-item').classList.contains('is-done'), false);

    click(container.querySelectorAll('.todo-item input')[0]);
    assertEqual(container.querySelector('.todo-empty').textContent, '보여 줄 할 일이 없어요.');

    click(filterButton(FILTERS.ALL));
    assertEqual(getItemTexts(container).length, 3);
  });
});

test('todo 데모 : 연속으로 조작해도 이벤트가 중복 실행되지 않는다', () => {
  withMountedApp(({ container }) => {
    const input = container.querySelector('.todo-input');

    for (const typedText of ['a', 'ab', 'abc', 'abcd', 'abcde']) {
      type(input, typedText);
    }

    submit(container);
    assertEqual(getItemTexts(container).length, 4, '한 번 제출했는데 여러 개가 추가됨');

    for (let i = 0; i < 4; i += 1) {
      click(container.querySelectorAll('.todo-item input')[0]);
    }

    assertEqual(container.querySelectorAll('.todo-item')[0].classList.contains('is-done'), true);
    assertEqual(getSummary(container), '남은 할 일 3개 / 전체 4개');
  });
});

test('todo 데모 : 남은 개수가 탭 제목에 반영되고 개수가 그대로면 Effect를 다시 실행하지 않는다', () => {
  withMountedApp(({ container }) => {
    assertEqual(document.title, `(2) ${BASE_TITLE}`);

    click(container.querySelectorAll('.todo-item input')[1]);
    assertEqual(document.title, `(1) ${BASE_TITLE}`);

    // 입력만 해서 남은 개수가 그대로면 Effect는 다시 실행되지 않는다.
    type(container.querySelector('.todo-input'), '글');
    assertEqual(document.title, `(1) ${BASE_TITLE}`);
  });
});

test('todo 데모 : unmount하면 탭 제목을 되돌리고 이후 상태 변경이 화면을 바꾸지 않는다', () => {
  withMountedApp(({ container, instance }) => {
    const checkbox = container.querySelectorAll('.todo-item input')[1];

    instance.unmount();
    assertEqual(document.title, BASE_TITLE);
    assertEqual(container.childNodes.length, 0);

    click(checkbox);
    assertEqual(container.childNodes.length, 0);
    assertEqual(document.title, BASE_TITLE);

    instance.mount();
  });
});

// Selective Rendering
// 루트 div의 자식 순서는 [제목, 폼, 검색, 필터, 목록, 요약]이다.
const LIST_CHILD_INDEX = 4;

function getListVNode(instance) {
  return instance.currentVNode.children[LIST_CHILD_INDEX];
}

// Selective Rendering
test('todo 데모 : 입력만 바뀌면 목록 VNode를 다시 만들지 않고 그대로 재사용한다', () => {
  withMountedApp(({ container, instance }) => {
    const listVNodeBefore = getListVNode(instance);

    type(container.querySelector('.todo-input'), '글');
    assertTrue(getListVNode(instance) === listVNodeBefore, '목록 VNode를 다시 만듦');
  });
});

// Selective Rendering
test('todo 데모 : 한 줄만 토글하면 나머지 줄의 VNode와 DOM은 그대로다', () => {
  withMountedApp(({ container, instance }) => {
    const itemVNodesBefore = [...getListVNode(instance).children];
    const itemNodes = Array.from(container.querySelectorAll('.todo-item'));
    const observer = new MutationObserver(() => {});
    const watchAllChanges = {
      childList: true,
      subtree: true,
      characterData: true,
      attributes: true,
    };

    observer.observe(itemNodes[0], watchAllChanges);
    observer.observe(itemNodes[2], watchAllChanges);
    click(container.querySelectorAll('.todo-item input')[1]);

    const itemVNodesAfter = getListVNode(instance).children;

    assertTrue(itemVNodesAfter[0] === itemVNodesBefore[0], '첫째 줄 VNode를 다시 만듦');
    assertTrue(itemVNodesAfter[2] === itemVNodesBefore[2], '셋째 줄 VNode를 다시 만듦');
    assertTrue(itemVNodesAfter[1] !== itemVNodesBefore[1], '토글한 줄 VNode가 그대로임');
    assertEqual(observer.takeRecords().length, 0, '바뀌지 않은 줄의 DOM이 바뀜');
    observer.disconnect();
  });
});

// Batching
test('todo 데모 : 할 일을 추가하면 목록과 입력창이 바뀌어도 렌더링은 한 번이다', () => {
  withMountedApp(({ container, instance }) => {
    type(container.querySelector('.todo-input'), '우유 사기');

    const renderCountBefore = instance.renderCount;

    submit(container);
    assertEqual(instance.renderCount - renderCountBefore, 1, '상태를 두 번 바꿨는데 여러 번 렌더링됨');
    assertEqual(container.querySelector('.todo-input').value, '');
    assertEqual(getItemTexts(container).length, 4);
  });
});

// Debouncing
test('todo 데모 : 검색창 글자는 바로 보이고 목록은 입력이 멈춘 뒤에 걸러진다', () => {
  const timers = installFakeTimers();

  try {
    withMountedApp(({ container }) => {
      const searchInput = container.querySelector('.todo-search-input');

      searchInput.focus();
      type(searchInput, 'use');
      assertEqual(searchInput.value, 'use');
      assertTrue(document.activeElement === searchInput, '검색창 포커스를 잃음');
      assertEqual(getItemTexts(container).length, 3, '입력 직후에 목록이 걸러짐');
      assertEqual(
        container.querySelector('.todo-search-status').textContent,
        '입력이 멈추면 검색해요',
      );

      timers.runPendingTimers();
      flush();
      assertEqual(getItemTexts(container), ['useState 구현하기']);
      assertEqual(container.querySelector('.todo-search-status').textContent, '');
      assertEqual(getSummary(container), '남은 할 일 2개 / 전체 3개');
    });
  } finally {
    timers.uninstall();
  }
});

// Debouncing
test('todo 데모 : 연속으로 입력하면 타이머는 하나만 남고 마지막 검색어로 한 번 걸러진다', () => {
  const timers = installFakeTimers();

  try {
    withMountedApp(({ container }) => {
      const searchInput = container.querySelector('.todo-search-input');

      for (const typedText of ['h', 'ho', 'hoo', 'hook']) {
        type(searchInput, typedText);
      }

      assertEqual(timers.getPendingDelays(), [300]);
      timers.runPendingTimers();
      flush();
      assertEqual(getItemTexts(container), ['Hook 규칙 읽어 보기']);
    });
  } finally {
    timers.uninstall();
  }
});

// Debouncing
test('todo 데모 : 검색어를 지우면 다시 전체 목록이 보이고, 검색 중 할 일 입력은 바로 반영된다', () => {
  const timers = installFakeTimers();

  try {
    withMountedApp(({ container }) => {
      const searchInput = container.querySelector('.todo-search-input');

      type(searchInput, 'dom');
      timers.runPendingTimers();
      flush();
      assertEqual(getItemTexts(container), ['Virtual DOM 복습하기']);

      type(container.querySelector('.todo-input'), '입력은 즉시');
      assertEqual(container.querySelector('.todo-input').value, '입력은 즉시');

      type(searchInput, '');
      timers.runPendingTimers();
      flush();
      assertEqual(getItemTexts(container).length, 3);
    });
  } finally {
    timers.uninstall();
  }
});

// Debouncing
test('searchTodos : 대소문자를 구분하지 않고, 검색어가 비면 같은 배열을 돌려준다', () => {
  const todos = [{ id: 1, text: 'Hook 읽기', isDone: false }, { id: 2, text: '쉬기', isDone: false }];

  assertEqual(searchTodos(todos, 'hOOk').map((todo) => todo.id), [1]);
  assertTrue(searchTodos(todos, '   ') === todos, '비었는데 새 배열을 만듦');
  assertEqual(searchTodos(todos, '없는말'), []);
});

test('filterTodos : 받은 배열을 바꾸지 않고 새 배열을 돌려주며, 알 수 없는 필터면 Error', () => {
  const todos = [{ id: 1, text: 'a', isDone: false }];
  const filtered = filterTodos(todos, FILTERS.ALL);

  assertTrue(filtered !== todos, '받은 배열을 그대로 돌려줌');
  assertEqual(todos, [{ id: 1, text: 'a', isDone: false }]);
  assertThrows(() => filterTodos(todos, 'toString'), Error, '알 수 없는 필터입니다');
});

test('startPhaseTwo : data-role 요소를 찾아 mount하고, 요소가 없으면 Error', () => {
  assertThrows(() => startPhaseTwo(), Error, '[startPhaseTwo] 화면 요소를 찾을 수 없습니다');

  const page = document.createElement('main');

  page.innerHTML = '<p data-role="todo-error"></p><div data-role="todo-root"></div>';
  document.body.append(page);

  try {
    startPhaseTwo();
    assertEqual(page.querySelectorAll('.todo-item').length, 3);

    submit(page);
    const errorMessage = page.querySelector('[data-role="todo-error"]');

    assertEqual(errorMessage.textContent, '[todoApp] 할 일을 입력해 주세요');
  } finally {
    page.remove();
    document.title = BASE_TITLE;
  }
});

test('todo 데모 : 완료 필터에서 첫 행을 해제해도 다음 행의 체크 상태가 유지된다', () => {
  withMountedApp(({ container }) => {
    click(container.querySelectorAll('.todo-item input')[1]);
    click(container.querySelector('[data-filter="done"]'));
    click(container.querySelector('.todo-item input'));

    assertEqual(getItemTexts(container), ['useState 구현하기']);
    assertEqual(container.querySelector('.todo-item input').checked, true);
    assertEqual(container.querySelector('.todo-item').classList.contains('is-done'), true);
  });
});

test('todo 데모 : 마지막 삭제 버튼이 사라지면 이전 버튼 또는 입력창에 포커스를 둔다', () => {
  withMountedApp(({ container }) => {
    const lastButtonSelector = '.todo-item:last-child [data-action="remove"]';
    const lastButton = () => container.querySelector(lastButtonSelector);
    for (let remaining = 2; remaining >= 0; remaining -= 1) {
      const button = lastButton();
      button.focus();
      click(button);
      assertEqual(getItemTexts(container).length, remaining);
      const expected = lastButton() ?? container.querySelector('#todo-input');
      assertTrue(document.activeElement === expected, '삭제 후 포커스가 목록 밖으로 벗어남');
    }
  });
});

test('todo 데모 : 삭제가 반영되기 전에 옮긴 포커스를 그대로 유지한다', () => {
  withMountedApp(({ container }) => {
    const button = container.querySelector('.todo-item:last-child [data-action="remove"]');
    const search = container.querySelector('#todo-search-input');
    button.focus();
    button.click();
    search.focus();
    flush();

    assertTrue(document.activeElement === search, '삭제가 검색창 포커스를 빼앗음');
  });
});
