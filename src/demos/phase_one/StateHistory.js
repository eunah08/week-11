import { NODE_TYPES } from '../../core/index.js';

/**
 * Patch할 때마다 저장하는 VNode 기록. 뒤로가기·앞으로가기에 사용한다. (Phase 1 문서 9.2)
 * DOM 노드는 저장하지 않고, 실제 DOM도 수정하지 않는다.
 */
export class StateHistory {
  /**
   * @param {object} initialVNode 첫 기록
   * @throws {TypeError} VNode가 아닌 값이 올 때
   */
  constructor(initialVNode) {
    assertVNode(initialVNode);
    this.vnodes = [initialVNode];
    this.currentIndex = 0;
  }

  /**
   * 현재 위치 뒤의 기록을 버리고 새 기록을 붙인 뒤 그 위치로 옮긴다.
   * @param {object} vnode 새 기록
   * @returns {void}
   * @throws {TypeError} VNode가 아닌 값이 올 때
   */
  push(vnode) {
    assertVNode(vnode);
    this.vnodes = [...this.vnodes.slice(0, this.currentIndex + 1), vnode];
    this.currentIndex = this.vnodes.length - 1;
  }

  /** @returns {object | null} 한 칸 뒤의 VNode (더 못 가면 null) */
  back() {
    if (!this.canGoBack()) {
      return null;
    }

    this.currentIndex -= 1;
    return this.getCurrent();
  }

  /** @returns {object | null} 한 칸 앞의 VNode (더 못 가면 null) */
  forward() {
    if (!this.canGoForward()) {
      return null;
    }

    this.currentIndex += 1;
    return this.getCurrent();
  }

  /** @returns {object} 현재 VNode */
  getCurrent() {
    return this.vnodes[this.currentIndex];
  }

  /** @returns {boolean} 뒤로 갈 수 있는지 */
  canGoBack() {
    return this.currentIndex > 0;
  }

  /** @returns {boolean} 앞으로 갈 수 있는지 */
  canGoForward() {
    return this.currentIndex < this.vnodes.length - 1;
  }

  /** @returns {number} 현재 위치 (1부터 셈, 화면 표시용) */
  getPosition() {
    return this.currentIndex + 1;
  }

  /** @returns {number} 전체 기록 개수 (화면 표시용) */
  getCount() {
    return this.vnodes.length;
  }
}

function assertVNode(value) {
  const isObject = value !== null && typeof value === 'object';
  const hasKnownType = isObject
    && (value.type === NODE_TYPES.ELEMENT || value.type === NODE_TYPES.TEXT);

  if (!hasKnownType) {
    throw new TypeError('[StateHistory] VNode가 필요합니다');
  }
}
