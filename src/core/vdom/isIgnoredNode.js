// VNode로 만들지 않는 노드의 기준을 이 파일 한 곳에 모은다.
// domToVNode와 applyPatches가 같은 기준을 써야 path 인덱스가 맞는다. (Phase 1 문서 6.2, 7.1, 8.4)

// 화면 구조가 아니라서 VNode로 만들지 않는 태그
const IGNORED_TAGS = ['script', 'style'];

// "공백만 있는 텍스트"로 볼 글자. HTML 소스의 줄바꿈·들여쓰기에 쓰이는 글자만 공백으로 본다.
// (스페이스, 탭, 줄바꿈, CR, FF) &nbsp;는 사용자가 입력한 띄어쓰기라 내용으로 남긴다.
// 공백의 기준을 바꾸려면 이 정규식만 고친다.
const WHITESPACE_ONLY_PATTERN = /^[ \t\n\r\f]*$/;

// 노드 종류별 무시 규칙. 새 노드 종류를 다루려면 이 표에 한 줄을 추가한다.
const IGNORE_RULE_BY_NODE_TYPE = new Map([
  [Node.TEXT_NODE, isWhitespaceOnlyText],
  [Node.ELEMENT_NODE, isIgnoredTag],
  [Node.COMMENT_NODE, () => true],
]);

/**
 * domToVNode가 VNode로 만들지 않는 DOM 노드인지 판단한다.
 * applyPatches가 path를 따라갈 때도 같은 기준으로 건너뛴다. (Phase 1 문서 8.4)
 * @param {Node} domNode 판단할 DOM 노드
 * @returns {boolean} 주석, 빈 텍스트·공백 텍스트, script, style 등이면 true
 */
export function isIgnoredNode(domNode) {
  const ignoreRule = IGNORE_RULE_BY_NODE_TYPE.get(domNode.nodeType);

  // 표에 없는 노드 종류(처리 명령, 문서 조각 등)는 화면 구조가 아니므로 버린다.
  if (!ignoreRule) {
    return true;
  }

  return ignoreRule(domNode);
}

function isWhitespaceOnlyText(textNode) {
  return WHITESPACE_ONLY_PATTERN.test(textNode.textContent);
}

function isIgnoredTag(element) {
  return IGNORED_TAGS.includes(element.tagName.toLowerCase());
}
