import { createElement } from '../../../core/index.js';
import { todoItem } from './todoItem.js';

/**
 * 할 일 목록. 보여 줄 할 일이 없으면 안내 문구를 보여 준다. props만 받는 순수 함수 컴포넌트다.
 * @param {{ todos: object[], onToggle: Function, onRemove: Function }} props
 *   보여 줄 할 일과 완료 토글·삭제 함수
 * @returns {object} VNode
 */
export function todoList({ todos, onToggle, onRemove }) {
  if (todos.length === 0) {
    return createElement('p', { class: 'todo-empty' }, '보여 줄 할 일이 없어요.');
  }

  const items = todos.map((todo) => createElement(todoItem, { todo, onToggle, onRemove }));

  return createElement('ul', { class: 'todo-list' }, items);
}
