import { NODE_TYPES, render, isVNode } from '../vdom/index.js';
import { diff } from '../diff/index.js';
import { applyPatches } from '../patch/index.js';
import { HOOK_KINDS, setCurrentComponent } from './hookContext.js';

const ROOT_NOT_FUNCTION_MESSAGE = '[FunctionComponent] 루트 컴포넌트는 함수여야 합니다';
const CONTAINER_NOT_ELEMENT_MESSAGE = '[FunctionComponent] container는 DOM 요소여야 합니다';
const ALREADY_MOUNTED_MESSAGE = '[FunctionComponent] 이미 mount되었습니다';
const NOT_MOUNTED_MESSAGE = '[FunctionComponent] mount 전에 update할 수 없습니다';
const ROOT_NOT_ELEMENT_MESSAGE = '[FunctionComponent] 루트는 element VNode여야 합니다';

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

    // 이전 mount가 도중에 실패했을 수 있으므로 Hook 값을 비우고 새로 시작한다.
    this.hooks = [];

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

  /**
   * 남은 Effect의 cleanup을 호출하고 관리 대상 DOM을 해제한다. (6.4)
   * 해제 뒤에는 상태 변경 함수가 불려도 갱신하지 않는다. mount되지 않았으면 아무것도 하지 않는다.
   * @returns {void}
   */
  unmount() {
    if (!this.isMounted) {
      return;
    }

    try {
      this.runEffectCleanups();
    } finally {
      // cleanup이 에러를 던져도 DOM 해제와 상태 비우기는 끝까지 한다.
      this.rootNode.remove();
      this.resetAfterUnmount();
    }
  }

  // 루트 컴포넌트를 실행해 새 VNode를 만든다. 실행하는 동안만 이 인스턴스가 현재 컴포넌트다.
  createRootVNode() {
    this.hookIndex = 0;
    this.pendingEffects = [];
    this.isRendering = true;
    this.renderCount += 1;
    setCurrentComponent(this);

    let vnode = null;

    try {
      vnode = this.RootComponent(this.props);
    } finally {
      setCurrentComponent(null);
      this.isRendering = false;
    }

    if (!isVNode(vnode) || vnode.type !== NODE_TYPES.ELEMENT) {
      throw new TypeError(ROOT_NOT_ELEMENT_MESSAGE);
    }

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

  runEffectCleanups() {
    for (const hook of this.hooks) {
      if (hook.kind === HOOK_KINDS.EFFECT && typeof hook.cleanup === 'function') {
        hook.cleanup();
        hook.cleanup = undefined;
      }
    }
  }

  // hooks를 비우면 해제 전에 만든 상태 변경 함수는 더 이상 이 인스턴스를 갱신하지 못한다.
  resetAfterUnmount() {
    this.hooks = [];
    this.pendingEffects = [];
    this.currentVNode = null;
    this.rootNode = null;
    this.isMounted = false;
  }
}
