import { NODE_TYPES } from './nodeTypes.js';
import { isIgnoredNode } from './isIgnoredNode.js';

/**
 * 실제 DOM을 Virtual DOM으로 변환한다. (Phase 1 문서 7.1)
 * @param {Node} domNode 변환할 DOM 노드
 * @returns {object | null} VNode (주석, 공백 텍스트, script, style은 null)
 * @throws {TypeError} domNode가 Node가 아닐 때
 */
export function domToVNode(domNode) {
  if (!(domNode instanceof Node)) {
    throw new TypeError('[domToVNode] Node가 필요합니다');
  }

  return toVNode(domNode);
}

function toVNode(domNode) {
  if (isIgnoredNode(domNode)) {
    return null;
  }

  // 텍스트 앞뒤 공백은 그대로 둔다.
  if (domNode.nodeType === Node.TEXT_NODE) {
    return { type: NODE_TYPES.TEXT, text: domNode.textContent };
  }

  return {
    type: NODE_TYPES.ELEMENT,
    tag: domNode.tagName.toLowerCase(),
    props: toProps(domNode),
    children: toChildVNodes(domNode),
  };
}

function toProps(element) {
  const props = {};

  for (const attribute of element.attributes) {
    props[attribute.name] = attribute.value;
  }

  return props;
}

function toChildVNodes(element) {
  const children = [];

  for (const childNode of element.childNodes) {
    const childVNode = toVNode(childNode);

    if (childVNode !== null) {
      children.push(childVNode);
    }
  }

  return children;
}
