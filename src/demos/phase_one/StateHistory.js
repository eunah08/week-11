import { isVNode } from '../../core/index.js';

const BACK_STEP = -1;
const FORWARD_STEP = 1;

/**
 * Patch할 때마다 저장하는 VNode 기록. 뒤로가기·앞으로가기에 사용한다. (Phase 1 문서 9.2)
 * DOM 노드는 저장하지 않고, 실제 DOM도 수정하지 않는다.
 * 기록 배열과 현재 위치는 # 비공개 필드라서 바깥에서 직접 바꿀 수 없다. (메서드로만 바뀐다)
 */
export class StateHistory {
  #vnodes;
  #currentIndex;

  /**
   * @param {object} initialVNode 첫 기록
   * @throws {TypeError} VNode가 아닌 값이 올 때
   */
  constructor(initialVNode) {
    assertVNode(initialVNode);
    this.#vnodes = [initialVNode];
    this.#currentIndex = 0;
  }

  /**
   * 현재 위치 뒤의 기록을 버리고 새 기록을 붙인 뒤 그 위치로 옮긴다.
   * @param {object} vnode 새 기록
   * @returns {void}
   * @throws {TypeError} VNode가 아닌 값이 올 때
   */
  push(vnode) {
    assertVNode(vnode);
    this.#vnodes = [...this.#vnodes.slice(0, this.#currentIndex + 1), vnode];
    this.#currentIndex = this.#vnodes.length - 1;
  }

  /** @returns {object | null} 한 칸 뒤의 VNode (더 못 가면 null) */
  back() {
    return this.#move(BACK_STEP);
  }

  /** @returns {object | null} 한 칸 앞의 VNode (더 못 가면 null) */
  forward() {
    return this.#move(FORWARD_STEP);
  }

  /** @returns {object} 현재 VNode */
  getCurrent() {
    return this.#vnodes[this.#currentIndex];
  }

  /** @returns {boolean} 뒤로 갈 수 있는지 */
  canGoBack() {
    return this.#canMove(BACK_STEP);
  }

  /** @returns {boolean} 앞으로 갈 수 있는지 */
  canGoForward() {
    return this.#canMove(FORWARD_STEP);
  }

  /** @returns {number} 현재 위치 (1부터 셈, 화면 표시용) */
  getPosition() {
    return this.#currentIndex + 1;
  }

  /** @returns {number} 전체 기록 개수 (화면 표시용) */
  getCount() {
    return this.#vnodes.length;
  }

  // 이동 규칙을 한 곳에 모은다. 여러 칸 이동이 필요해지면 step만 바꿔 쓰면 된다.
  #move(step) {
    if (!this.#canMove(step)) {
      return null;
    }

    this.#currentIndex += step;
    return this.getCurrent();
  }

  #canMove(step) {
    const targetIndex = this.#currentIndex + step;

    return targetIndex >= 0 && targetIndex < this.#vnodes.length;
  }
}

// VNode 형식 기준은 core의 isVNode 하나를 diff와 함께 쓴다.
function assertVNode(value) {
  if (!isVNode(value)) {
    throw new TypeError('[StateHistory] VNode가 필요합니다');
  }
}
