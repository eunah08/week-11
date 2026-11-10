import assert from 'node:assert/strict';
import { test } from 'node:test';
import { JSDOM } from 'jsdom';

// 브라우저용 테스트를 Node에서도 돌린다. (npm test)
// 코어 코드가 document, Element 같은 브라우저 값을 쓰므로, 테스트를 불러오기 전에 jsdom으로 채워 둔다.
// 탭 제목 테스트가 있어 브라우저 테스트 페이지처럼 title을 넣어 둔다.
const TEST_PAGE_HTML = '<!DOCTYPE html><html>'
  + '<head><title>미니 리액트 테스트</title></head><body></body></html>';
const { window } = new JSDOM(TEST_PAGE_HTML);

// 테스트에서 쓰는 브라우저 값들. 새 브라우저 값을 쓰는 테스트를 만들면 이 목록에 이름을 추가한다.
const BROWSER_GLOBALS = [
  'window', 'document', 'Node', 'Element', 'HTMLElement', 'Text', 'Comment',
  'Event', 'MouseEvent', 'KeyboardEvent', 'InputEvent', 'MutationObserver',
];

for (const name of BROWSER_GLOBALS) {
  globalThis[name] = name === 'window' ? window : window[name];
}

const { runTests } = await import('./index.js');
const results = runTests();

window.close();

// 브라우저 테스트 결과 하나를 Node 테스트 하나로 바꿔 등록한다.
for (const { name, isPassed, errorMessage } of results) {
  test(name, () => assert.ok(isPassed, errorMessage));
}
