// script, style은 VNode로 만들지 않는다 (Phase 1 문서 7.1)
const IGNORED_TAGS = Object.freeze(['script', 'style']);

/**
 * VNode로 만들지 않는 DOM 노드인지 확인한다. (Phase 1 문서 7.1, 8.4)
 * domToVNode와 applyPatches가 같은 기준을 쓰도록 한 곳에 모았다.
 * applyPatches는 path를 셀 때 이 노드들을 건너뛰어야 VNode 인덱스와 맞는다.
 * @param {Node} domNode 확인할 DOM 노드
 * @returns {boolean} 무시하는 노드(주석, 공백뿐인 텍스트, script, style 등)면 true
 */
export function isIgnoredNode(domNode) { // VNode로 만들지 않는(path도 세지 않는) 노드인지 확인한다
  if (domNode.nodeType === Node.TEXT_NODE) {
    // 공백만 있는 텍스트는 HTML 들여쓰기·줄바꿈이므로 무시한다
    const trimmedText = domNode.nodeValue.trim();
    return trimmedText === '';
  } else if (domNode.nodeType === Node.ELEMENT_NODE) {
    const tag = domNode.tagName.toLowerCase();
    return IGNORED_TAGS.includes(tag);
  } else {
    // 주석 등 요소·텍스트가 아닌 노드는 모두 무시한다
    return true;
  }
}
