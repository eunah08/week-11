import { createElement, memo } from '../../../core/index.js';

/**
 * 할 일 목록을 어떤 기준으로 보여 줄지 정하는 값.
 */
export const FILTERS = Object.freeze({
  ALL: 'all',
  ACTIVE: 'active',
  DONE: 'done',
});

// 필터별로 보여 줄 할 일을 고르는 규칙. 새 필터는 FILTERS, 이 표, FILTER_BUTTONS에 한 줄씩 추가한다.
// 일반 객체 대신 Map을 쓰는 이유는 'toString' 같은 기본 객체 이름을 필터로 착각하지 않게 하기 위해서다.
const SHOW_RULE_BY_FILTER = new Map([
  [FILTERS.ALL, () => true],
  [FILTERS.ACTIVE, (todo) => !todo.isDone],
  [FILTERS.DONE, (todo) => todo.isDone],
]);

// 화면에 보여 줄 필터 버튼과 그 순서.
const FILTER_BUTTONS = [
  { filter: FILTERS.ALL, label: '전체' },
  { filter: FILTERS.ACTIVE, label: '진행 중' },
  { filter: FILTERS.DONE, label: '완료' },
];

/**
 * 필터 기준에 맞는 할 일만 골라 새 배열로 돌려준다. 받은 배열은 바꾸지 않는다.
 * @param {{ id: number, text: string, isDone: boolean }[]} todos 전체 할 일
 * @param {string} filter FILTERS 값
 * @returns {object[]} 필터 기준에 맞는 할 일
 * @throws {Error} 알 수 없는 필터일 때
 */
export function filterTodos(todos, filter) {
  const showRule = SHOW_RULE_BY_FILTER.get(filter);

  if (showRule === undefined) {
    throw new Error(`[filterTodos] 알 수 없는 필터입니다: ${filter}`);
  }

  return todos.filter(showRule);
}

/**
 * 검색어가 들어 있는 할 일만 골라 새 배열로 돌려준다. 대소문자는 구분하지 않고, 검색어가 비었으면 그대로 돌려준다.
 * @param {{ id: number, text: string, isDone: boolean }[]} todos 할 일 목록
 * @param {string} keyword 검색어
 * @returns {object[]} 검색어가 들어 있는 할 일
 */
export function searchTodos(todos, keyword) {
  const normalizedKeyword = keyword.trim().toLowerCase();

  if (normalizedKeyword === '') {
    return todos;
  }

  return todos.filter((todo) => todo.text.toLowerCase().includes(normalizedKeyword));
}

// Memoization
/**
 * 필터 버튼 모음. props만 받는 순수 함수 컴포넌트다.
 * @param {{ currentFilter: string, onChange: Function }} props 현재 필터와 필터 변경 함수
 * @returns {object} VNode
 */
export const todoFilter = memo(({ currentFilter, onChange }) => {
  const buttons = FILTER_BUTTONS.map(({ filter, label }) => {
    return createFilterButton({ filter, label, isActive: filter === currentFilter, onChange });
  });

  return createElement('div', { class: 'todo-filter', role: 'group' }, buttons);
});

function createFilterButton({ filter, label, isActive, onChange }) {
  return createElement(
    'button',
    {
      type: 'button',
      class: isActive ? 'todo-filter-button is-active' : 'todo-filter-button',
      'data-action': 'filter',
      'data-filter': filter,
      'aria-pressed': String(isActive),
      onClick: () => onChange(filter),
    },
    label,
  );
}
