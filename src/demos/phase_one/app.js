import {
  domToVNode, render, diff, applyPatches, isIgnoredNode, PATCH_TYPES,
} from '../../core/index.js';
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

  elements.testArea.addEventListener('change', syncTestCheckbox);
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

  // 화면 컨테이너와 그 안의 비교 대상 카드를 구분한다.
  const realRoot = getComparisonRoot(realArea);
  const initialVNode = domToVNode(realRoot);

  return {
    realRoot,
    testRoot: renderTestRoot(testArea, initialVNode),
    history: new StateHistory(initialVNode),
  };
}

// 카드 자체를 편집 영역으로 만들어 새 줄도 카드 안에 남긴다.
function renderTestRoot(testArea, vnode) {
  const editorVNode = { ...vnode, props: { ...vnode.props, contenteditable: 'true' } };
  const rootNode = render(editorVNode);

  testArea.replaceChildren(rootNode);
  return rootNode;
}

// 테스트용 편집 속성은 실제 영역과 이력에 저장하지 않는다.
function readTestVNode(testArea, expectedRoot) {
  const rootNode = getComparisonRoot(testArea);
  if (rootNode !== expectedRoot) {
    throw new Error('비교할 카드가 바뀌었어요. 기존 카드 안에서 편집해 주세요');
  }

  const vnode = domToVNode(rootNode);
  const props = { ...vnode.props };
  delete props.contenteditable;
  return { ...vnode, props };
}

// 브라우저가 바꾼 체크값을 속성에도 기록해야 Patch와 이력에 반영된다.
function syncTestCheckbox({ target }) {
  if (!(target instanceof Element) || !target.matches('input[type="checkbox"]')) {
    return;
  }

  applyPatches(target, [
    { type: PATCH_TYPES.UPDATE_PROPS, path: [], set: { checked: target.checked }, remove: [] },
  ]);
}

// 9.4 Patch : 테스트 영역 다시 읽기 → 현재 이력과 diff → 실제 영역에 반영 → History 저장
function patchRealArea(elements, state) {
  const nextVNode = readTestVNode(elements.testArea, state.testRoot);
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
  state.testRoot = renderTestRoot(elements.testArea, targetVNode);
  updateHistoryControls(elements, history);
}

function getComparisonRoot(container) {
  const roots = Array.from(container.childNodes).filter((childNode) => !isIgnoredNode(childNode));

  if (roots.length !== 1 || !(roots[0] instanceof Element)) {
    throw new Error('비교할 카드가 하나 있어야 해요. 가장 바깥 카드를 유지해 주세요');
  }

  return roots[0];
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
