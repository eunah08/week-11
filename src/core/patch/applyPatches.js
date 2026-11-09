// vdom·diff 폴더에는 index.js가 없고, core/index.js를 거치면 순환 import가 되므로 파일을 직접 불러온다
import { render } from '../vdom/render.js';
import { isVNode } from '../vdom/isVNode.js';
import { isIgnoredNode } from '../vdom/isIgnoredNode.js';
import { setProp, removeProp } from '../vdom/domProps.js';
import { PATCH_TYPES } from '../diff/patchTypes.js';

// 경로를 찾을 수 없을 때의 에러 메시지 (Phase 1 문서 8.4)
const PATH_NOT_FOUND_MESSAGE = '[applyPatches] 경로를 찾을 수 없습니다';

/**
 * 변경 목록을 실제 DOM에 적용한다. (Phase 1 문서 8.4)
 * @param {Node} rootNode path 기준이 되는 루트 DOM 노드 (부모가 있어야 한다)
 * @param {object[]} patches 적용할 Patch 배열
 * @returns {Node} 적용 후의 현재 루트 DOM 노드
 * @throws {TypeError} rootNode가 Node가 아니거나, patches가 배열이 아니거나,
 *   Patch의 형태가 올바르지 않거나, 알 수 없는 patch.type일 때
 * @throws {RangeError} 경로를 찾을 수 없을 때
 * @throws {Error} 부모가 없는 루트를 교체하려 할 때
 */
export function applyPatches(rootNode, patches) { // 변경 목록을 실제 DOM에 적용하고 현재 루트를 돌려준다
  const isRootDomNode = rootNode instanceof Node;
  if (!isRootDomNode) {
    throw new TypeError('[applyPatches] Node가 필요합니다'); // 경고문
  }

  const isPatchArray = Array.isArray(patches);
  if (!isPatchArray) {
    throw new TypeError('[applyPatches] Patch 배열이 필요합니다'); // 경고문
  }

  // 일부만 적용된 채로 멈추지 않도록, 적용하기 전에 모든 Patch의 형태를 먼저 확인한다
  for (const patch of patches) {
    if (!isPatchShape(patch)) {
      throw new TypeError('[applyPatches] Patch 형태가 올바르지 않습니다'); // 경고문
    }
  }

  // Patch를 앞에서부터 하나씩 적용한다. 루트가 교체될 수 있어서 현재 루트를 계속 갱신한다
  let currentRootNode = rootNode;
  for (const patch of patches) {
    currentRootNode = applyPatch(currentRootNode, patch);
  }
  return currentRootNode;
}

function isPatchShape(patch) { // Patch가 종류별로 필요한 값을 갖췄는지 확인한다
  const isObjectValue = typeof patch === 'object' && patch !== null;
  if (!isObjectValue || !Array.isArray(patch.path)) {
    return false;
  }

  if (patch.type === PATCH_TYPES.INSERT) {
    const hasIndex = Number.isInteger(patch.index);
    return hasIndex && isVNode(patch.node);
  } else if (patch.type === PATCH_TYPES.REPLACE) {
    return isVNode(patch.node);
  } else if (patch.type === PATCH_TYPES.UPDATE_TEXT) {
    return typeof patch.text === 'string';
  } else if (patch.type === PATCH_TYPES.UPDATE_PROPS) {
    const hasSet = typeof patch.set === 'object' && patch.set !== null;
    return hasSet && Array.isArray(patch.remove);
  } else {
    // REMOVE는 path만 있으면 된다. 알 수 없는 종류는 applyPatch가 에러를 던진다
    return true;
  }
}

function applyPatch(rootNode, patch) { // Patch 하나를 종류별로 적용한다
  const targetDomNode = getNodeByPath(rootNode, patch.path);

  if (patch.type === PATCH_TYPES.INSERT) {
    const newDomNode = render(patch.node);
    insertChildAt(targetDomNode, newDomNode, patch.index);
  } else if (patch.type === PATCH_TYPES.REMOVE) {
    targetDomNode.remove();
  } else if (patch.type === PATCH_TYPES.REPLACE) {
    // 루트 자신이 교체될 수 있어서 바뀐 루트를 돌려받는다
    return replaceNode(targetDomNode, patch, rootNode);
  } else if (patch.type === PATCH_TYPES.UPDATE_TEXT) {
    targetDomNode.textContent = patch.text;
  } else if (patch.type === PATCH_TYPES.UPDATE_PROPS) {
    updateProps(targetDomNode, patch);
  } else {
    throw new TypeError('[applyPatches] 알 수 없는 Patch 타입입니다'); // 경고문
  }

  return rootNode;
}

function replaceNode(targetDomNode, patch, rootNode) { // 노드를 새 노드로 교체한다 (루트면 새 루트)
  const isRootReplaced = patch.path.length === 0;
  const hasParent = targetDomNode.parentNode !== null;
  if (isRootReplaced && !hasParent) {
    throw new Error('[applyPatches] 부모가 없는 루트는 교체할 수 없습니다'); // 경고문
  }

  const newDomNode = render(patch.node);
  targetDomNode.replaceWith(newDomNode);

  if (isRootReplaced) {
    return newDomNode;
  } else {
    return rootNode;
  }
}

function updateProps(targetDomNode, patch) { // 속성을 넣고(set) 지운다(remove)
  for (const key of Object.keys(patch.set)) {
    const value = patch.set[key];
    setProp(targetDomNode, key, value);
  }

  for (const key of patch.remove) {
    removeProp(targetDomNode, key);
  }
}

function insertChildAt(parentDomNode, childDomNode, index) { // 부모의 index 위치에 자식을 끼워 넣는다
  const children = getCountedChildren(parentDomNode);

  const isIndexInRange = index >= 0 && index <= children.length;
  if (!isIndexInRange) {
    throw new RangeError(PATH_NOT_FOUND_MESSAGE); // 경고문
  }

  if (index === children.length) {
    // 맨 끝 자리면 마지막에 붙인다
    parentDomNode.appendChild(childDomNode);
  } else {
    const referenceDomNode = children[index];
    parentDomNode.insertBefore(childDomNode, referenceDomNode);
  }
}

function getNodeByPath(rootNode, path) { // 루트에서 path를 따라가 노드를 찾는다 (없으면 RangeError)
  let currentDomNode = rootNode;

  for (const index of path) {
    const children = getCountedChildren(currentDomNode);

    const isIndexInRange = index >= 0 && index < children.length;
    if (!isIndexInRange) {
      throw new RangeError(PATH_NOT_FOUND_MESSAGE); // 경고문
    }

    currentDomNode = children[index];
  }

  return currentDomNode;
}

function getCountedChildren(parentDomNode) { // VNode 인덱스에 맞게, 무시된 노드를 뺀 자식 목록
  const children = [];
  for (const childDomNode of parentDomNode.childNodes) {
    if (!isIgnoredNode(childDomNode)) {
      children.push(childDomNode);
    }
  }
  return children;
}
