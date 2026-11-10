import { FunctionComponent } from '../../core/index.js';
import { todoApp } from './components/todoApp.js';

// 화면 요소를 찾는 선택자. HTML의 data-role이 바뀌면 이 표만 고친다.
const PAGE_SELECTORS = {
  container: '[data-role="todo-root"]',
  errorMessage: '[data-role="todo-error"]',
};

/**
 * Phase 2 데모(할 일 목록)를 시작한다. (Phase 2 문서 11장)
 * 루트 컴포넌트를 FunctionComponent로 mount한다.
 * @returns {void}
 * @throws {Error} 화면 요소를 찾을 수 없을 때
 */
export function startPhaseTwo() {
  const { container, errorMessage } = getPageElements();
  const props = { showError: (error) => showError(errorMessage, error) };

  const instance = new FunctionComponent(todoApp, props, container);

  // Batching : 예약된 갱신에서 난 에러는 이벤트 핸들러의 try/catch가 잡지 못하므로 여기로 받는다.
  instance.onError = props.showError;
  instance.mount();
}

function getPageElements() {
  const elements = {};

  for (const [elementName, selector] of Object.entries(PAGE_SELECTORS)) {
    elements[elementName] = getRequiredElement(selector);
  }

  return elements;
}

// HTML에서 요소 이름이 바뀌었을 때 나중에 엉뚱한 곳에서 터지지 않고 여기서 바로 알린다.
function getRequiredElement(selector) {
  const element = document.querySelector(selector);

  if (element === null) {
    throw new Error(`[startPhaseTwo] 화면 요소를 찾을 수 없습니다: ${selector}`);
  }

  return element;
}

// 에러 메시지는 textContent로 보여 준다. (Phase 2 문서 12장, null이면 표시를 지운다)
function showError(errorMessage, error) {
  if (error === null) {
    errorMessage.textContent = '';
    return;
  }

  errorMessage.textContent = error.message;
  console.error(error);
}
