import { NODE_TYPES } from '../../core/index.js';

/**
 * Patch할 때마다 저장하는 VNode 기록. (Phase 1 문서 9.2)
 * 기록 개수에는 제한이 없고, DOM 노드가 아닌 VNode만 저장한다.
 */
export class StateHistory {
  /**
   * @param {object} initialVNode 첫 기록
   * @throws {TypeError} VNode가 아닌 값이 올 때
   */
  constructor(initialVNode) { // 첫 기록을 저장한다
    if (!isVNode(initialVNode)) {
      throw new TypeError('[StateHistory] VNode가 필요합니다');
    }

    // 저장한 VNode 목록과, 지금 보고 있는 기록의 위치
    this.vnodes = [initialVNode];
    this.currentIndex = 0;
  }

  /**
   * 현재 위치 뒤의 기록을 버리고 새 기록을 붙인 뒤 그 위치로 옮긴다.
   * @param {object} vnode 새 기록
   * @returns {void}
   * @throws {TypeError} VNode가 아닌 값이 올 때
   */
  push(vnode) { // 새 기록을 붙인다 (현재 위치 뒤의 기록은 버린다)
    if (!isVNode(vnode)) {
      throw new TypeError('[StateHistory] VNode가 필요합니다');
    }

    // 뒤로가기한 상태에서 새 기록을 넣으면, 현재 위치 뒤의 기록(앞으로가기 기록)은 버린다
    const keepCount = this.currentIndex + 1;
    this.vnodes = this.vnodes.slice(0, keepCount);

    this.vnodes.push(vnode);
    this.currentIndex = this.vnodes.length - 1;
  }

  /** @returns {object | null} 한 칸 뒤의 VNode (더 못 가면 null) */
  back() { // 한 칸 뒤로 가서 VNode를 돌려준다 (못 가면 null)
    if (!this.canGoBack()) {
      return null;
    }

    this.currentIndex = this.currentIndex - 1;
    return this.getCurrent();
  }

  /** @returns {object | null} 한 칸 앞의 VNode (더 못 가면 null) */
  forward() { // 한 칸 앞으로 가서 VNode를 돌려준다 (못 가면 null)
    if (!this.canGoForward()) {
      return null;
    }

    this.currentIndex = this.currentIndex + 1;
    return this.getCurrent();
  }

  /** @returns {object} 현재 VNode */
  getCurrent() { // 현재 VNode를 돌려준다
    return this.vnodes[this.currentIndex];
  }

  /** @returns {boolean} 뒤로 갈 수 있는지 */
  canGoBack() { // 뒤로 갈 수 있는지 알려준다
    const isFirstRecord = this.currentIndex === 0;
    return !isFirstRecord;
  }

  /** @returns {boolean} 앞으로 갈 수 있는지 */
  canGoForward() { // 앞으로 갈 수 있는지 알려준다
    const lastIndex = this.vnodes.length - 1;
    const isLastRecord = this.currentIndex === lastIndex;
    return !isLastRecord;
  }

  /**
   * 화면의 이력 위치 표시용 정보. (협약에 없는 추가 메서드)
   * @returns {{ index: number, count: number }} 현재 위치(0부터)와 전체 기록 개수
   */
  getPosition() { // 현재 위치와 전체 기록 개수를 돌려준다
    const position = {
      index: this.currentIndex,
      count: this.vnodes.length,
    };
    return position;
  }
}

function isVNode(value) { // 값이 VNode인지 확인한다
  const isObjectValue = typeof value === 'object' && value !== null;
  if (!isObjectValue) {
    return false;
  }

  const isElement = value.type === NODE_TYPES.ELEMENT;
  const isText = value.type === NODE_TYPES.TEXT;
  return isElement || isText;
}
