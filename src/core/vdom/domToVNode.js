import { NODE_TYPES } from './nodeTypes.js';
import { isIgnoredNode } from './isIgnoredNode.js';

// 노드 종류별 변환 방법. 새 VNode 종류를 만들려면 이 표에 한 줄을 추가한다.
// isIgnoredNode에서 버리지 않기로 한 노드 종류는 반드시 여기에도 있어야 한다.
const CONVERTER_BY_NODE_TYPE = new Map([
  [Node.TEXT_NODE, toTextVNode],
  [Node.ELEMENT_NODE, toElementVNode],
]);

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

  const convert = CONVERTER_BY_NODE_TYPE.get(domNode.nodeType);

  // 두 표가 어긋났을 때(무시하지 않는데 변환 방법이 없음) 조용히 넘어가지 않고 알린다.
  if (!convert) {
    throw new Error(`[domToVNode] 변환 방법이 없는 노드 종류입니다: ${domNode.nodeType}`);
  }

  return convert(domNode);
}

// 텍스트 앞뒤 공백은 그대로 둔다.
function toTextVNode(textNode) {
  return { type: NODE_TYPES.TEXT, text: textNode.textContent };
}

function toElementVNode(element) {
  return {
    type: NODE_TYPES.ELEMENT,
    tag: element.tagName.toLowerCase(),
    props: toProps(element),
    children: toChildVNodes(element),
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
