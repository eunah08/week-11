/**
 * 변경 목록을 실제 DOM에 적용한다. (Phase 1 문서 8.4)
 * @param {Node} rootNode path 기준이 되는 루트 DOM 노드 (부모가 있어야 한다)
 * @param {object[]} patches 적용할 Patch 배열
 * @returns {Node} 적용 후의 현재 루트 DOM 노드
 * @throws {TypeError} 알 수 없는 patch.type일 때
 * @throws {RangeError} 경로를 찾을 수 없을 때
 */
export function applyPatches(rootNode, patches) {
  // TODO(팀원): 협약대로 구현
  throw new Error('[applyPatches] 아직 구현되지 않았습니다');
}
