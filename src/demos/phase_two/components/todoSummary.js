import { createElement } from '../../../core/index.js';

/**
 * 남은 할 일 개수 안내. props만 받는 순수 함수 컴포넌트다.
 * @param {{ remainingCount: number, totalCount: number }} props 남은 개수와 전체 개수
 * @returns {object} VNode
 */
export function todoSummary({ remainingCount, totalCount }) {
  return createElement(
    'p',
    { class: 'todo-summary' },
    `남은 할 일 ${remainingCount}개 / 전체 ${totalCount}개`,
  );
}
