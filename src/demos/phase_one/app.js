import { domToVNode, render, diff, applyPatches, isIgnoredNode } from '../../core/index.js';
import { StateHistory } from './StateHistory.js';
import { SAMPLE_HTML } from './sample.js';

// 화면 요소를 찾는 선택자. HTML의 id나 data-action이 바뀌면 이 표만 고친다. (Phase 1 문서 9.1)
const PAGE_SELECTORS = {
  realArea: '#real-area',
  testArea: '#test-area',
  patchButton: '[data-action="patch"]',
  backButton: '[data-action="back"]',
  forwardButton: '[data-action="forward"]',
  historyPosition: '#history-position',
  errorMessage: '#error-message',
};

// 버튼과 동작의 연결. 새 버튼은 PAGE_SELECTORS와 이 표에 한 줄씩 추가한다.
const BUTTON_ACTIONS = [
  { buttonName: 'patchButton', run: patchRealArea },
  { buttonName: 'backButton', run: (elements, state) => moveHistory(elements, state, 'back') },
  {
    buttonName: 'forwardButton',
    run: (elements, state) => moveHistory(elements, state, 'forward'),
  },
];

// 이력 이동 방향(StateHistory의 메서드 이름)과 그 반대 방향.
// 실제 영역 반영에 실패하면 반대 방향으로 한 칸 움직여 이력 위치를 되돌린다.
const OPPOSITE_DIRECTION = { back: 'forward', forward: 'back' };

/**
 * Phase 1 데모를 시작한다. (Phase 1 문서 9.3, 9.4)
 * 실제 영역에 샘플을 넣고, 테스트 영역을 같은 VNode로 그린 뒤 버튼 이벤트를 연결한다.
 * @returns {void}
 * @throws {Error} 화면 요소나 비교할 루트 노드를 찾을 수 없을 때
 */
export function startPhaseOne() {
  const elements = getPageElements();
  const state = initializeAreas(elements);

  for (const { buttonName, run } of BUTTON_ACTIONS) {
    elements[buttonName].addEventListener('click', () => {
      runSafely(elements, () => run(elements, state));
    });
  }

  updateHistoryControls(elements, state.history);
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
    throw new Error(`[startPhaseOne] 화면 요소를 찾을 수 없습니다: ${selector}`);
  }

  return element;
}

// 9.3 초기화 : 실제 영역 DOM 읽기 → VNode → 테스트 영역 렌더링 → History 저장
function initializeAreas({ realArea, testArea }) {
  const template = document.createElement('template');

  // innerHTML은 코드에 직접 적은 SAMPLE_HTML에만 쓴다. 사용자 입력을 넣으면 스크립트가 실행될 수 있다.
  template.innerHTML = SAMPLE_HTML;
  realArea.append(template.content);

  // 화면을 담는 컨테이너(#real-area)와 비교 대상 루트(그 안의 첫 노드)를 구분한다.
  const realRoot = getComparisonRoot(realArea);
  const initialVNode = domToVNode(realRoot);

  testArea.replaceChildren(render(initialVNode));

  return {
    realRoot,
    history: new StateHistory(initialVNode),
  };
}

// 9.4 Patch : 테스트 영역 다시 읽기 → 현재 이력과 diff → 실제 영역에 반영 → History 저장
function patchRealArea(elements, state) {
  const nextVNode = domToVNode(getComparisonRoot(elements.testArea));
  const patches = diff(state.history.getCurrent(), nextVNode);

  // 바뀐 게 없으면 DOM도 History도 건드리지 않는다.
  if (patches.length === 0) {
    return;
  }

  state.realRoot = applyPatches(state.realRoot, patches);
  state.history.push(nextVNode);
  updateHistoryControls(elements, state.history);
}

// 9.4 이력 이동 : 이동할 VNode와 현재 VNode를 diff해 실제 영역에 반영하고 테스트 영역은 다시 그린다.
function moveHistory(elements, state, direction) {
  const { history } = state;
  const currentVNode = history.getCurrent();
  const targetVNode = history[direction]();

  if (targetVNode === null) {
    return;
  }

  try {
    state.realRoot = applyPatches(state.realRoot, diff(currentVNode, targetVNode));
  } catch (error) {
    // 반영에 실패하면 이력 위치를 되돌려, 화면은 그대로인데 위치만 옮겨진 상태를 막는다.
    history[OPPOSITE_DIRECTION[direction]]();
    throw error;
  }

  // 테스트 영역에만 있던 미반영 수정은 선택한 이력으로 대체한다.
  elements.testArea.replaceChildren(render(targetVNode));
  updateHistoryControls(elements, history);
}

function getComparisonRoot(container) {
  const rootNode = Array.from(container.childNodes).find((childNode) => !isIgnoredNode(childNode));

  if (!rootNode) {
    throw new Error('[startPhaseOne] 비교할 루트 노드가 없습니다. 가장 바깥 상자를 지우지 마세요');
  }

  return rootNode;
}

function updateHistoryControls({ backButton, forwardButton, historyPosition }, history) {
  backButton.disabled = !history.canGoBack();
  forwardButton.disabled = !history.canGoForward();
  historyPosition.textContent = `이력 ${history.getPosition()} / ${history.getCount()}`;
}

// 버튼 핸들러의 에러를 화면과 콘솔에 함께 남긴다. (Phase 1 문서 9.4, 11장)
// 에러 메시지에 사용자가 넣은 속성 이름이 들어갈 수 있어 innerHTML이 아닌 textContent로 보여 준다.
function runSafely({ errorMessage }, action) {
  try {
    action();
    errorMessage.textContent = '';
  } catch (error) {
    errorMessage.textContent = error.message;
    console.error(error);
  }
}
