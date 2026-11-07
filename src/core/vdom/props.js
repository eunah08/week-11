// VNode props를 실제 DOM에 넣고 빼는 규칙 (Phase 1 문서 6.3, 8.4)
// render와 applyPatches가 같은 규칙을 쓰도록 이 파일에 모은다.

const EVENT_PREFIX = 'on';

// 속성뿐 아니라 DOM 프로퍼티에도 현재 상태가 저장되는 키
const PROPERTY_KEYS = ['value', 'checked'];

/**
 * DOM 노드에 속성 하나를 설정한다. 이벤트면 이전 리스너를 빼고 새 리스너를 단다.
 * domNode를 직접 수정한다.
 * @param {Element} domNode 대상 요소
 * @param {string} key 속성 이름 (이벤트는 onClick 형식)
 * @param {string | boolean | Function | null | undefined} value 속성 값
 * @returns {void}
 * @throws {TypeError} 문자열·불리언·함수·null·undefined가 아닌 값일 때
 */
export function setProp(domNode, key, value) {
  if (isEventProp(key, value)) {
    updateEventListener(domNode, toEventName(key), value);
    return;
  }

  if (value === false || value === null || value === undefined) {
    removeProp(domNode, key);
    return;
  }

  if (value !== true && typeof value !== 'string') {
    throw new TypeError(`[setProp] 지원하지 않는 속성 값입니다: ${key}`);
  }

  const attributeValue = value === true ? '' : value;

  domNode.setAttribute(key, attributeValue);
  updateDomProperty(domNode, key, attributeValue);
}

/**
 * DOM 노드에서 속성 하나를 제거한다. 이벤트면 리스너를 뺀다.
 * domNode를 직접 수정한다.
 * @param {Element} domNode 대상 요소
 * @param {string} key 제거할 속성 이름
 * @returns {void}
 */
export function removeProp(domNode, key) {
  const handlers = domNode.handlers;
  const eventName = toEventName(key);

  if (key.startsWith(EVENT_PREFIX) && handlers && handlers[eventName]) {
    domNode.removeEventListener(eventName, handlers[eventName]);
    delete handlers[eventName];
    return;
  }

  domNode.removeAttribute(key);
  updateDomProperty(domNode, key, null);
}

function isEventProp(key, value) {
  return key.startsWith(EVENT_PREFIX) && typeof value === 'function';
}

function toEventName(key) {
  return key.slice(EVENT_PREFIX.length).toLowerCase();
}

// 이전 핸들러를 DOM의 handlers 객체에 기억해 두어 중복 등록을 막는다. (8.4 권장 방식)
function updateEventListener(domNode, eventName, handler) {
  if (!domNode.handlers) {
    domNode.handlers = {};
  }

  const previousHandler = domNode.handlers[eventName];

  if (previousHandler) {
    domNode.removeEventListener(eventName, previousHandler);
  }

  domNode.addEventListener(eventName, handler);
  domNode.handlers[eventName] = handler;
}

// 입력창의 현재 값이 속성과 어긋나지 않도록 프로퍼티도 맞춘다. (null은 제거)
function updateDomProperty(domNode, key, attributeValue) {
  if (!PROPERTY_KEYS.includes(key) || !(key in domNode)) {
    return;
  }

  if (key === 'checked') {
    domNode.checked = attributeValue !== null;
    return;
  }

  domNode.value = attributeValue === null ? '' : attributeValue;
}
