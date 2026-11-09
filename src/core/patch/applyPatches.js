// vdom·diff 폴더에는 index.js가 없고, core/index.js를 거치면 순환 import가 되므로 파일을 직접 불러온다
import { render } from '../vdom/render.js';
import { setProp, removeProp } from '../vdom/domProps.js';
import { PATCH_TYPES } from '../diff/patchTypes.js';

// domToVNode(7.1)가 VNode로 만들지 않는 태그. 경로를 셀 때 같은 기준으로 건너뛴다
const SKIPPED_TAGS = Object.freeze(['script', 'style']);

/**
 * 변경 목록을 실제 DOM에 적용한다. (Phase 1 문서 8.4)
 * @param {Node} rootNode path 기준이 되는 루트 DOM 노드 (부모가 있어야 한다)
 * @param {object[]} patches 적용할 Patch 배열
 * @returns {Node} 적용 후의 현재 루트 DOM 노드
 * @throws {TypeError} rootNode가 Node가 아니거나 patches가 배열이 아니거나 알 수 없는 patch.type일 때
 * @throws {RangeError} 경로를 찾을 수 없을 때
 * @throws {Error} 부모가 없는 루트를 교체하려 할 때
 */
export function applyPatches(rootNode, patches) { // 변경 목록을 실제 DOM에 적용하고 현재 루트를 돌려준다
  const isRootDomNode = rootNode instanceof Node;
  if (!isRootDomNode) {
    throw new TypeError('[applyPatches] Node가 필요합니다');
  }

  const isPatchArray = Array.isArray(patches);
  if (!isPatchArray) {
    throw new TypeError('[applyPatches] Patch 배열이 필요합니다');
  }

  // Patch를 앞에서부터 하나씩 적용한다. 루트가 교체될 수 있어서 현재 루트를 계속 갱신한다
  let currentRootNode = rootNode;
  for (const patch of patches) {
    currentRootNode = applyPatch(currentRootNode, patch);
  }
  return currentRootNode;
}

function applyPatch(rootNode, patch) { // Patch 하나를 종류별로 적용한다
  const targetNode = getNodeByPath(rootNode, patch.path);

  if (patch.type === PATCH_TYPES.INSERT) {
    const newNode = render(patch.node);
    insertChildAt(targetNode, newNode, patch.index);
  } else if (patch.type === PATCH_TYPES.REMOVE) {
    targetNode.remove();
  } else if (patch.type === PATCH_TYPES.REPLACE) {
    // 루트 자신이 교체될 수 있어서 바뀐 루트를 돌려받는다
    return replaceNode(targetNode, patch, rootNode);
  } else if (patch.type === PATCH_TYPES.UPDATE_TEXT) {
    targetNode.textContent = patch.text;
  } else if (patch.type === PATCH_TYPES.UPDATE_PROPS) {
    updateProps(targetNode, patch);
  } else {
    throw new TypeError('[applyPatches] 알 수 없는 Patch 타입입니다');
  }

  return rootNode;
}

function replaceNode(targetNode, patch, rootNode) { // 노드를 새 노드로 교체한다 (루트면 새 루트를 돌려준다)
  const isRootReplaced = patch.path.length === 0;
  const hasParent = targetNode.parentNode !== null;
  if (isRootReplaced && !hasParent) {
    throw new Error('[applyPatches] 부모가 없는 루트는 교체할 수 없습니다');
  }

  const newNode = render(patch.node);
  targetNode.replaceWith(newNode);

  if (isRootReplaced) {
    return newNode;
  } else {
    return rootNode;
  }
}

function updateProps(targetNode, patch) { // 속성을 넣고(set) 지운다(remove)
  for (const key of Object.keys(patch.set)) {
    const value = patch.set[key];
    setProp(targetNode, key, value);
  }

  for (const key of patch.remove) {
    removeProp(targetNode, key);
  }
}

function insertChildAt(parentNode, childNode, index) { // 부모의 index 위치에 자식을 끼워 넣는다
  const children = getCountedChildren(parentNode);

  const isIndexInRange = index >= 0 && index <= children.length;
  if (!isIndexInRange) {
    throw new RangeError('[applyPatches] 경로를 찾을 수 없습니다');
  }

  if (index === children.length) {
    // 맨 끝 자리면 마지막에 붙인다
    parentNode.appendChild(childNode);
  } else {
    const referenceNode = children[index];
    parentNode.insertBefore(childNode, referenceNode);
  }
}

function getNodeByPath(rootNode, path) { // 루트에서 path를 따라가 노드를 찾는다 (없으면 RangeError)
  let currentNode = rootNode;

  for (const index of path) {
    const children = getCountedChildren(currentNode);

    const isIndexInRange = index >= 0 && index < children.length;
    if (!isIndexInRange) {
      throw new RangeError('[applyPatches] 경로를 찾을 수 없습니다');
    }

    currentNode = children[index];
  }

  return currentNode;
}

function getCountedChildren(parentNode) { // VNode 인덱스에 맞게, 무시된 노드를 뺀 자식 목록
  const children = [];
  for (const childNode of parentNode.childNodes) {
    if (!isSkippedNode(childNode)) {
      children.push(childNode);
    }
  }
  return children;
}

function isSkippedNode(domNode) { // 경로를 셀 때 건너뛸 노드인지 확인한다
  if (domNode.nodeType === Node.TEXT_NODE) {
    const trimmedText = domNode.nodeValue.trim();
    return trimmedText === '';
  } else if (domNode.nodeType === Node.ELEMENT_NODE) {
    const tag = domNode.tagName.toLowerCase();
    return SKIPPED_TAGS.includes(tag);
  } else {
    // 주석 등 나머지 노드는 모두 건너뛴다
    return true;
  }
}
