import { NODE_TYPES } from '../vdom/index.js';
import { PATCH_TYPES } from './patchTypes.js';

/**
 * 두 Virtual DOM을 비교해 변경 목록을 만든다. (Phase 1 문서 8.3)
 * 입력 VNode는 수정하지 않는다.
 * @param {object} previousVNode 이전 VNode
 * @param {object} nextVNode 다음 VNode
 * @returns {object[]} Patch 배열 (바뀐 게 없으면 빈 배열)
 * @throws {TypeError} 둘 중 하나라도 VNode가 아닐 때
 */
export function diff(previousVNode, nextVNode) {
  if (!isVNode(previousVNode) || !isVNode(nextVNode)) {
    throw new TypeError('[diff] VNode가 필요합니다');
  }

  const patches = [];

  diffNode({ previousVNode, nextVNode, path: [] }, patches);
  return patches;
}

function isVNode(value) {
  if (value === null || typeof value !== 'object') {
    return false;
  }

  if (value.type === NODE_TYPES.TEXT) {
    return typeof value.text === 'string';
  }

  const hasElementShape = typeof value.tag === 'string'
    && value.props !== null
    && typeof value.props === 'object'
    && Array.isArray(value.children);

  return value.type === NODE_TYPES.ELEMENT && hasElementShape;
}

// patches 배열에 결과를 쌓는다. (diff 안에서 만든 배열이라 바깥 값을 고치지 않는다)
function diffNode({ previousVNode, nextVNode, path }, patches) {
  if (isReplaceNeeded(previousVNode, nextVNode)) {
    patches.push({ type: PATCH_TYPES.REPLACE, path, node: nextVNode });
    return;
  }

  if (nextVNode.type === NODE_TYPES.TEXT) {
    if (previousVNode.text !== nextVNode.text) {
      patches.push({ type: PATCH_TYPES.UPDATE_TEXT, path, text: nextVNode.text });
    }
    return;
  }

  const propChanges = diffProps(previousVNode.props, nextVNode.props);

  if (propChanges.hasChanges) {
    patches.push({
      type: PATCH_TYPES.UPDATE_PROPS,
      path,
      set: propChanges.set,
      remove: propChanges.remove,
    });
  }

  diffChildren({ previousVNode, nextVNode, path }, patches);
}

function isReplaceNeeded(previousVNode, nextVNode) {
  if (previousVNode.type !== nextVNode.type) {
    return true;
  }

  return nextVNode.type === NODE_TYPES.ELEMENT && previousVNode.tag !== nextVNode.tag;
}

// 함수 값도 Object.is로 비교하므로 참조가 다르면 다른 값으로 본다.
function diffProps(previousProps, nextProps) {
  const set = {};
  const remove = [];

  for (const [key, value] of Object.entries(nextProps)) {
    if (!(key in previousProps) || !Object.is(previousProps[key], value)) {
      set[key] = value;
    }
  }

  for (const key of Object.keys(previousProps)) {
    if (!(key in nextProps)) {
      remove.push(key);
    }
  }

  const hasChanges = Object.keys(set).length > 0 || remove.length > 0;

  return { set, remove, hasChanges };
}

// 같은 인덱스끼리 비교한다. INSERT는 인덱스 오름차순, REMOVE는 내림차순으로 만든다. (8.2)
function diffChildren({ previousVNode, nextVNode, path }, patches) {
  const previousChildren = previousVNode.children;
  const nextChildren = nextVNode.children;
  const commonLength = Math.min(previousChildren.length, nextChildren.length);

  for (let i = 0; i < commonLength; i++) {
    const childPath = [...path, i];

    diffNode({
      previousVNode: previousChildren[i],
      nextVNode: nextChildren[i],
      path: childPath,
    }, patches);
  }

  for (let i = commonLength; i < nextChildren.length; i++) {
    patches.push({ type: PATCH_TYPES.INSERT, path, index: i, node: nextChildren[i] });
  }

  for (let i = previousChildren.length - 1; i >= commonLength; i--) {
    patches.push({ type: PATCH_TYPES.REMOVE, path: [...path, i] });
  }
}
