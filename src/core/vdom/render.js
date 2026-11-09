import { NODE_TYPES } from './nodeTypes.js';
import { setProp } from './domProps.js';

/**
 * Virtual DOM으로 새 DOM 노드를 만든다. (Phase 1 문서 7.2)
 * @param {object} vnode 변환할 VNode
 * @returns {Node} document에 붙이지 않은 새 DOM 노드
 * @throws {TypeError} vnode.type이 알 수 없는 값일 때
 */
export function render(vnode) { // VNode로 새 DOM 노드를 만든다 (document에는 붙이지 않는다)
  const isObjectValue = typeof vnode === 'object' && vnode !== null;

  if (isObjectValue && vnode.type === NODE_TYPES.TEXT) {
    return renderText(vnode);
  } else if (isObjectValue && vnode.type === NODE_TYPES.ELEMENT) {
    return renderElement(vnode);
  } else {
    throw new TypeError('[render] 알 수 없는 VNode 타입입니다');
  }
}

function renderText(vnode) { // 텍스트 VNode로 텍스트 노드를 만든다
  const textNode = document.createTextNode(vnode.text);
  return textNode;
}

function renderElement(vnode) { // 요소를 만들고 속성과 자식을 채운다
  const element = document.createElement(vnode.tag);

  for (const key of Object.keys(vnode.props)) {
    const value = vnode.props[key];
    setProp(element, key, value);
  }

  for (const child of vnode.children) {
    const childNode = render(child);
    element.appendChild(childNode);
  }

  return element;
}
