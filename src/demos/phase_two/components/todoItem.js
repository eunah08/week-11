import { createElement } from '../../../core/index.js';

/**
 * 할 일 한 줄. props만 받는 순수 함수 컴포넌트다.
 * @param {{
 *   todo: { id: number, text: string, isDone: boolean },
 *   onToggle: Function,
 *   onRemove: Function,
 * }} props 할 일과 완료 토글·삭제 함수
 * @returns {object} VNode
 */
export function todoItem({ todo, onToggle, onRemove }) {
  return createElement(
    'li',
    { class: todo.isDone ? 'todo-item is-done' : 'todo-item' },
    createElement(
      'label',
      { class: 'todo-label' },
      createElement('input', {
        type: 'checkbox',
        checked: todo.isDone,
        onChange: () => onToggle(todo.id),
      }),
      createElement('span', { class: 'todo-text' }, todo.text),
    ),
    createElement(
      'button',
      {
        type: 'button',
        class: 'todo-remove-button',
        'data-action': 'remove',
        'aria-label': `${todo.text} 삭제`,
        onClick: () => onRemove(todo.id),
      },
      '삭제',
    ),
  );
}
