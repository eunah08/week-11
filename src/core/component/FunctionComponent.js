/**
 * 루트 함수형 컴포넌트의 실행·갱신·해제를 관리한다. (Phase 2 문서 6장)
 */
export class FunctionComponent {
  /**
   * @param {Function} RootComponent 루트 컴포넌트 함수
   * @param {object} props 루트 컴포넌트에 넘길 props
   * @param {Element} container 루트 DOM을 넣을 컨테이너
   * @throws {TypeError} RootComponent가 함수가 아닐 때
   */
  constructor(RootComponent, props, container) {
    // TODO(팀원): 협약 6.1의 구성(hooks, hookIndex, currentVNode, rootNode, pendingEffects)
    this.RootComponent = RootComponent;
    this.props = props;
    this.container = container;
  }

  /** 최초 렌더링. (6.2) */
  mount() {
    throw new Error('[FunctionComponent] 아직 구현되지 않았습니다');
  }

  /** 다시 실행하고 Diff·Patch를 적용한다. (6.3) */
  update() {
    throw new Error('[FunctionComponent] 아직 구현되지 않았습니다');
  }

  /** Effect를 정리하고 관리 대상 DOM을 해제한다. (6.4) */
  unmount() {
    throw new Error('[FunctionComponent] 아직 구현되지 않았습니다');
  }
}
