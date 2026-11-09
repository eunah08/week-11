import { domToVNode, render, diff, applyPatches } from '../../core/index.js';
import { StateHistory } from './StateHistory.js';
import { SAMPLE_HTML } from './sample.js';

/**
 * Phase 1 데모를 시작한다. (Phase 1 문서 9.3, 9.4)
 * @returns {void}
 * @throws {Error} 필요한 화면 요소나 샘플 루트 요소가 없을 때
 */
export function startPhaseOne() { // Phase 1 데모를 시작한다
  const demo = createDemo();

  // 1) 실제 영역에 샘플 DOM을 넣는다. 이후 실제 영역은 applyPatches로만 바뀐다
  demo.realArea.innerHTML = SAMPLE_HTML;

  // 2) 실제 영역의 샘플을 읽어 Virtual DOM을 만든다
  //    영역(컨테이너)과 비교 대상 루트를 구분한다. 루트는 영역 안의 첫 번째 요소다
  demo.realRootNode = demo.realArea.firstElementChild;
  if (demo.realRootNode === null) {
    throw new Error('[startPhaseOne] 실제 영역에 비교할 요소가 없습니다');
  }
  const initialVNode = domToVNode(demo.realRootNode);

  // 3) 같은 Virtual DOM으로 테스트 영역을 그린다
  const testRootNode = render(initialVNode);
  demo.testArea.replaceChildren(testRootNode);

  // 4) 초기 상태를 History에 저장한다
  demo.history = new StateHistory(initialVNode);

  connectButtons(demo);
  updateHistoryControls(demo);
}

function createDemo() { // 화면 요소와 상태를 한 객체로 모은다
  const demo = {
    realArea: getElement('#real-area'),
    testArea: getElement('#test-area'),
    patchButton: getElement('[data-action="patch"]'),
    backButton: getElement('[data-action="back"]'),
    forwardButton: getElement('[data-action="forward"]'),
    historyPosition: getElement('#history-position'),
    errorMessage: getElement('#error-message'),
    // 아래 두 값은 startPhaseOne에서 채운다
    realRootNode: null,
    history: null,
  };
  return demo;
}

function getElement(selector) { // 선택자로 요소를 찾는다 (없으면 에러)
  const element = document.querySelector(selector);
  if (element === null) {
    throw new Error(`[getElement] ${selector} 요소가 없습니다`);
  }
  return element;
}

function connectButtons(demo) { // 세 버튼에 클릭 이벤트를 연결한다
  demo.patchButton.addEventListener('click', () => {
    handlePatchClick(demo);
  });
  demo.backButton.addEventListener('click', () => {
    handleBackClick(demo);
  });
  demo.forwardButton.addEventListener('click', () => {
    handleForwardClick(demo);
  });
}

function handlePatchClick(demo) { // Patch 클릭 처리 (core가 던진 에러는 여기서 잡는다, 9.4·11장)
  clearErrorMessage(demo);
  try {
    patchRealArea(demo);
  } catch (error) {
    showErrorMessage(demo, error);
  }
  updateHistoryControls(demo);
}

function handleBackClick(demo) { // 뒤로가기 클릭 처리
  clearErrorMessage(demo);
  try {
    goBack(demo);
  } catch (error) {
    showErrorMessage(demo, error);
  }
  updateHistoryControls(demo);
}

function handleForwardClick(demo) { // 앞으로가기 클릭 처리
  clearErrorMessage(demo);
  try {
    goForward(demo);
  } catch (error) {
    showErrorMessage(demo, error);
  }
  updateHistoryControls(demo);
}

function patchRealArea(demo) { // 테스트 영역의 변경을 실제 영역에 반영하고 이력에 저장한다
  // 1) 테스트 영역의 현재 DOM을 다시 읽는다
  const testRootNode = demo.testArea.firstElementChild;
  if (testRootNode === null) {
    throw new Error('[patchRealArea] 테스트 영역에 비교할 요소가 없습니다');
  }
  const nextVNode = domToVNode(testRootNode);

  // 2) 현재 이력의 Virtual DOM과 비교한다
  const currentVNode = demo.history.getCurrent();
  const patches = diff(currentVNode, nextVNode);

  // 3) 바뀐 게 없으면 에러가 아니며, 새 이력도 만들지 않는다
  if (patches.length === 0) {
    return;
  }

  // 4) 변경 사항만 실제 영역에 반영하고, 돌려받은 루트를 이후에 쓴다
  demo.realRootNode = applyPatches(demo.realRootNode, patches);

  // 5) 반영에 성공한 뒤에만 새 Virtual DOM을 저장한다
  demo.history.push(nextVNode);
}

function goBack(demo) { // 이력에서 한 칸 뒤로 가서 화면에 보여준다
  if (!demo.history.canGoBack()) {
    return;
  }

  const currentVNode = demo.history.getCurrent();
  const targetVNode = demo.history.back();
  showHistoryRecord(demo, currentVNode, targetVNode);
}

function goForward(demo) { // 이력에서 한 칸 앞으로 가서 화면에 보여준다
  if (!demo.history.canGoForward()) {
    return;
  }

  const currentVNode = demo.history.getCurrent();
  const targetVNode = demo.history.forward();
  showHistoryRecord(demo, currentVNode, targetVNode);
}

function showHistoryRecord(demo, currentVNode, targetVNode) { // 이동한 기록을 실제 영역과 테스트 영역에 보여준다
  // 1) 실제 영역 : 현재 기록과 이동할 기록을 비교해서 바뀐 부분만 반영한다
  const patches = diff(currentVNode, targetVNode);
  demo.realRootNode = applyPatches(demo.realRootNode, patches);

  // 2) 테스트 영역 : 반영하지 않은 수정이 있어도 이동한 기록으로 대체한다
  const testRootNode = render(targetVNode);
  demo.testArea.replaceChildren(testRootNode);
}

function clearErrorMessage(demo) { // 에러 문구를 지운다
  demo.errorMessage.textContent = '';
}

function showErrorMessage(demo, error) { // 에러를 콘솔과 화면에 보여준다
  console.error(error);
  demo.errorMessage.textContent = error.message;
}

function updateHistoryControls(demo) { // 이동 버튼 활성 상태와 이력 위치 문구를 갱신한다
  const canGoBack = demo.history.canGoBack();
  const canGoForward = demo.history.canGoForward();
  demo.backButton.disabled = !canGoBack;
  demo.forwardButton.disabled = !canGoForward;

  const position = demo.history.getPosition();
  const currentNumber = position.index + 1;
  demo.historyPosition.textContent = `이력 ${currentNumber} / ${position.count}`;
}
