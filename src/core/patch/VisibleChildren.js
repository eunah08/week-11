import { isIgnoredNode } from '../vdom/index.js';

/**
 * 한 번의 applyPatches 동안 부모별 "보이는 자식 목록"을 기억한다.
 * 보이는 자식 = domToVNode가 무시하지 않는 자식. 이 목록의 인덱스가 VNode children 인덱스와 같다.
 *
 * patch마다 목록을 새로 세면 같은 부모 아래 patch N개에 O(N²)이 걸려서 기억해 둔다.
 * 그 대신 DOM을 바꿀 때마다 같은 이름의 메서드를 한 번 불러 목록도 똑같이 바꿔야 한다.
 *   parentNode.insertBefore(...)  →  visibleChildren.insert(...)
 *   node.remove()                 →  visibleChildren.remove(...)
 *   node.replaceWith(...)         →  visibleChildren.replace(...)
 *   자식이 통째로 바뀌는 변경      →  visibleChildren.forget(...)
 */
export class VisibleChildren {
  #childrenByParent = new Map();

  of(parentNode) {
    if (!this.#childrenByParent.has(parentNode)) {
      this.#childrenByParent.set(parentNode, countVisibleChildren(parentNode));
    }

    return this.#childrenByParent.get(parentNode);
  }

  insert(parentNode, index, newNode) {
    if (!isIgnoredNode(newNode)) {
      this.of(parentNode).splice(index, 0, newNode);
    }
  }

  // target은 findByPath의 결과다. 루트는 부모 목록을 기억하지 않으므로 고칠 것이 없다.
  remove({ parentNode, indexInParent }) {
    if (parentNode !== null) {
      this.of(parentNode).splice(indexInParent, 1);
    }
  }

  replace(target, newNode) {
    if (isIgnoredNode(newNode)) {
      this.remove(target);
    } else if (target.parentNode !== null) {
      this.of(target.parentNode)[target.indexInParent] = newNode;
    }
  }

  // textContent처럼 자식을 통째로 바꾸는 변경 뒤에는 그 노드의 목록을 버리고 다음에 다시 센다.
  forget(parentNode) {
    this.#childrenByParent.delete(parentNode);
  }
}

function countVisibleChildren(parentNode) {
  return Array.from(parentNode.childNodes).filter((childNode) => !isIgnoredNode(childNode));
}
