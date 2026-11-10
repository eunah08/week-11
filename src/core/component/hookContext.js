// 지금 렌더링 중인 컴포넌트를 모듈 안 변수 하나에 저장하고 꺼낸다. (Phase 2 문서 8장)
// Hook들과 createElement는 이것으로 자기가 속한 컴포넌트를 찾는다.

/**
 * hooks 배열 칸 하나가 어떤 Hook의 것인지 구분하는 값. (Phase 2 문서 7장)
 */
export const HOOK_KINDS = Object.freeze({
  STATE: 'state',
  EFFECT: 'effect',
  MEMO: 'memo',
});

// 루트 컴포넌트를 렌더링하는 동안에만 값이 있고, 그 밖에는 null이다.
let currentComponent = null;

// Memoization
// 지금 렌더링 중인 루트 컴포넌트. currentComponent와 달리 자식 컴포넌트를 부르는 동안에도 유지된다.
// (자식 안에서 Hook을 막으려면 currentComponent가 null이어야 하지만, memo 캐시는 루트에 있어서 필요하다)
let renderOwner = null;

/**
 * 지금 렌더링 중인 컴포넌트를 정한다. 렌더링이 끝나거나 자식 컴포넌트를 부를 때는 null을 넣는다.
 * @param {object | null} component 렌더링 중인 FunctionComponent (없으면 null)
 * @returns {void}
 */
export function setCurrentComponent(component) {
  currentComponent = component;
}

/**
 * 지금 렌더링 중인 루트 컴포넌트를 정한다. 자식 컴포넌트를 부르는 동안에도 바뀌지 않는다.
 * @param {object | null} component 렌더링 중인 FunctionComponent (없으면 null)
 * @returns {void}
 */
export function setRenderOwner(component) {
  renderOwner = component;
}

/**
 * 지금 렌더링 중인 루트 컴포넌트를 돌려준다. memo 캐시를 찾을 때 쓴다.
 * @returns {object | null} 렌더링 중인 FunctionComponent (렌더링 중이 아니면 null)
 */
export function getRenderOwner() {
  return renderOwner;
}

/**
 * 지금 렌더링 중인 컴포넌트를 돌려준다.
 * @returns {object | null} 렌더링 중인 FunctionComponent (렌더링 중이 아니면 null)
 */
export function getCurrentComponent() {
  return currentComponent;
}
