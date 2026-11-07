import { NODE_TYPES } from './nodeTypes.js';

// 화면 구조가 아니라서 VNode로 만들지 않는 태그 (Phase 1 문서 7.1)
const IGNORED_TAGS = ['script', 'style'];

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
  if (domNode.nodeType === Node.TEXT_NODE) {
    return toTextVNode(domNode);
  }

  if (domNode.nodeType === Node.ELEMENT_NODE) {
    return toElementVNode(domNode);
  }

  // 주석 등 요소·텍스트가 아닌 노드는 화면 구조에 포함하지 않는다.
  return null;
}

function toTextVNode(textNode) {
  const text = textNode.textContent;

  // 빈 문자열과 공백만 있는 텍스트는 무시한다. 내용이 있으면 앞뒤 공백은 그대로 둔다.
  if (text.trim() === '') {
    return null;
  }

  return {
    type: NODE_TYPES.TEXT,
    text,
  };
}

function toElementVNode(element) {
  const tag = element.tagName.toLowerCase();

  if (IGNORED_TAGS.includes(tag)) {
    return null;
  }

  return {
    type: NODE_TYPES.ELEMENT,
    tag,
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
