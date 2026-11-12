import { NODE_TYPES, render, isVNode } from '../vdom/index.js';
import { diff } from '../diff/index.js';
import { applyPatches } from '../patch/index.js';
import { HOOK_KINDS, setCurrentComponent, setRenderOwner } from './hookContext.js';
import { MemoCache } from './MemoCache.js';

const ROOT_NOT_FUNCTION_MESSAGE = '[FunctionComponent] 루트 컴포넌트는 함수여야 합니다';
const CONTAINER_NOT_ELEMENT_MESSAGE = '[FunctionComponent] container는 DOM 요소여야 합니다';
const ALREADY_MOUNTED_MESSAGE = '[FunctionComponent] 이미 mount되었습니다';
const NOT_MOUNTED_MESSAGE = '[FunctionComponent] mount 전에 update할 수 없습니다';
const ROOT_NOT_ELEMENT_MESSAGE = '[FunctionComponent] 루트는 element VNode여야 합니다';
const HOOK_ORDER_CHANGED_MESSAGE = '[FunctionComponent] Hook 호출 순서가 바뀌었습니다';

/**
 * 루트 함수형 컴포넌트의 실행·갱신·해제를 관리한다. (Phase 2 문서 6장)
 * Hook 값은 hooks 배열에 두고, 화면 반영은 Phase 1의 render, diff, applyPatches만 쓴다.
 */
export class FunctionComponent {
  /**
   * @param {Function} RootComponent 루트 컴포넌트 함수
   * @param {object} [props] 루트 컴포넌트에 넘길 props (없으면 빈 객체)
   * @param {Element} container 루트 DOM을 넣을 컨테이너
   * @throws {TypeError} RootComponent가 함수가 아니거나 container가 DOM 요소가 아닐 때
   */
  constructor(RootComponent, props, container) {
    if (typeof RootComponent !== 'function') {
      throw new TypeError(ROOT_NOT_FUNCTION_MESSAGE);
    }

    if (!(container instanceof Element)) {
      throw new TypeError(CONTAINER_NOT_ELEMENT_MESSAGE);
    }

    this.RootComponent = RootComponent;
    this.props = props ?? {};
    this.container = container;
    this.hooks = [];
    this.hookIndex = 0;
    this.currentVNode = null;
    this.rootNode = null;
    this.pendingEffects = [];
    this.isMounted = false;
    this.isRendering = false;
    this.renderCount = 0;
    this.isUpdateScheduled = false;
    // Memoization : memo 자식 컴포넌트의 이전 렌더링 결과
    this.memoCache = new MemoCache();
    // 예약된 갱신(Batching)에서 난 에러를 받을 함수. 없으면 에러를 그대로 던진다. (Phase 2 문서에 없는 추가 필드)
    this.onError = null;
  }

  /**
   * 최초 렌더링. 루트를 실행해 container에 넣고 Effect를 실행한다. (6.2)
   * @returns {void}
   * @throws {Error} 이미 mount되었을 때
   * @throws {TypeError} 루트 컴포넌트가 element VNode를 반환하지 않았을 때
   */
  mount() {
    if (this.isMounted) {
      throw new Error(ALREADY_MOUNTED_MESSAGE);
    }

    // 이전 mount가 도중에 실패했을 수 있으므로 Hook 값과 memo 결과를 비우고 새로 시작한다.
    this.hooks = [];
    this.memoCache = new MemoCache();

    const vnode = this.createRootVNode();
    const rootNode = render(vnode);

    this.container.replaceChildren(rootNode);
    this.currentVNode = vnode;
    this.rootNode = rootNode;
    this.isMounted = true;
    this.runPendingEffects();
  }

  /**
   * 루트를 다시 실행하고 바뀐 부분만 DOM에 반영한 뒤 Effect를 실행한다. (6.3)
   * @returns {void}
   * @throws {Error} mount되지 않았을 때
   * @throws {TypeError} 루트 컴포넌트가 element VNode를 반환하지 않았을 때
   */
  update() {
    if (!this.isMounted) {
      throw new Error(NOT_MOUNTED_MESSAGE);
    }

    const nextVNode = this.createRootVNode();
    const patches = diff(this.currentVNode, nextVNode);

    // 루트가 교체될 수 있으므로 applyPatches가 돌려준 루트를 쓴다.
    this.rootNode = applyPatches(this.rootNode, patches);
    this.currentVNode = nextVNode;
    this.runPendingEffects();
  }

  // Batching
  /**
   * 갱신을 예약한다. 같은 틱에서 여러 번 불려도 microtask 한 번으로 모아 update()를 한 번만 실행한다.
   * 상태 변경 함수가 호출될 때마다 즉시 update()하지 않고 이 함수를 쓴다. (Phase 2 문서 7.1 Batching)
   * @returns {void}
   */
  requestUpdate() {
    if (!this.isMounted || this.isUpdateScheduled) {
      return;
    }

    this.isUpdateScheduled = true;
    queueMicrotask(() => this.flushUpdate());
  }

  // Batching
  /**
   * 예약된 갱신이 있으면 기다리지 않고 지금 실행한다. 테스트에서 microtask를 기다리지 않으려고 쓴다.
   * microtask에서 불린 경우 에러는 호출한 쪽이 없으므로 onError로 넘기고, 없으면 그대로 던진다.
   * @returns {void}
   */
  flushUpdate() {
    if (!this.isUpdateScheduled) {
      return;
    }

    this.isUpdateScheduled = false;

    if (!this.isMounted) {
      return;
    }

    try {
      this.update();
    } catch (error) {
      if (typeof this.onError !== 'function') {
        throw error;
      }

      this.onError(error);
    }
  }

  /**
   * 남은 Effect의 cleanup을 호출하고 관리 대상 DOM을 해제한다. (6.4)
   * 해제 뒤에는 상태 변경 함수가 불려도 갱신하지 않는다. mount되지 않았으면 아무것도 하지 않는다.
   * cleanup이 에러를 던져도 나머지 cleanup과 DOM 해제를 마친 뒤, 처음 난 에러를 던진다.
   * @returns {void}
   * @throws {Error} cleanup이 에러를 던졌을 때 (처음 난 에러)
   */
  unmount() {
    if (!this.isMounted) {
      return;
    }

    const firstCleanupError = this.runEffectCleanups();

    this.rootNode.remove();
    this.resetAfterUnmount();

    if (firstCleanupError !== null) {
      throw firstCleanupError;
    }
  }

  // 루트 컴포넌트를 실행해 새 VNode를 만든다. 실행하는 동안만 이 인스턴스가 현재 컴포넌트다.
  createRootVNode() {
    this.hookIndex = 0;
    this.pendingEffects = [];
    this.isRendering = true;
    this.renderCount += 1;
    this.memoCache.startRender();
    setCurrentComponent(this);
    setRenderOwner(this);

    let vnode = null;

    try {
      vnode = this.RootComponent(this.props);
    } finally {
      setCurrentComponent(null);
      setRenderOwner(null);
      this.isRendering = false;
    }

    this.assertHookCountUnchanged();

    if (!isVNode(vnode) || vnode.type !== NODE_TYPES.ELEMENT) {
      throw new TypeError(ROOT_NOT_ELEMENT_MESSAGE);
    }

    // 렌더링이 성공했을 때만 이번 결과를 다음 렌더링의 비교 기준으로 삼는다.
    this.memoCache.finishRender();

    return vnode;
  }

  // DOM 반영이 끝난 뒤 렌더링 중에 모아 둔 Effect를 등록된 순서대로 실행한다.
  runPendingEffects() {
    const effects = this.pendingEffects;
    const renderCountAtStart = this.renderCount;

    // Effect 안에서 상태가 바뀌어 update()가 다시 불려도 같은 Effect를 두 번 실행하지 않게 비운다.
    this.pendingEffects = [];

    for (const runEffect of effects) {
      // 실행 도중 다시 렌더링되었으면 남은 Effect는 낡은 렌더링의 것이다.
      // 새 렌더링이 의존성을 다시 비교해 필요한 Effect를 이미 따로 등록했으므로 건너뛴다.
      if (this.renderCount !== renderCountAtStart) {
        return;
      }

      runEffect();
    }
  }

  // 이전 렌더링보다 Hook을 적게 부르면(조건문 안의 Hook) 남은 칸이 엉뚱한 Hook과 짝지어진다.
  // 더 많이 부른 경우는 getHookSlot이 칸 종류를 비교할 때 잡는다.
  // 최초 렌더링(mount 전)에는 칸을 새로 만드는 중이라 검사하지 않는다.
  assertHookCountUnchanged() {
    if (this.isMounted && this.hookIndex !== this.hooks.length) {
      throw new Error(HOOK_ORDER_CHANGED_MESSAGE);
    }
  }

  // 모든 Effect의 cleanup을 Hook 순서대로 부르고, 처음 난 에러를 돌려준다. (없으면 null)
  // 하나가 에러를 던져도 나머지 cleanup은 끝까지 부른다.
  runEffectCleanups() {
    let firstCleanupError = null;

    for (const hook of this.hooks) {
      if (hook.kind !== HOOK_KINDS.EFFECT || typeof hook.cleanup !== 'function') {
        continue;
      }

      // cleanup은 한 번만 부르도록 부르기 전에 비운다.
      const cleanup = hook.cleanup;

      hook.cleanup = undefined;

      try {
        cleanup();
      } catch (error) {
        if (firstCleanupError === null) {
          firstCleanupError = error;
        }
      }
    }

    return firstCleanupError;
  }

  // hooks를 비우면 해제 전에 만든 상태 변경 함수는 더 이상 이 인스턴스를 갱신하지 못한다.
  resetAfterUnmount() {
    this.hooks = [];
    this.memoCache = new MemoCache();
    this.pendingEffects = [];
    this.currentVNode = null;
    this.rootNode = null;
    this.isMounted = false;
    this.isUpdateScheduled = false;
  }
}
