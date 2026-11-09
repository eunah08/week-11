// render와 applyPatches가 함께 쓰는 속성·이벤트 적용 보조 함수 (Phase 1 문서 6.3, 8.4)

const EVENT_PREFIX = 'on';

// 현재 값이 속성이 아니라 DOM 프로퍼티에 저장되는 키 (6.3)
const VALUE_KEY = 'value';
const CHECKED_KEY = 'checked';

/**
 * DOM 노드에 속성 하나를 설정한다. (domNode를 직접 변경한다)
 * @param {Element} domNode 대상 요소
 * @param {string} key 속성 이름 (onClick 같은 이벤트 포함)
 * @param {string | boolean | Function | null | undefined} value 속성 값
 * @returns {void}
 */
export function setProp(domNode, key, value) { // 속성 하나를 DOM에 설정한다 (이벤트면 리스너를 단다)
  // 이전 값이 이벤트였다면 먼저 떼어서 중복 등록을 막는다
  if (hasEventHandler(domNode, key)) {
    removeEventHandler(domNode, key);
  }

  const isEvent = isEventProp(key, value);
  const isEmptyValue = value === false || value === null || value === undefined;

  if (isEvent) {
    addEventHandler(domNode, key, value);
  } else if (isEmptyValue) {
    // false, null, undefined는 속성이 없는 것으로 본다
    domNode.removeAttribute(key);
    resetDomProperty(domNode, key);
  } else if (value === true) {
    // true는 값 없이 속성만 있는 것으로 본다
    domNode.setAttribute(key, '');
    updateDomProperty(domNode, key, '');
  } else {
    domNode.setAttribute(key, value);
    updateDomProperty(domNode, key, value);
  }
}

/**
 * DOM 노드에서 속성 하나를 제거한다. 이벤트면 리스너를 뗀다. (domNode를 직접 변경한다)
 * @param {Element} domNode 대상 요소
 * @param {string} key 제거할 속성 이름
 * @returns {void}
 */
export function removeProp(domNode, key) { // 속성 하나를 DOM에서 지운다 (이벤트면 리스너를 뗀다)
  if (hasEventHandler(domNode, key)) {
    removeEventHandler(domNode, key);
  } else {
    domNode.removeAttribute(key);
    resetDomProperty(domNode, key);
  }
}

function isEventProp(key, value) { // on으로 시작하고 값이 함수인지 확인한다
  const startsWithOn = key.startsWith(EVENT_PREFIX);
  const isFunction = typeof value === 'function';
  return startsWithOn && isFunction;
}

function toEventName(key) { // onClick 같은 이름을 click으로 바꾼다
  const nameWithoutPrefix = key.slice(EVENT_PREFIX.length);
  const eventName = nameWithoutPrefix.toLowerCase();
  return eventName;
}

function hasEventHandler(domNode, key) { // 이 노드에 기억해 둔 핸들러가 있는지 확인한다
  if (!domNode.handlers) {
    return false;
  }
  return Object.hasOwn(domNode.handlers, key);
}

function addEventHandler(domNode, key, handler) { // 이벤트 리스너를 단다
  // 나중에 떼어 낼 수 있게 DOM에 handlers 객체로 기억한다 (8.4 권장)
  if (!domNode.handlers) {
    domNode.handlers = {};
  }
  domNode.handlers[key] = handler;

  const eventName = toEventName(key);
  domNode.addEventListener(eventName, handler);
}

function removeEventHandler(domNode, key) { // 기억해 둔 핸들러로 리스너를 떼고 기록도 지운다
  const handler = domNode.handlers[key];
  const eventName = toEventName(key);
  domNode.removeEventListener(eventName, handler);

  delete domNode.handlers[key];
}

function updateDomProperty(domNode, key, attributeValue) { // value, checked의 DOM 프로퍼티를 속성에 맞춘다
  if (key === VALUE_KEY) {
    domNode.value = attributeValue;
  } else if (key === CHECKED_KEY) {
    domNode.checked = true;
  }
}

function resetDomProperty(domNode, key) { // value, checked의 DOM 프로퍼티를 빈 상태로 되돌린다
  if (key === VALUE_KEY) {
    domNode.value = '';
  } else if (key === CHECKED_KEY) {
    domNode.checked = false;
  }
}
