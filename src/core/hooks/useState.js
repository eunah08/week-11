import { HOOK_KINDS } from '../component/index.js';
import { getHookSlot } from './getHookSlot.js';

const CHANGE_WHILE_RENDERING_MESSAGE = '[useState] 렌더링 중에는 상태를 바꿀 수 없습니다';

/**
 * 상태와 상태 변경 함수를 돌려준다. (Phase 2 문서 7.1)
 * 상태 변경 함수는 값 또는 (이전 값) => 다음 값 함수를 받는다.
 * 값이 Object.is 기준으로 같으면 다시 그리지 않고, 다르면 갱신을 예약한다. (Batching)
 * 같은 틱에서 여러 번 바꿔도 화면 갱신은 microtask에서 한 번만 일어난다.
 * 상태 변경 함수는 렌더링마다 같은 함수이며, 컴포넌트가 해제된 뒤에는 아무 일도 하지 않는다.
 * @param {any} initialValue 초기값 (함수면 최초 한 번만 호출해서 그 결과를 쓴다)
 * @returns {[any, Function]} [현재 값, 상태 변경 함수]
 * @throws {Error} 루트 컴포넌트 렌더링 밖(자식 컴포넌트 안 포함)에서 호출하거나 Hook 순서가 바뀔 때
 */
export function useState(initialValue) {
  const { slot } = getHookSlot('useState', HOOK_KINDS.STATE, (context) => {
    return createStateSlot(context, initialValue);
  });

  return [slot.value, slot.setValue];
}

function createStateSlot({ component, hookIndex }, initialValue) {
  const slot = {
    kind: HOOK_KINDS.STATE,
    value: resolveInitialValue(initialValue),
    setValue: null,
  };

  slot.setValue = (nextValueOrUpdater) => {
    changeState({ component, hookIndex, slot }, nextValueOrUpdater);
  };

  return slot;
}

function resolveInitialValue(initialValue) {
  return typeof initialValue === 'function' ? initialValue() : initialValue;
}

// 상태를 바꾸고 화면을 다시 그린다. 렌더링 중 호출은 무한 반복을 만들 수 있어 막는다. (7.1)
function changeState({ component, hookIndex, slot }, nextValueOrUpdater) {
  if (component.isRendering) {
    throw new Error(CHANGE_WHILE_RENDERING_MESSAGE);
  }

  // 해제(unmount)로 hooks가 비워졌으면 이 칸은 더 이상 쓰이지 않는다.
  if (component.hooks[hookIndex] !== slot) {
    return;
  }

  const nextValue = typeof nextValueOrUpdater === 'function'
    ? nextValueOrUpdater(slot.value)
    : nextValueOrUpdater;

  if (Object.is(nextValue, slot.value)) {
    return;
  }

  // Batching : 값은 바로 바꿔 두고(다음 함수형 갱신이 최신 값을 쓰도록), 화면 갱신만 모아서 한다.
  slot.value = nextValue;
  component.requestUpdate();
}
