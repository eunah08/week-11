import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { JSDOM } from 'jsdom';
import { startPhaseOne } from '../../../src/demos/phase_one/app.js';

// npm test는 저장소 루트에서 실행되므로 루트 기준 경로로 index.html을 읽는다
const INDEX_HTML = readFileSync('index.html', 'utf-8');

function openPage() { // index.html로 화면을 만들고 데모를 시작한다
  const jsdom = new JSDOM(INDEX_HTML);
  globalThis.document = jsdom.window.document;
  globalThis.Node = jsdom.window.Node;

  startPhaseOne();

  const page = {
    realArea: document.querySelector('#real-area'),
    testArea: document.querySelector('#test-area'),
    errorMessage: document.querySelector('#error-message'),
    historyPosition: document.querySelector('#history-position'),
    patchButton: document.querySelector('[data-action="patch"]'),
    backButton: document.querySelector('[data-action="back"]'),
    forwardButton: document.querySelector('[data-action="forward"]'),
  };
  return page;
}

function getTitleText(area) { // 영역 안 제목의 글자를 돌려준다
  const titleNode = area.querySelector('h1');
  return titleNode.textContent;
}

function editTitleAndPatch(page, newTitle) { // 테스트 영역의 제목을 바꾸고 Patch 버튼을 누른다
  const titleNode = page.testArea.querySelector('h1');
  titleNode.textContent = newTitle;
  page.patchButton.click();
}

test('초기화 : 두 영역이 같은 구조로 그려지고 이동 버튼은 비활성이다', () => {
  const page = openPage();

  assert.equal(getTitleText(page.testArea), getTitleText(page.realArea));
  assert.equal(page.backButton.disabled, true);
  assert.equal(page.forwardButton.disabled, true);
  assert.equal(page.historyPosition.textContent, '이력 1 / 1');
});

test('테스트 영역 수정 후 Patch하면 실제 영역에 변경만 반영된다', () => {
  const page = openPage();
  const realListNode = page.realArea.querySelector('ul');

  editTitleAndPatch(page, '바뀐 제목');

  assert.equal(getTitleText(page.realArea), '바뀐 제목');
  assert.equal(page.realArea.querySelector('ul'), realListNode);
  assert.equal(page.historyPosition.textContent, '이력 2 / 2');
  assert.equal(page.backButton.disabled, false);
});

test('항목을 추가·삭제해도 실제 영역이 같은 구조가 된다', () => {
  const page = openPage();
  const testListNode = page.testArea.querySelector('ul');
  const newItem = document.createElement('li');
  newItem.textContent = '네 번째 항목';
  testListNode.appendChild(newItem);
  testListNode.firstElementChild.remove();

  page.patchButton.click();

  const realItems = page.realArea.querySelectorAll('li');
  const realItemTexts = [];
  for (const item of realItems) {
    realItemTexts.push(item.textContent);
  }
  assert.deepEqual(realItemTexts, ['두 번째 항목', '세 번째 항목', '네 번째 항목']);
});

test('뒤로가기·앞으로가기 시 두 영역이 함께 바뀐다', () => {
  const page = openPage();
  editTitleAndPatch(page, '두 번째 상태');

  page.backButton.click();
  assert.equal(getTitleText(page.realArea), '미니 리액트');
  assert.equal(getTitleText(page.testArea), '미니 리액트');
  assert.equal(page.forwardButton.disabled, false);

  page.forwardButton.click();
  assert.equal(getTitleText(page.realArea), '두 번째 상태');
  assert.equal(getTitleText(page.testArea), '두 번째 상태');
  assert.equal(page.forwardButton.disabled, true);
});

test('뒤로가기 후 새 Patch를 하면 앞으로가기 기록이 사라진다', () => {
  const page = openPage();
  editTitleAndPatch(page, 'B');
  editTitleAndPatch(page, 'C');
  page.backButton.click();
  page.backButton.click();

  editTitleAndPatch(page, 'D');

  assert.equal(page.forwardButton.disabled, true);
  assert.equal(page.historyPosition.textContent, '이력 2 / 2');
});

test('이력 이동 시 테스트 영역의 미반영 수정은 선택한 이력으로 대체된다', () => {
  const page = openPage();
  editTitleAndPatch(page, 'B');
  const titleNode = page.testArea.querySelector('h1');
  titleNode.textContent = '반영 안 한 수정';

  page.backButton.click();

  assert.equal(getTitleText(page.testArea), '미니 리액트');
});

test('변경이 없으면 새 이력을 만들지 않는다', () => {
  const page = openPage();

  page.patchButton.click();

  assert.equal(page.historyPosition.textContent, '이력 1 / 1');
  assert.equal(page.errorMessage.textContent, '');
});

test('연속 조작 후에도 버튼 이벤트가 중복 실행되지 않는다', () => {
  const page = openPage();
  editTitleAndPatch(page, 'B');
  page.backButton.click();
  page.forwardButton.click();
  page.backButton.click();
  page.forwardButton.click();

  // 뒤로가기 한 번이 정확히 한 칸만 움직여야 한다
  page.backButton.click();

  assert.equal(page.historyPosition.textContent, '이력 1 / 2');
  assert.equal(getTitleText(page.realArea), '미니 리액트');
});

test('에러는 화면과 콘솔에 보여 주고 이력은 그대로 둔다', () => {
  const page = openPage();
  page.testArea.replaceChildren();

  // 테스트 출력이 지저분해지지 않게 console.error를 잠깐 바꿔서 호출 횟수만 센다
  const originalConsoleError = console.error;
  let consoleErrorCount = 0;
  console.error = function countConsoleError() {
    consoleErrorCount = consoleErrorCount + 1;
  };
  try {
    page.patchButton.click();
  } finally {
    console.error = originalConsoleError;
  }

  assert.match(page.errorMessage.textContent, /테스트 영역에 비교할 요소가 없습니다/);
  assert.equal(consoleErrorCount, 1);
  assert.equal(page.historyPosition.textContent, '이력 1 / 1');
});
