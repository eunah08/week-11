// VNode props를 실제 DOM에 넣고 빼는 규칙 (Phase 1 문서 6.3, 8.4)
// render와 applyPatches가 모두 이 파일을 거치므로, 속성 규칙과 보안 검사는 이 파일만 고친다.

const EVENT_PREFIX = 'on';

// 속성뿐 아니라 DOM 프로퍼티에도 현재 상태가 저장되는 키와, 프로퍼티를 맞추는 방법.
// attributeValue가 null이면 속성이 제거된 것이다. 새 키(예: selected)는 이 표에 한 줄을 추가한다.
const PROPERTY_SYNCER_BY_KEY = new Map([
  ['value', (domNode, attributeValue) => {
    domNode.value = attributeValue === null ? '' : attributeValue;
  }],
  ['checked', (domNode, attributeValue) => {
    domNode.checked = attributeValue !== null;
  }],
]);

/**
 * DOM 노드에 속성 하나를 설정한다. 이벤트면 이전 리스너를 빼고 새 리스너를 단다.
 * domNode를 직접 수정한다.
 * @param {Element} domNode 대상 요소
 * @param {string} key 속성 이름 (이벤트는 onClick 형식)
 * @param {string | boolean | Function | null | undefined} value 속성 값
 * @returns {void}
 * @throws {TypeError} 지원하지 않는 값이거나, 이벤트 키에 함수가 아닌 값(문자열 등)이 올 때
 */
export function setProp(domNode, key, value) {
  if (isEventKey(key)) {
    setEventProp(domNode, key, value);
    return;
  }

  if (isAbsentValue(value)) {
    removeProp(domNode, key);
    return;
  }

  if (value !== true && typeof value !== 'string') {
    throw new TypeError(`[setProp] 지원하지 않는 속성 값입니다: ${key}`);
  }

  const attributeValue = value === true ? '' : value;

  domNode.setAttribute(key, attributeValue);
  syncDomProperty(domNode, key, attributeValue);
}

/**
 * DOM 노드에서 속성 하나를 제거한다. 이벤트면 리스너를 뺀다.
 * domNode를 직접 수정한다.
 * @param {Element} domNode 대상 요소
 * @param {string} key 제거할 속성 이름
 * @returns {void}
 */
export function removeProp(domNode, key) {
  if (isEventKey(key)) {
    removeEventHandler(domNode, key);
    return;
  }

  domNode.removeAttribute(key);
  syncDomProperty(domNode, key, null);
}

/**
 * 현재 상태가 프로퍼티에도 저장되는 속성(value, checked)을 props 값으로 다시 맞춘다.
 * <select>의 value는 <option>이 붙은 뒤에야 먹히므로, 자식을 붙인 다음에 부른다.
 * @param {Element} domNode 대상 요소
 * @param {object} props 방금 설정한 props
 * @returns {void}
 */
export function restoreCurrentProps(domNode, props) {
  for (const key of PROPERTY_SYNCER_BY_KEY.keys()) {
    if (Object.hasOwn(props, key)) {
      setProp(domNode, key, props[key]);
    }
  }
}

// HTML 속성 이름은 대소문자를 가리지 않으므로 ONCLICK도 이벤트로 본다.
function isEventKey(key) {
  return key.toLowerCase().startsWith(EVENT_PREFIX);
}

// 6.3 : false, null, undefined는 속성이 없는 것으로 본다.
function isAbsentValue(value) {
  return value === false || value === null || value === undefined;
}

function toEventName(key) {
  return key.slice(EVENT_PREFIX.length).toLowerCase();
}

// 6.3 : 이벤트 값은 함수만 받는다.
// 문자열을 onclick 같은 속성으로 넣으면 브라우저가 코드로 실행하므로(XSS) 막는다.
function setEventProp(domNode, key, value) {
  if (typeof value === 'function') {
    updateEventHandler(domNode, key, value);
    return;
  }

  if (isAbsentValue(value)) {
    removeEventHandler(domNode, key);
    return;
  }

  throw new TypeError(`[setProp] 이벤트 값은 함수여야 합니다: ${key}`);
}

// 이전 핸들러를 DOM의 handlers 객체에 기억해 두어 중복 등록을 막는다. (8.4 권장 방식)
function updateEventHandler(domNode, key, handler) {
  removeEventHandler(domNode, key);

  if (!domNode.handlers) {
    // 이벤트 이름이 constructor 같은 이름이어도 기본 객체 값과 섞이지 않게 한다.
    domNode.handlers = Object.create(null);
  }

  const eventName = toEventName(key);

  domNode.addEventListener(eventName, handler);
  domNode.handlers[eventName] = handler;
}

function removeEventHandler(domNode, key) {
  const eventName = toEventName(key);
  const previousHandler = domNode.handlers ? domNode.handlers[eventName] : undefined;

  if (previousHandler) {
    domNode.removeEventListener(eventName, previousHandler);
    delete domNode.handlers[eventName];
  }

  // HTML로 직접 넣은 문자열 이벤트 속성(onclick="...")이 남아 있을 수 있어 함께 지운다.
  domNode.removeAttribute(key);
}

// 입력창의 현재 값이 속성과 어긋나지 않도록 프로퍼티도 맞춘다.
function syncDomProperty(domNode, key, attributeValue) {
  const syncProperty = PROPERTY_SYNCER_BY_KEY.get(key);

  if (!syncProperty || !(key in domNode)) {
    return;
  }

  syncProperty(domNode, attributeValue);
}
