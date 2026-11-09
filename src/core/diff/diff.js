// vdom 폴더에는 index.js가 없고, core/index.js를 거치면 순환 import가 되므로 파일을 직접 불러온다
import { NODE_TYPES } from '../vdom/nodeTypes.js';
import { PATCH_TYPES } from './patchTypes.js';

/**
 * 두 Virtual DOM을 비교해 변경 목록을 만든다. (Phase 1 문서 8.3)
 * @param {object} previousVNode 이전 VNode
 * @param {object} nextVNode 다음 VNode
 * @returns {object[]} Patch 배열 (바뀐 게 없으면 빈 배열)
 * @throws {TypeError} 둘 중 하나라도 VNode가 아닐 때
 */
export function diff(previousVNode, nextVNode) { // 두 VNode를 비교해 변경 목록(Patch 배열)을 만든다
  const isPreviousVNode = isVNode(previousVNode);
  const isNextVNode = isVNode(nextVNode);
  if (!isPreviousVNode || !isNextVNode) {
    throw new TypeError('[diff] VNode가 필요합니다');
  }

  // 루트 자신의 경로는 빈 배열이다
  const rootPath = [];
  const patches = diffNode(previousVNode, nextVNode, rootPath);
  return patches;
}

function isVNode(value) { // 값이 요소나 텍스트 VNode인지 확인한다
  const isObjectValue = typeof value === 'object' && value !== null;
  if (!isObjectValue) {
    return false;
  }

  const isElement = value.type === NODE_TYPES.ELEMENT;
  const isText = value.type === NODE_TYPES.TEXT;
  return isElement || isText;
}

function diffNode(previousVNode, nextVNode, path) { // 노드 하나와 그 아래를 비교해 Patch 목록을 만든다
  const isTypeChanged = previousVNode.type !== nextVNode.type;
  const isTextNode = nextVNode.type === NODE_TYPES.TEXT;
  const isTagChanged = previousVNode.tag !== nextVNode.tag;

  if (isTypeChanged) {
    // element ↔ text처럼 종류가 다르면 통째로 교체하고 그 아래는 보지 않는다
    return [createReplacePatch(nextVNode, path)];
  } else if (isTextNode) {
    // 텍스트 노드는 tag가 없으므로 태그 비교보다 먼저 처리한다
    return diffText(previousVNode, nextVNode, path);
  } else if (isTagChanged) {
    // div → span처럼 태그가 다르면 통째로 교체한다
    return [createReplacePatch(nextVNode, path)];
  } else {
    return diffElement(previousVNode, nextVNode, path);
  }
}

function createReplacePatch(nextVNode, path) { // REPLACE Patch 하나를 만든다
  const replacePatch = {
    type: PATCH_TYPES.REPLACE,
    path: path,
    node: nextVNode,
  };
  return replacePatch;
}

function diffText(previousVNode, nextVNode, path) { // 텍스트가 다르면 UPDATE_TEXT를 만든다
  if (previousVNode.text === nextVNode.text) {
    return [];
  }

  const textPatch = {
    type: PATCH_TYPES.UPDATE_TEXT,
    path: path,
    text: nextVNode.text,
  };
  return [textPatch];
}

function diffElement(previousVNode, nextVNode, path) { // 같은 태그의 요소를 비교한다 (속성 먼저, 그다음 자식)
  const propPatches = diffProps(previousVNode.props, nextVNode.props, path);
  const childPatches = diffChildren(previousVNode.children, nextVNode.children, path);

  const patches = propPatches.concat(childPatches);
  return patches;
}

function diffProps(previousProps, nextProps, path) { // 속성이 바뀌었으면 UPDATE_PROPS를 만든다
  const set = createSetProps(previousProps, nextProps);
  const remove = createRemoveKeys(previousProps, nextProps);

  const hasSetProps = Object.keys(set).length > 0;
  const hasRemoveKeys = remove.length > 0;
  if (!hasSetProps && !hasRemoveKeys) {
    return [];
  }

  const propsPatch = {
    type: PATCH_TYPES.UPDATE_PROPS,
    path: path,
    set: set,
    remove: remove,
  };
  return [propsPatch];
}

function createSetProps(previousProps, nextProps) { // 새로 생겼거나 값이 바뀐 속성을 모은다
  const set = {};
  for (const key of Object.keys(nextProps)) {
    const isNewKey = !Object.hasOwn(previousProps, key);
    // 이벤트 함수도 참조가 다르면 다른 값으로 본다
    const isChangedValue = previousProps[key] !== nextProps[key];

    if (isNewKey || isChangedValue) {
      set[key] = nextProps[key];
    }
  }
  return set;
}

function createRemoveKeys(previousProps, nextProps) { // 이전에만 있던 속성 이름을 모은다
  const remove = [];
  for (const key of Object.keys(previousProps)) {
    const isRemovedKey = !Object.hasOwn(nextProps, key);

    if (isRemovedKey) {
      remove.push(key);
    }
  }
  return remove;
}

function diffChildren(previousChildren, nextChildren, path) { // 자식들을 같은 인덱스끼리 비교한다
  const commonLength = Math.min(previousChildren.length, nextChildren.length);

  const commonPatches = diffCommonChildren(previousChildren, nextChildren, path);
  const insertPatches = createInsertPatches(nextChildren, commonLength, path);
  const removePatches = createRemovePatches(previousChildren, commonLength, path);

  const patches = commonPatches.concat(insertPatches, removePatches);
  return patches;
}

function diffCommonChildren(previousChildren, nextChildren, path) { // 공통 구간의 자식을 같은 인덱스끼리 비교한다
  const commonLength = Math.min(previousChildren.length, nextChildren.length);

  let patches = [];
  for (let i = 0; i < commonLength; i++) {
    const childPath = path.concat([i]);
    const childPatches = diffNode(previousChildren[i], nextChildren[i], childPath);
    patches = patches.concat(childPatches);
  }
  return patches;
}

function createInsertPatches(nextChildren, commonLength, path) { // 남는 새 자식을 INSERT로 만든다 (오름차순)
  const patches = [];
  for (let i = commonLength; i < nextChildren.length; i++) {
    const insertPatch = {
      type: PATCH_TYPES.INSERT,
      path: path,
      index: i,
      node: nextChildren[i],
    };
    patches.push(insertPatch);
  }
  return patches;
}

function createRemovePatches(previousChildren, commonLength, path) { // 남는 이전 자식을 REMOVE로 만든다 (내림차순)
  const patches = [];
  // 뒤에서부터 지워야 앞쪽 인덱스가 밀리지 않는다 (8.2)
  for (let i = previousChildren.length - 1; i >= commonLength; i--) {
    const removePatch = {
      type: PATCH_TYPES.REMOVE,
      path: path.concat([i]),
    };
    patches.push(removePatch);
  }
  return patches;
}
