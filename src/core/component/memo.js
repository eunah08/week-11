const MEMO_COMPONENTS = new WeakSet();

// Memoization
/**
 * 자식 컴포넌트를 memo 대상으로 표시한다. (차별화 요소)
 * 표시한 컴포넌트는 props가 이전과 같으면(얕은 비교) 다시 실행하지 않고 이전 VNode를 재사용한다.
 * 자식 컴포넌트는 props만 받는 순수 함수이므로 같은 props면 항상 같은 VNode가 나온다.
 * @param {Function} component memo로 표시할 자식 컴포넌트 함수
 * @returns {Function} 같은 컴포넌트 함수 (표시만 하고 새 함수를 만들지 않는다)
 * @throws {TypeError} component가 함수가 아닐 때
 */
export function memo(component) {
  if (typeof component !== 'function') {
    throw new TypeError('[memo] 컴포넌트 함수가 필요합니다');
  }

  MEMO_COMPONENTS.add(component);

  return component;
}

/**
 * memo로 표시한 컴포넌트인지 판단한다.
 * @param {Function} component 확인할 컴포넌트 함수
 * @returns {boolean} memo로 표시했으면 true
 */
export function isMemoComponent(component) {
  return MEMO_COMPONENTS.has(component);
}
