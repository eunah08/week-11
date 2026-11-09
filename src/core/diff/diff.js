import { NODE_TYPES, isVNode } from '../vdom/index.js';
import { PATCH_TYPES } from './patchTypes.js';

const NOT_VNODE_MESSAGE = '[diff] VNode가 필요합니다';

// 종류가 같은 두 VNode를 비교하는 방법. 새 VNode 종류를 비교하려면 이 표에 한 줄을 추가한다.
// 모든 비교 함수는 (이전 VNode, 다음 VNode, path)를 받아 Patch 배열을 돌려준다.
const SAME_TYPE_DIFFER_BY_VNODE_TYPE = new Map([
  [NODE_TYPES.TEXT, diffText],
  [NODE_TYPES.ELEMENT, diffElement],
]);

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
    throw new TypeError(NOT_VNODE_MESSAGE);
  }

  const rootPath = [];

  return diffNode(previousVNode, nextVNode, rootPath);
}

// 입력 검증은 공개 함수 가드에서만 한다. (11장) 자식은 검증된 VNode라고 믿는다.
function diffNode(previousVNode, nextVNode, path) {
  if (isReplaceNeeded(previousVNode, nextVNode)) {
    return [createReplacePatch(nextVNode, path)];
  }

  const diffSameType = SAME_TYPE_DIFFER_BY_VNODE_TYPE.get(nextVNode.type);

  return diffSameType(previousVNode, nextVNode, path);
}

// 종류가 다르거나, 같은 요소라도 태그가 다르면 통째로 교체한다.
function isReplaceNeeded(previousVNode, nextVNode) {
  const isTypeChanged = previousVNode.type !== nextVNode.type;
  const isTagChanged = nextVNode.type === NODE_TYPES.ELEMENT && previousVNode.tag !== nextVNode.tag;

  return isTypeChanged || isTagChanged;
}

function createReplacePatch(nextVNode, path) {
  return { type: PATCH_TYPES.REPLACE, path, node: nextVNode };
}

function diffText(previousVNode, nextVNode, path) {
  if (previousVNode.text === nextVNode.text) {
    return [];
  }

  return [{ type: PATCH_TYPES.UPDATE_TEXT, path, text: nextVNode.text }];
}

// 속성을 먼저 비교하고, 그다음 자식을 비교한다.
function diffElement(previousVNode, nextVNode, path) {
  const propPatches = createPropsPatches(previousVNode.props, nextVNode.props, path);
  const childPatches = diffChildren(previousVNode.children, nextVNode.children, path);

  return propPatches.concat(childPatches);
}

function createPropsPatches(previousProps, nextProps, path) {
  const { set, remove } = diffProps(previousProps, nextProps);
  const hasPropChanges = Object.keys(set).length > 0 || remove.length > 0;

  return hasPropChanges ? [{ type: PATCH_TYPES.UPDATE_PROPS, path, set, remove }] : [];
}

// set : 새로 생겼거나 값이 바뀐 속성, remove : 이전에만 있던 속성 이름
// 함수 값도 Object.is로 비교하므로 참조가 다르면 다른 값으로 본다.
// in 대신 Object.hasOwn을 써서 toString 같은 기본 객체 이름을 props로 착각하지 않는다.
function diffProps(previousProps, nextProps) {
  const set = {};
  const remove = [];

  for (const [key, value] of Object.entries(nextProps)) {
    const isNewKey = !Object.hasOwn(previousProps, key);

    if (isNewKey || !Object.is(previousProps[key], value)) {
      set[key] = value;
    }
  }

  for (const key of Object.keys(previousProps)) {
    if (!Object.hasOwn(nextProps, key)) {
      remove.push(key);
    }
  }

  return { set, remove };
}

// 같은 인덱스끼리 비교한다. 남는 새 자식은 INSERT(오름차순), 남는 이전 자식은 REMOVE(내림차순). (8.2)
function diffChildren(previousChildren, nextChildren, path) {
  const commonLength = Math.min(previousChildren.length, nextChildren.length);
  const commonPatches = diffCommonChildren(previousChildren, nextChildren, path);
  const insertPatches = createInsertPatches(nextChildren, commonLength, path);
  const removePatches = createRemovePatches(previousChildren, commonLength, path);

  return commonPatches.concat(insertPatches, removePatches);
}

// 공통 구간은 push로 한 배열에 모은다. concat을 반복하면 형제가 많을 때 느려진다.
function diffCommonChildren(previousChildren, nextChildren, path) {
  const commonLength = Math.min(previousChildren.length, nextChildren.length);
  const patches = [];

  for (let i = 0; i < commonLength; i++) {
    for (const patch of diffNode(previousChildren[i], nextChildren[i], [...path, i])) {
      patches.push(patch);
    }
  }

  return patches;
}

function createInsertPatches(nextChildren, commonLength, path) {
  const patches = [];

  for (let i = commonLength; i < nextChildren.length; i++) {
    patches.push({ type: PATCH_TYPES.INSERT, path, index: i, node: nextChildren[i] });
  }

  return patches;
}

function createRemovePatches(previousChildren, commonLength, path) {
  const patches = [];

  // 뒤에서부터 지워야 앞쪽 인덱스가 밀리지 않는다.
  for (let i = previousChildren.length - 1; i >= commonLength; i--) {
    patches.push({ type: PATCH_TYPES.REMOVE, path: [...path, i] });
  }

  return patches;
}
