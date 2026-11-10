import { createElement } from '../../../core/index.js';

/**
 * 할 일 입력 폼. props만 받는 순수 함수 컴포넌트다.
 * 입력창의 값은 draftText가 정하므로, 글자를 쳐도 입력창 DOM은 교체되지 않는다.
 * @param {{ draftText: string, onDraftInput: Function, onSubmit: Function }} props
 *   입력 중인 글자, 입력 이벤트와 제출 이벤트를 처리하는 함수
 * @returns {object} VNode
 */
export function todoForm({ draftText, onDraftInput, onSubmit }) {
  return createElement(
    'form',
    { class: 'todo-form', onSubmit },
    createElement('label', { class: 'todo-form-label', for: 'todo-input' }, '할 일'),
    createElement('input', {
      id: 'todo-input',
      class: 'todo-input',
      type: 'text',
      value: draftText,
      placeholder: '할 일을 입력하세요',
      autocomplete: 'off',
      onInput: onDraftInput,
    }),
    createElement(
      'button',
      { type: 'submit', class: 'todo-add-button', 'data-action': 'add' },
      '추가',
    ),
  );
}
