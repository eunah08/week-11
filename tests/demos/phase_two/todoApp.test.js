import { FunctionComponent } from '../../../src/core/index.js';
import { todoApp } from '../../../src/demos/phase_two/components/todoApp.js';
import { filterTodos, FILTERS } from '../../../src/demos/phase_two/components/todoFilter.js';
import { startPhaseTwo } from '../../../src/demos/phase_two/app.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

// 데모가 탭 제목을 바꾸기 전의 제목. 테스트가 끝나면 이 값으로 돌아와야 한다.
const BASE_TITLE = document.title;

// 데모를 문서에 붙여 실행하고, 테스트가 끝나면 해제해서 탭 제목과 문서를 되돌린다.
function withMountedApp(runTest) {
  const container = document.createElement('div');
  const errors = [];
  const showError = (error) => errors.push(error === null ? null : error.message);
  const instance = new FunctionComponent(todoApp, { showError }, container);

  document.body.append(container);
  instance.mount();

  try {
    runTest({ container, errors, instance });
  } finally {
    instance.unmount();
    container.remove();
  }
}

function type(input, value) {
  input.value = value;
  input.dispatchEvent(new Event('input', { bubbles: true }));
}

function submit(container) {
  const form = container.querySelector('form');

  form.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
}

function click(domNode) {
  domNode.dispatchEvent(new MouseEvent('click', { bubbles: true, cancelable: true }));
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
