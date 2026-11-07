import { NODE_TYPES } from './nodeTypes.js';
import { setProp } from './props.js';

/**
 * Virtual DOM으로 새 DOM 노드를 만든다. (Phase 1 문서 7.2)
 * @param {object} vnode 변환할 VNode
 * @returns {Node} document에 붙이지 않은 새 DOM 노드
 * @throws {TypeError} vnode.type이 알 수 없는 값일 때
 */
export function render(vnode) {
  if (vnode === null || typeof vnode !== 'object') {
    throw new TypeError('[render] 알 수 없는 VNode 타입입니다');
  }

  if (vnode.type === NODE_TYPES.TEXT) {
    return document.createTextNode(vnode.text);
  }

  if (vnode.type === NODE_TYPES.ELEMENT) {
    return createElementNode(vnode);
  }

  throw new TypeError('[render] 알 수 없는 VNode 타입입니다');
}

function createElementNode(vnode) {
  const domNode = document.createElement(vnode.tag);

  for (const [key, value] of Object.entries(vnode.props)) {
    setProp(domNode, key, value);
  }

  for (const childVNode of vnode.children) {
    domNode.appendChild(render(childVNode));
  }

  return domNode;
}
