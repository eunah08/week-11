import { render, setProp, removeProp, isIgnoredNode } from '../vdom/index.js';
import { PATCH_TYPES } from '../diff/index.js';

/**
 * 변경 목록을 실제 DOM에 적용한다. (Phase 1 문서 8.4)
 * patches를 앞에서부터 순서대로 하나씩 적용하고, 바뀌지 않은 노드는 건드리지 않는다.
 * @param {Node} rootNode path 기준이 되는 루트 DOM 노드 (부모가 있어야 한다)
 * @param {object[]} patches 적용할 Patch 배열
 * @returns {Node} 적용 후의 현재 루트 DOM 노드
 * @throws {TypeError} rootNode·patches 형식이 틀리거나 알 수 없는 patch.type일 때
 * @throws {RangeError} 경로를 찾을 수 없을 때
 */
export function applyPatches(rootNode, patches) {
  if (!(rootNode instanceof Node) || rootNode.parentNode === null) {
    throw new TypeError('[applyPatches] 부모가 있는 루트 DOM 노드가 필요합니다');
  }

  if (!Array.isArray(patches)) {
    throw new TypeError('[applyPatches] Patch 배열이 필요합니다');
  }

  let currentRoot = rootNode;

  for (const patch of patches) {
    currentRoot = applyPatch(currentRoot, patch);
  }

  return currentRoot;
}

// 패치 하나를 적용하고 현재 루트를 돌려준다. (루트가 교체되면 새 노드)
function applyPatch(rootNode, patch) {
  const applyByType = PATCH_APPLIERS[patch.type];

  if (!applyByType) {
    throw new TypeError(`[applyPatches] 알 수 없는 Patch 타입입니다: ${patch.type}`);
  }

  const replacedRoot = applyByType(rootNode, patch);

  return replacedRoot || rootNode;
}

const PATCH_APPLIERS = {
  [PATCH_TYPES.INSERT]: applyInsert,
  [PATCH_TYPES.REMOVE]: applyRemove,
  [PATCH_TYPES.REPLACE]: applyReplace,
  [PATCH_TYPES.UPDATE_TEXT]: applyUpdateText,
  [PATCH_TYPES.UPDATE_PROPS]: applyUpdateProps,
};

function applyInsert(rootNode, patch) {
  const parentNode = getDomNodeByPath(rootNode, patch.path);
  const childNodes = getVisibleChildNodes(parentNode);

  if (patch.index > childNodes.length) {
    throw new RangeError('[applyPatches] 경로를 찾을 수 없습니다');
  }

  // index가 자식 수와 같으면 referenceNode가 null이라 맨 뒤에 붙는다.
  const referenceNode = childNodes[patch.index] || null;

  parentNode.insertBefore(render(patch.node), referenceNode);
}

function applyRemove(rootNode, patch) {
  getDomNodeByPath(rootNode, patch.path).remove();
}

function applyReplace(rootNode, patch) {
  const targetNode = getDomNodeByPath(rootNode, patch.path);
  const newNode = render(patch.node);

  targetNode.replaceWith(newNode);

  if (targetNode === rootNode) {
    return newNode;
  }

  return null;
}

function applyUpdateText(rootNode, patch) {
  getDomNodeByPath(rootNode, patch.path).textContent = patch.text;
}

function applyUpdateProps(rootNode, patch) {
  const targetNode = getDomNodeByPath(rootNode, patch.path);

  for (const [key, value] of Object.entries(patch.set)) {
    setProp(targetNode, key, value);
  }

  for (const key of patch.remove) {
    removeProp(targetNode, key);
  }
}

function getDomNodeByPath(rootNode, path) {
  let currentNode = rootNode;

  for (const index of path) {
    currentNode = getVisibleChildNodes(currentNode)[index];

    if (!currentNode) {
      throw new RangeError('[applyPatches] 경로를 찾을 수 없습니다');
    }
  }

  return currentNode;
}

// domToVNode가 무시한 노드를 빼고 세어야 VNode의 children 인덱스와 맞는다.
function getVisibleChildNodes(parentNode) {
  return Array.from(parentNode.childNodes).filter((childNode) => !isIgnoredNode(childNode));
}
