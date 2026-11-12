import { render, applyPatches, PATCH_TYPES } from '../../../src/core/index.js';
import { test, assertThrows, assertTrue } from '../../testRunner.js';

function element(tag, props = {}, children = []) {
  return { type: 'element', tag, props, children };
}

function propsPatch(set, remove) {
  return { type: PATCH_TYPES.UPDATE_PROPS, path: [], set, remove };
}

test('props : 이벤트 키에 문자열이 오면 TypeError (문자열 코드 실행 방지)', () => {
  const message = '[setProp] 이벤트 값은 함수여야 합니다';

  assertThrows(() => render(element('img', { onerror: 'alert(1)' })), TypeError, message);
  assertThrows(() => render(element('img', { ONERROR: 'alert(1)' })), TypeError, message);
  assertThrows(() => render(element('button', { onClick: true })), TypeError, message);
});

test('props : 이벤트 키에 null·false가 오면 리스너를 뺀다', () => {
  let clickCount = 0;
  const domNode = render(element('button', { onClick: () => { clickCount += 1; } }));
  const container = document.createElement('div');

  container.append(domNode);
  applyPatches(domNode, [propsPatch({ onClick: null }, [])]);
  domNode.click();
  assertTrue(clickCount === 0, '리스너가 남음');
});

test('props : 이벤트를 제거하면 HTML로 들어 있던 문자열 이벤트 속성도 지운다', () => {
  const container = document.createElement('div');
  const rootNode = document.createElement('button');

  // 직접 넣은 HTML에 문자열 이벤트 속성이 있던 상황을 만든다. (10.1 : HTML 문자열에 이벤트 코드를 쓰지 않음)
  rootNode.setAttribute('onclick', 'void 0');
  container.append(rootNode);
  applyPatches(rootNode, [propsPatch({}, ['onclick'])]);
  assertTrue(!rootNode.hasAttribute('onclick'), 'onclick 속성이 남음');
});
