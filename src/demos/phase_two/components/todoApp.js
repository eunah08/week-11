import { createElement, useState, useMemo, useEffect } from '../../../core/index.js';
import { FILTERS, filterTodos, todoFilter } from './todoFilter.js';
import { todoForm } from './todoForm.js';
import { todoList } from './todoList.js';
import { todoSummary } from './todoSummary.js';

const EMPTY_INPUT_MESSAGE = '[todoApp] 할 일을 입력해 주세요';

const INITIAL_TODOS = [
  { id: 1, text: 'Virtual DOM 복습하기', isDone: true },
  { id: 2, text: 'useState 구현하기', isDone: false },
  { id: 3, text: 'Hook 규칙 읽어 보기', isDone: false },
];

/**
 * 할 일 목록 루트 컴포넌트. State와 Hook은 여기에서만 쓰고, 자식에게는 props로 내려준다.
 * 이벤트 핸들러의 에러는 props.showError로 화면에 보여 준다. (Phase 2 문서 12장)
 * @param {{ showError: Function }} props showError(error | null) : 에러를 보여 주거나(null이면 지움)
 * @returns {object} VNode
 */
export function todoApp(props) {
  const [todos, setTodos] = useState(INITIAL_TODOS);
  const [draftText, setDraftText] = useState('');
  const [filter, setFilter] = useState(FILTERS.ALL);

  const visibleTodos = useMemo(() => filterTodos(todos, filter), [todos, filter]);
  const remainingCount = useMemo(() => countRemainingTodos(todos), [todos]);

  useEffect(() => showRemainingInTitle(remainingCount), [remainingCount]);

  const setters = { setTodos, setDraftText, setFilter };
  const handlers = createHandlers({ draftText, ...setters }, props.showError);

  return createElement(
    'div',
    { class: 'todo-app' },
    createElement('h2', { class: 'todo-title' }, '할 일 목록'),
    createElement(todoForm, {
      draftText,
      onDraftInput: handlers.onDraftInput,
      onSubmit: handlers.onSubmit,
    }),
    createElement(todoFilter, { currentFilter: filter, onChange: handlers.onFilterChange }),
    createElement(todoList, {
      todos: visibleTodos,
      onToggle: handlers.onToggle,
      onRemove: handlers.onRemove,
    }),
    createElement(todoSummary, { remainingCount, totalCount: todos.length }),
  );
}

function countRemainingTodos(todos) {
  return todos.filter((todo) => !todo.isDone).length;
}

// DOM 반영 뒤에 브라우저 탭 제목에 남은 개수를 붙인다. 다시 실행하거나 해제할 때 cleanup이 되돌린다.
function showRemainingInTitle(remainingCount) {
  const previousTitle = document.title;

  document.title = `(${remainingCount}) ${previousTitle}`;

  return () => {
    document.title = previousTitle;
  };
}

// 이벤트 핸들러 모음. 상태를 바꾸는 함수는 모두 루트가 만들어 자식에게 내려준다. (Phase 2 문서 9장)
function createHandlers(state, showError) {
  const { draftText, setTodos, setDraftText, setFilter } = state;
  const runSafely = createSafeRunner(showError);

  return {
    onDraftInput: runSafely((event) => setDraftText(event.target.value)),
    onSubmit: runSafely((event) => {
      event.preventDefault();
      addTodo({ draftText, setTodos, setDraftText });
    }),
    onToggle: runSafely((todoId) => setTodos((todos) => toggleTodo(todos, todoId))),
    onRemove: runSafely((todoId) => setTodos((todos) => removeTodo(todos, todoId))),
    onFilterChange: runSafely((nextFilter) => setFilter(nextFilter)),
  };
}

// 핸들러를 감싸 에러를 화면에 보여 준다. 성공하면 이전 에러 표시를 지운다.
function createSafeRunner(showError) {
  return (handler) => (...args) => {
    try {
      handler(...args);
      showError(null);
    } catch (error) {
      showError(error);
    }
  };
}

function addTodo({ draftText, setTodos, setDraftText }) {
  const text = draftText.trim();

  if (text === '') {
    throw new Error(EMPTY_INPUT_MESSAGE);
  }

  setTodos((todos) => [...todos, createTodo(todos, text)]);
  setDraftText('');
}

function createTodo(todos, text) {
  const lastId = todos.reduce((maxId, todo) => Math.max(maxId, todo.id), 0);

  return { id: lastId + 1, text, isDone: false };
}

function toggleTodo(todos, todoId) {
  return todos.map((todo) => (todo.id === todoId ? { ...todo, isDone: !todo.isDone } : todo));
}

function removeTodo(todos, todoId) {
  return todos.filter((todo) => todo.id !== todoId);
}
