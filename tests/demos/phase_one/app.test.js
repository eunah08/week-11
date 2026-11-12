import { startPhaseOne } from '../../../src/demos/phase_one/app.js';
import { test, assertEqual, assertTrue } from '../../testRunner.js';

function withDemo(runTest) {
  const fixture = document.createElement('div');
  fixture.innerHTML = `
    <button data-action="patch">Patch</button>
    <button data-action="back">뒤로</button>
    <button data-action="forward">앞으로</button>
    <span id="history-position"></span>
    <p id="error-message"></p>
    <fieldset id="real-area" disabled></fieldset>
    <div id="test-area"></div>`;
  document.body.append(fixture);
  try {
    startPhaseOne();
    runTest(fixture);
  } finally {
    fixture.remove();
  }
}

test('Phase 1 : 편집 속성은 테스트 카드에만 있고 이력 이동 뒤에도 유지된다', () => {
  withDemo((fixture) => {
    const editor = fixture.querySelector('#test-area > section');
    assertEqual(editor.getAttribute('contenteditable'), 'true');
    editor.querySelector('h3').textContent = '수정한 제목';
    fixture.querySelector('[data-action="patch"]').click();

    const realRoot = fixture.querySelector('#real-area > section');
    assertEqual(realRoot.querySelector('h3').textContent, '수정한 제목');
    assertEqual(realRoot.hasAttribute('contenteditable'), false);

    fixture.querySelector('[data-action="back"]').click();
    fixture.querySelector('[data-action="forward"]').click();
    const restoredEditor = fixture.querySelector('#test-area > section');
    assertEqual(restoredEditor.getAttribute('contenteditable'), 'true');
    assertEqual(fixture.querySelector('#test-area h3').textContent, '수정한 제목');
  });
});

test('Phase 1 : 추가 루트가 있으면 실제 영역과 이력을 그대로 두고 편집을 보존한다', () => {
  withDemo((fixture) => {
    const editor = fixture.querySelector('#test-area');
    editor.querySelector('h3').textContent = '수정한 제목';
    editor.append(document.createTextNode('카드 밖 편집'));
    const beforePatch = editor.innerHTML;
    const originalTitle = fixture.querySelector('#real-area h3').textContent;
    const reportError = console.error;

    try {
      console.error = () => {};
      fixture.querySelector('[data-action="patch"]').click();
    } finally {
      console.error = reportError;
    }

    assertTrue(fixture.querySelector('#error-message').textContent.includes('카드'));
    assertEqual(fixture.querySelector('#real-area h3').textContent, originalTitle);
    assertEqual(fixture.querySelector('#history-position').textContent, '이력 1 / 1');
    assertEqual(editor.innerHTML, beforePatch);
  });
});

test('Phase 1 : 테스트 체크 상태는 Patch와 이력 이동에 반영된다', () => {
  withDemo((fixture) => {
    const realCheckbox = () => fixture.querySelector('#real-area input');
    const testCheckbox = () => fixture.querySelector('#test-area input');
    assertTrue(realCheckbox().matches(':disabled'), '실제 영역은 미리보기다');

    testCheckbox().click();
    fixture.querySelector('[data-action="patch"]').click();
    assertEqual(realCheckbox().checked, false);

    fixture.querySelector('[data-action="back"]').click();
    assertEqual([realCheckbox().checked, testCheckbox().checked], [true, true]);
    fixture.querySelector('[data-action="forward"]').click();
    assertEqual([realCheckbox().checked, testCheckbox().checked], [false, false]);
  });
});
