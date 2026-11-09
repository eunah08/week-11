import { NODE_TYPES } from './nodeTypes.js';

/**
 * 값이 VNode 형태인지 확인한다. 자식 VNode도 끝까지 확인한다. (Phase 1 문서 6장)
 * - 텍스트 노드 : type이 'text'이고 text가 문자열
 * - 요소 노드 : type이 'element'이고 tag가 문자열, props가 객체, children이 VNode 배열
 * @param {*} value 확인할 값
 * @returns {boolean} VNode 형태면 true
 */
export function isVNode(value) { // 값이 VNode 형태인지 확인한다 (자식까지)
  const isObjectValue = typeof value === 'object' && value !== null;
  if (!isObjectValue) {
    return false;
  }

  if (value.type === NODE_TYPES.TEXT) {
    return isTextVNode(value);
  } else if (value.type === NODE_TYPES.ELEMENT) {
    return isElementVNode(value);
  } else {
    return false;
  }
}

function isTextVNode(value) { // 텍스트 VNode에 필요한 값이 있는지 확인한다
  const hasText = typeof value.text === 'string';
  return hasText;
}

function isElementVNode(value) { // 요소 VNode에 필요한 값이 있는지 확인한다
  const hasTag = typeof value.tag === 'string';
  const hasProps = typeof value.props === 'object' && value.props !== null;
  const hasChildren = Array.isArray(value.children);
  if (!hasTag || !hasProps || !hasChildren) {
    return false;
  }

  // 자식 중 하나라도 VNode가 아니면 전체가 VNode가 아니다
  for (const child of value.children) {
    if (!isVNode(child)) {
      return false;
    }
  }
  return true;
}
