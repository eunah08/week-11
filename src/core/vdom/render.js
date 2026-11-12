import { NODE_TYPES } from './nodeTypes.js';
import { setProp, restoreCurrentProps } from './props.js';

const UNKNOWN_TYPE_MESSAGE = '[render] 알 수 없는 VNode 타입입니다';

// VNode 종류별 DOM 생성 방법. 새 VNode 종류를 그리려면 이 표에 한 줄을 추가한다.
const RENDERER_BY_VNODE_TYPE = new Map([
  [NODE_TYPES.TEXT, renderTextVNode],
  [NODE_TYPES.ELEMENT, renderElementVNode],
]);

/**
 * Virtual DOM으로 새 DOM 노드를 만든다. (Phase 1 문서 7.2)
 * @param {object} vnode 변환할 VNode
 * @returns {Node} document에 붙이지 않은 새 DOM 노드
 * @throws {TypeError} vnode.type이 알 수 없는 값일 때
 */
export function render(vnode) {
  const renderByType = findRenderer(vnode);

  if (renderByType === null) {
    throw new TypeError(UNKNOWN_TYPE_MESSAGE);
  }

  return renderByType(vnode);
}

function findRenderer(vnode) {
  if (vnode === null || typeof vnode !== 'object') {
    return null;
  }

  return RENDERER_BY_VNODE_TYPE.get(vnode.type) || null;
}

// createTextNode는 글자를 HTML로 해석하지 않으므로 글자 안의 태그가 실행되지 않는다.
function renderTextVNode(vnode) {
  return document.createTextNode(vnode.text);
}

function renderElementVNode(vnode) {
  const domNode = document.createElement(vnode.tag);

  for (const [key, value] of Object.entries(vnode.props)) {
    setProp(domNode, key, value);
  }

  for (const childVNode of vnode.children) {
    domNode.appendChild(render(childVNode));
  }

  // <select>의 value는 option이 붙은 뒤에야 먹으므로 자식을 붙인 다음 한 번 더 맞춘다.
  restoreCurrentProps(domNode, vnode.props);

  return domNode;
}
