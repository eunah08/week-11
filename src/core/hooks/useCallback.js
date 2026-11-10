import { HOOK_KINDS } from '../component/index.js';
import { getHookSlot } from './getHookSlot.js';
import { isDepsChanged } from './isDepsChanged.js';

const CALLBACK_NOT_FUNCTION_MESSAGE = '[useCallback] 콜백 함수가 필요합니다';

// Memoization
/**
 * 의존성이 같으면 이전에 받은 함수를 그대로 돌려준다. (차별화 요소)
 * 렌더링마다 새로 만든 함수가 props로 내려가면 memo 컴포넌트가 매번 "달라졌다"고 보므로,
 * 함수 참조를 고정할 때 쓴다. 함수 안에서 쓰는 값은 모두 의존성에 넣는다.
 * @param {Function} callback 고정할 함수
 * @param {any[]} [dependencies] 의존성 배열 (생략하면 렌더링마다 새 함수를 쓴다)
 * @returns {Function} 의존성이 같은 동안 같은 함수
 * @throws {TypeError} callback이 함수가 아닐 때
 * @throws {Error} 루트 컴포넌트 렌더링 밖에서 호출하거나 Hook 순서가 바뀔 때
 */
export function useCallback(callback, dependencies) {
  if (typeof callback !== 'function') {
    throw new TypeError(CALLBACK_NOT_FUNCTION_MESSAGE);
  }

  const { slot } = getHookSlot('useCallback', HOOK_KINDS.MEMO, createCallbackSlot);

  if (isDepsChanged(slot.dependencies, dependencies)) {
    slot.value = callback;
    slot.dependencies = dependencies;
  }

  return slot.value;
}

// useMemo와 같은 모양의 칸을 쓴다. (value에 함수를 저장한다)
function createCallbackSlot() {
  return { kind: HOOK_KINDS.MEMO, dependencies: undefined, value: undefined };
}
