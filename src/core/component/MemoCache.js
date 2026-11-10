// Memoization
/**
 * memo 컴포넌트의 이전 렌더링 결과를 기억한다. 루트 컴포넌트(FunctionComponent)마다 하나씩 가진다.
 * 같은 컴포넌트를 렌더링 안에서 n번째로 부른 호출은 이전 렌더링의 n번째 호출과 props를 비교한다.
 * 컴포넌트와 props가 같으면 결과가 같으므로, 순번이 어긋나도 틀린 결과가 나오지는 않는다. (덜 재사용할 뿐이다)
 */
export class MemoCache {
  constructor() {
    this.previousEntriesByComponent = new Map();
    this.nextEntriesByComponent = new Map();
  }

  /** 렌더링을 시작한다. 이번 렌더링에서 부른 호출 목록을 새로 모은다. */
  startRender() {
    this.nextEntriesByComponent = new Map();
  }

  /** 렌더링이 성공했다. 이번 호출 목록을 다음 렌더링이 비교할 기준으로 삼는다. */
  finishRender() {
    this.previousEntriesByComponent = this.nextEntriesByComponent;
  }

  /**
   * props가 이전과 같으면 이전 VNode를, 다르면 createVNode로 새로 만든 VNode를 돌려준다.
   * @param {Function} component memo 컴포넌트 함수
   * @param {object} props 이번에 넘길 props
   * @param {Function} createVNode 컴포넌트를 실제로 실행해 VNode를 만드는 함수
   * @returns {object} VNode
   */
  getOrCreateVNode(component, props, createVNode) {
    const nextEntries = findEntries(this.nextEntriesByComponent, component, true);
    const callIndex = nextEntries.length;
    const entry = { props, vnode: null };

    // 컴포넌트를 실행하기 전에 자리부터 잡아, 안쪽에서 같은 컴포넌트를 불러도 순번이 겹치지 않게 한다.
    nextEntries.push(entry);

    const previousEntry = findEntries(this.previousEntriesByComponent, component, false)[callIndex];

    if (previousEntry !== undefined && isPropsEqual(previousEntry.props, props)) {
      entry.vnode = previousEntry.vnode;
      return entry.vnode;
    }

    entry.vnode = createVNode();
    return entry.vnode;
  }
}

function findEntries(entriesByComponent, component, shouldCreate) {
  if (!entriesByComponent.has(component)) {
    if (!shouldCreate) {
      return [];
    }

    entriesByComponent.set(component, []);
  }

  return entriesByComponent.get(component);
}

// props를 얕게 비교한다. 값은 Object.is로, 배열(children 포함)은 항목끼리 Object.is로 비교한다.
function isPropsEqual(previousProps, nextProps) {
  const previousKeys = Object.keys(previousProps);

  if (previousKeys.length !== Object.keys(nextProps).length) {
    return false;
  }

  return previousKeys.every((key) => {
    return Object.hasOwn(nextProps, key) && isValueEqual(previousProps[key], nextProps[key]);
  });
}

function isValueEqual(previousValue, nextValue) {
  if (Object.is(previousValue, nextValue)) {
    return true;
  }

  if (!Array.isArray(previousValue) || !Array.isArray(nextValue)) {
    return false;
  }

  return previousValue.length === nextValue.length
    && previousValue.every((item, index) => Object.is(item, nextValue[index]));
}
