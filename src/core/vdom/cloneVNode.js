import { NODE_TYPES } from './nodeTypes.js';

/**
 * VNode를 깊게 복사한다. 원본과 같은 객체를 공유하지 않으므로 복사본을 고쳐도 원본은 그대로다.
 * props 값(문자열·불리언·함수)은 그대로 옮긴다. 검증된 VNode만 받는다.
 * @param {object} vnode 복사할 검증된 VNode
 * @returns {object} 원본과 독립적인 VNode 복사본
 */
export function cloneVNode(vnode) {
  if (vnode.type === NODE_TYPES.TEXT) {
    return { type: NODE_TYPES.TEXT, text: vnode.text };
  }

  return {
    type: NODE_TYPES.ELEMENT,
    tag: vnode.tag,
    props: { ...vnode.props },
    children: vnode.children.map(cloneVNode),
  };
}
