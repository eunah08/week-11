import { HOOK_KINDS } from '../component/index.js';
import { getHookSlot } from './getHookSlot.js';
import { isDepsChanged } from './isDepsChanged.js';

const FACTORY_NOT_FUNCTION_MESSAGE = '[useMemo] 계산 함수가 필요합니다';

/**
 * 의존성이 같으면 이전 계산 결과를 재사용한다. (Phase 2 문서 7.3)
 * 최초 렌더링과 의존성이 바뀔 때, 의존성을 생략했을 때는 렌더링마다 계산한다.
 * @param {Function} factory 계산 함수 (DOM 변경, 상태 변경, 외부 요청을 하지 않는다)
 * @param {any[]} [dependencies] 의존성 배열
 * @returns {any} 계산 결과
 * @throws {TypeError} factory가 함수가 아닐 때
 * @throws {Error} 루트 컴포넌트 렌더링 밖에서 호출하거나 Hook 순서가 바뀔 때
 */
export function useMemo(factory, dependencies) {
  if (typeof factory !== 'function') {
    throw new TypeError(FACTORY_NOT_FUNCTION_MESSAGE);
  }

  const { slot } = getHookSlot('useMemo', HOOK_KINDS.MEMO, createMemoSlot);

  if (isDepsChanged(slot.dependencies, dependencies)) {
    // 계산이 성공한 뒤에 의존성을 기억해야, 계산이 실패해도 다음 렌더링에서 다시 계산한다.
    slot.value = factory();
    slot.dependencies = dependencies;
  }

  return slot.value;
}

function createMemoSlot() {
  return { kind: HOOK_KINDS.MEMO, dependencies: undefined, value: undefined };
}
