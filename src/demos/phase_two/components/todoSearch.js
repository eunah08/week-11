import { createElement, memo } from '../../../core/index.js';

// Memoization
/**
 * 검색창. props만 받는 순수 함수 컴포넌트다.
 * 입력창의 값(searchText)은 글자를 칠 때마다 바로 바뀌고, 목록은 입력이 멈춘 뒤에 걸러진다.
 * @param {{ searchText: string, isPending: boolean, onSearchInput: Function }} props
 *   입력 중인 검색어, 아직 목록에 반영되지 않았는지 여부, 입력 이벤트를 처리하는 함수
 * @returns {object} VNode
 */
export const todoSearch = memo(({ searchText, isPending, onSearchInput }) => {
  return createElement(
    'div',
    { class: 'todo-search' },
    createElement('label', { class: 'todo-search-label', for: 'todo-search-input' }, '검색'),
    createElement('input', {
      id: 'todo-search-input',
      class: 'todo-search-input',
      type: 'search',
      value: searchText,
      placeholder: '검색어를 입력하세요',
      autocomplete: 'off',
      onInput: onSearchInput,
    }),
    createElement('span', { class: 'todo-search-status' }, isPending ? '입력이 멈추면 검색해요' : ''),
  );
});
