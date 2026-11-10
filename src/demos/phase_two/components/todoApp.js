import {
  createElement,
  useState,
  useMemo,
  useEffect,
  useCallback,
  useDebouncedValue,
} from '../../../core/index.js';
import { FILTERS, filterTodos, searchTodos, todoFilter } from './todoFilter.js';
import { todoForm } from './todoForm.js';
import { todoSearch } from './todoSearch.js';
import { todoList } from './todoList.js';
import { todoSummary } from './todoSummary.js';

const EMPTY_INPUT_MESSAGE = '[todoApp] 할 일을 입력해 주세요';
const SEARCH_DELAY_MS = 300;

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
  const [searchText, setSearchText] = useState('');

  // Debouncing : 검색창 글자는 바로 보여 주고, 목록을 거르는 검색어는 입력이 멈춘 뒤에 따라오게 한다.
  const debouncedSearchText = useDebouncedValue(searchText, SEARCH_DELAY_MS);

  const visibleTodos = useVisibleTodos(todos, filter, debouncedSearchText);
  const remainingCount = useMemo(() => countRemainingTodos(todos), [todos]);

  useEffect(() => showRemainingInTitle(remainingCount), [remainingCount]);

  const setters = { setTodos, setDraftText, setFilter, setSearchText };
  const inputHandlers = useInputHandlers({ draftText, ...setters }, props.showError);
  const listHandlers = useListHandlers(setters, props.showError);

  return createTodoAppVNode({
    draftText,
    filter,
    searchText,
    isSearchPending: searchText !== debouncedSearchText,
    visibleTodos,
    remainingCount,
    totalCount: todos.length,
    handlers: { ...inputHandlers, ...listHandlers },
  });
}

// 화면 구성. Hook을 쓰지 않고 값을 받아 VNode만 만든다.
function createTodoAppVNode(view) {
  const { handlers } = view;

  return createElement(
    'div',
    { class: 'todo-app' },
    createElement('h2', { class: 'todo-title' }, '할 일 목록'),
    createElement(todoForm, {
      draftText: view.draftText,
      onDraftInput: handlers.onDraftInput,
      onSubmit: handlers.onSubmit,
    }),
    createElement(todoSearch, {
      searchText: view.searchText,
      isPending: view.isSearchPending,
      onSearchInput: handlers.onSearchInput,
    }),
    createElement(todoFilter, { currentFilter: view.filter, onChange: handlers.onFilterChange }),
    createElement(todoList, {
      todos: view.visibleTodos,
      onToggle: handlers.onToggle,
      onRemove: handlers.onRemove,
    }),
    createElement(todoSummary, {
      remainingCount: view.remainingCount,
      totalCount: view.totalCount,
    }),
  );
}

// Memoization : 입력만 바뀔 때는 목록을 다시 거르지 않고 이전 결과를 쓴다.
// 루트의 최상위에서 부르는 Hook 묶음이다.
function useVisibleTodos(todos, filter, keyword) {
  return useMemo(() => searchTodos(filterTodos(todos, filter), keyword), [todos, filter, keyword]);
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
// 아래 useXxxHandlers는 루트의 최상위에서 부르는 Hook 묶음이다. (호출 순서는 항상 같다)
// Memoization : 핸들러 참조를 고정해야 memo 자식이 props가 그대로라고 판단한다.
// 상태 변경 함수는 렌더링이 바뀌어도 같은 함수이고, 목록 변경은 함수형 갱신을 쓰므로 의존성이 적다.
function useInputHandlers(state, showError) {
  const { draftText, setTodos, setDraftText, setSearchText } = state;

  const onDraftInput = useCallback(
    createSafeHandler(showError, (event) => setDraftText(event.target.value)),
    [setDraftText, showError],
  );
  const onSubmit = useCallback(
    createSafeHandler(showError, (event) => {
      event.preventDefault();
      addTodo({ draftText, setTodos, setDraftText });
    }),
    [draftText, setTodos, setDraftText, showError],
  );
  const onSearchInput = useCallback(
    createSafeHandler(showError, (event) => setSearchText(event.target.value)),
    [setSearchText, showError],
  );

  return { onDraftInput, onSubmit, onSearchInput };
}

function useListHandlers(state, showError) {
  const { setTodos, setFilter } = state;

  const onToggle = useCallback(
    createSafeHandler(showError, (todoId) => setTodos((todos) => toggleTodo(todos, todoId))),
    [setTodos, showError],
  );
  const onRemove = useCallback(
    createSafeHandler(showError, (todoId) => setTodos((todos) => removeTodo(todos, todoId))),
    [setTodos, showError],
  );
  const onFilterChange = useCallback(
    createSafeHandler(showError, (nextFilter) => setFilter(nextFilter)),
    [setFilter, showError],
  );

  return { onToggle, onRemove, onFilterChange };
}

// 핸들러를 감싸 에러를 화면에 보여 준다. 성공하면 이전 에러 표시를 지운다.
function createSafeHandler(showError, handler) {
  return (...args) => {
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
