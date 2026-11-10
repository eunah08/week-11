import { getCurrentComponent } from '../component/index.js';

/**
 * 지금 렌더링 중인 컴포넌트의 hooks 배열에서 이번 Hook이 쓸 칸을 꺼낸다. (Phase 2 문서 7장)
 * 칸이 없으면(최초 렌더링) createSlot으로 만들어 넣는다. 호출할 때마다 hookIndex를 하나 늘린다.
 * @param {string} hookName 에러 메시지에 쓸 Hook 이름 (예: 'useState')
 * @param {string} kind 이 Hook의 칸 종류 (HOOK_KINDS 값)
 * @param {Function} createSlot ({ component, hookIndex })를 받아 새 칸을 만드는 함수
 * @returns {{ component: object, slot: object }} 현재 컴포넌트와 이번 Hook의 칸
 * @throws {Error} 루트 컴포넌트 렌더링 중이 아닐 때, 같은 칸의 Hook 종류가 바뀌었을 때
 */
export function getHookSlot(hookName, kind, createSlot) {
  const component = getCurrentComponent();

  if (component === null) {
    throw new Error(`[${hookName}] 루트 컴포넌트 렌더링 중에만 호출할 수 있습니다`);
  }

  const hookIndex = component.hookIndex;

  component.hookIndex += 1;

  if (component.hooks[hookIndex] === undefined) {
    component.hooks[hookIndex] = createSlot({ component, hookIndex });
  }

  const slot = component.hooks[hookIndex];

  if (slot.kind !== kind) {
    throw new Error(`[${hookName}] Hook 호출 순서가 바뀌었습니다`);
  }

  return { component, slot };
}
