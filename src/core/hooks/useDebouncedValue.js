import { useState } from './useState.js';
import { useEffect } from './useEffect.js';

const INVALID_DELAY_MESSAGE = '[useDebouncedValue] 지연 시간은 0 이상의 숫자여야 합니다';

// Debouncing
/**
 * 값이 바뀐 뒤 delayMs 동안 더 바뀌지 않을 때만 따라 바뀌는 값을 돌려준다. (차별화 요소)
 * 값이 계속 바뀌는 동안에는 이전 타이머를 cleanup으로 지우고 새로 걸어, 마지막 값만 반영한다.
 * 입력창 자체의 값처럼 즉시 보여야 하는 값에는 쓰지 않고, 검색처럼 늦게 따라와도 되는 값에 쓴다.
 * @param {any} value 지켜볼 값
 * @param {number} delayMs 값이 멈춘 뒤 기다릴 시간 (밀리초, 0 이상)
 * @returns {any} 처음에는 value, 이후에는 delayMs 동안 변화가 없었던 마지막 value
 * @throws {TypeError} delayMs가 0 이상의 숫자가 아닐 때
 * @throws {Error} 루트 컴포넌트 렌더링 밖에서 호출하거나 Hook 순서가 바뀔 때
 */
export function useDebouncedValue(value, delayMs) {
  if (typeof delayMs !== 'number' || !(delayMs >= 0)) {
    throw new TypeError(INVALID_DELAY_MESSAGE);
  }

  const [debouncedValue, setDebouncedValue] = useState(value);

  useEffect(() => {
    const timerId = setTimeout(() => setDebouncedValue(value), delayMs);

    // 값이 다시 바뀌거나 컴포넌트가 해제되면 아직 안 끝난 타이머를 지운다.
    return () => clearTimeout(timerId);
  }, [value, delayMs]);

  return debouncedValue;
}
