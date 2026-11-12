import { HOOK_KINDS } from '../component/index.js';
import { getHookSlot } from './getHookSlot.js';
import { isDepsChanged } from './isDepsChanged.js';

const EFFECT_NOT_FUNCTION_MESSAGE = '[useEffect] 콜백 함수가 필요합니다';

/**
 * DOM 반영 이후 실행할 작업을 등록한다. (Phase 2 문서 7.2)
 * 렌더링 중에는 pendingEffects에 넣어 두기만 하고, DOM 반영이 끝난 뒤 실행한다.
 * 의존성을 생략하면 매번, 빈 배열이면 최초 한 번, 값이 있으면 Object.is 기준으로 바뀔 때 실행한다.
 * 다시 실행하기 전에 이전 cleanup을 먼저 호출하고, 해제할 때도 남은 cleanup을 호출한다.
 * @param {Function} effect 실행할 작업 (함수를 반환하면 cleanup으로 저장한다)
 * @param {any[]} [dependencies] 의존성 배열
 * @returns {void}
 * @throws {TypeError} effect가 함수가 아닐 때
 * @throws {Error} 루트 컴포넌트 렌더링 밖에서 호출하거나 Hook 순서가 바뀔 때
 */
export function useEffect(effect, dependencies) {
  if (typeof effect !== 'function') {
    throw new TypeError(EFFECT_NOT_FUNCTION_MESSAGE);
  }

  const { component, slot } = getHookSlot('useEffect', HOOK_KINDS.EFFECT, createEffectSlot);

  if (isDepsChanged(slot.dependencies, dependencies)) {
    component.pendingEffects.push(() => runEffect(slot, effect, dependencies));
  }
}

function createEffectSlot() {
  return { kind: HOOK_KINDS.EFFECT, dependencies: undefined, cleanup: undefined };
}

// 이전 cleanup을 먼저 부르고 Effect를 실행한다. 의존성은 실제로 실행한 뒤에 기억한다.
function runEffect(slot, effect, dependencies) {
  if (typeof slot.cleanup === 'function') {
    slot.cleanup();
    slot.cleanup = undefined;
  }

  slot.dependencies = dependencies;

  const cleanup = effect();

  slot.cleanup = typeof cleanup === 'function' ? cleanup : undefined;
}
