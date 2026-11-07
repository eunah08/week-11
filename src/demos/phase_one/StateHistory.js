/**
 * Patch할 때마다 저장하는 VNode 기록. (Phase 1 문서 9.2)
 */
export class StateHistory {
  /**
   * @param {object} initialVNode 첫 기록
   * @throws {TypeError} VNode가 아닌 값이 올 때
   */
  constructor(initialVNode) {
    // TODO(팀원): 협약대로 구현
    this.initialVNode = initialVNode;
  }

  /** @param {object} vnode 새 기록 */
  push(vnode) {
    throw new Error('[StateHistory] 아직 구현되지 않았습니다');
  }

  /** @returns {object | null} 이전 VNode (없으면 null) */
  back() {
    throw new Error('[StateHistory] 아직 구현되지 않았습니다');
  }

  /** @returns {object | null} 다음 VNode (없으면 null) */
  forward() {
    throw new Error('[StateHistory] 아직 구현되지 않았습니다');
  }

  /** @returns {object} 현재 VNode */
  getCurrent() {
    throw new Error('[StateHistory] 아직 구현되지 않았습니다');
  }

  /** @returns {boolean} 뒤로 갈 수 있는지 */
  canGoBack() {
    throw new Error('[StateHistory] 아직 구현되지 않았습니다');
  }

  /** @returns {boolean} 앞으로 갈 수 있는지 */
  canGoForward() {
    throw new Error('[StateHistory] 아직 구현되지 않았습니다');
  }
}
