// 화면 구조가 아니라서 VNode로 만들지 않는 태그 (Phase 1 문서 7.1)
const IGNORED_TAGS = ['script', 'style'];

/**
 * domToVNode가 VNode로 만들지 않는 DOM 노드인지 판단한다.
 * applyPatches가 path를 따라갈 때도 같은 기준으로 건너뛴다. (Phase 1 문서 8.4)
 * @param {Node} domNode 판단할 DOM 노드
 * @returns {boolean} 주석, 빈 텍스트·공백 텍스트, script, style 등이면 true
 */
export function isIgnoredNode(domNode) {
  if (domNode.nodeType === Node.TEXT_NODE) {
    return domNode.textContent.trim() === '';
  }

  if (domNode.nodeType === Node.ELEMENT_NODE) {
    return IGNORED_TAGS.includes(domNode.tagName.toLowerCase());
  }

  return true;
}
