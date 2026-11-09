import { NODE_TYPES } from './nodeTypes.js';
import { isIgnoredNode } from './isIgnoredNode.js';

/**
 * 실제 DOM을 Virtual DOM으로 변환한다. (Phase 1 문서 7.1)
 * @param {Node} domNode 변환할 DOM 노드
 * @returns {object | null} VNode (주석, 공백 텍스트, script, style은 null)
 * @throws {TypeError} domNode가 Node가 아닐 때
 */
export function domToVNode(domNode) { // 실제 DOM 노드를 VNode로 바꾼다
  const isDomNode = domNode instanceof Node;
  if (!isDomNode) {
    throw new TypeError('[domToVNode] Node가 필요합니다'); // 경고문
  }

  if (isIgnoredNode(domNode)) {
    return null;
  }

  // 무시하지 않는 노드는 텍스트 아니면 요소다
  if (domNode.nodeType === Node.TEXT_NODE) {
    return textToVNode(domNode);
  } else {
    return elementToVNode(domNode);
  }
}

function textToVNode(domNode) { // 텍스트 노드를 VNode로 바꾼다
  // 텍스트의 앞뒤 공백은 그대로 둔다
  const textVNode = {
    type: NODE_TYPES.TEXT,
    text: domNode.nodeValue,
  };
  return textVNode;
}

function elementToVNode(domNode) { // 요소를 VNode로 바꾼다
  const tag = domNode.tagName.toLowerCase();
  const props = attributesToProps(domNode.attributes);
  const children = childNodesToVNodes(domNode.childNodes);

  const elementVNode = {
    type: NODE_TYPES.ELEMENT,
    tag: tag,
    props: props,
    children: children,
  };
  return elementVNode;
}

function attributesToProps(attributes) { // 속성 목록을 { 이름: 값 } 객체로 바꾼다
  const props = {};
  for (const attribute of attributes) {
    props[attribute.name] = attribute.value;
  }
  return props;
}

function childNodesToVNodes(childNodes) { // 자식 노드들을 VNode 배열로 바꾼다 (null은 뺀다)
  const children = [];
  for (const childDomNode of childNodes) {
    const childVNode = domToVNode(childDomNode);

    if (childVNode !== null) {
      children.push(childVNode);
    }
  }
  return children;
}
