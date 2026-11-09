import { NODE_TYPES } from './nodeTypes.js';

// VNode로 만들지 않는 태그 (Phase 1 문서 7.1)
const IGNORED_TAGS = Object.freeze(['script', 'style']);

/**
 * 실제 DOM을 Virtual DOM으로 변환한다. (Phase 1 문서 7.1)
 * @param {Node} domNode 변환할 DOM 노드
 * @returns {object | null} VNode (주석, 공백 텍스트, script, style은 null)
 * @throws {TypeError} domNode가 Node가 아닐 때
 */
export function domToVNode(domNode) { // 실제 DOM 노드를 VNode로 바꾼다
  const isDomNode = domNode instanceof Node;
  if (!isDomNode) {
    throw new TypeError('[domToVNode] Node가 필요합니다');
  }

  if (domNode.nodeType === Node.TEXT_NODE) {
    return textToVNode(domNode);
  } else if (domNode.nodeType === Node.ELEMENT_NODE) {
    return elementToVNode(domNode);
  } else {
    // 주석 등 요소·텍스트가 아닌 노드는 VNode로 만들지 않는다
    return null;
  }
}

function textToVNode(textNode) { // 텍스트 노드를 VNode로 바꾼다 (공백뿐이면 null)
  const text = textNode.nodeValue;

  // 공백만 있는 텍스트는 HTML 들여쓰기·줄바꿈이므로 버린다
  const trimmedText = text.trim();
  if (trimmedText === '') {
    return null;
  }

  // 그 밖의 텍스트는 앞뒤 공백까지 그대로 둔다
  const textVNode = {
    type: NODE_TYPES.TEXT,
    text: text,
  };
  return textVNode;
}

function elementToVNode(element) { // 요소를 VNode로 바꾼다 (script, style이면 null)
  const tag = element.tagName.toLowerCase();

  const isIgnoredTag = IGNORED_TAGS.includes(tag);
  if (isIgnoredTag) {
    return null;
  }

  const props = attributesToProps(element.attributes);
  const children = childNodesToVNodes(element.childNodes);

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
  for (const childNode of childNodes) {
    const childVNode = domToVNode(childNode);

    if (childVNode !== null) {
      children.push(childVNode);
    }
  }
  return children;
}
