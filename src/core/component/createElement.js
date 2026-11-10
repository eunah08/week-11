import { NODE_TYPES, isVNode } from '../vdom/index.js';
import { getCurrentComponent, setCurrentComponent } from './hookContext.js';

const INVALID_TAG_MESSAGE = '[createElement] tag가 올바르지 않습니다';
const INVALID_PROPS_MESSAGE = '[createElement] props는 객체 또는 null이어야 합니다';
const INVALID_CHILD_MESSAGE = '[createElement] 자식으로 쓸 수 없는 값입니다';
const INVALID_COMPONENT_RESULT_MESSAGE = '[createElement] 자식 컴포넌트는 VNode를 반환해야 합니다';

// 공백만 있는 글자는 VNode로 만들지 않는다. (Phase 1 문서 6.2)
// 기준은 vdom/isIgnoredNode.js의 WHITESPACE_ONLY_PATTERN과 같아야 applyPatches의 path가 맞는다.
const WHITESPACE_ONLY_PATTERN = /^[ \t\n\r\f]*$/;

/**
 * 요소 Virtual DOM을 만든다. (Phase 2 문서 5.1)
 * tag가 함수(자식 컴포넌트)면 { ...props, children }로 바로 호출한 결과 VNode를 돌려준다.
 * 자식 컴포넌트를 부르는 동안에는 현재 렌더링 중인 컴포넌트를 null로 바꿨다가 되돌린다.
 * @param {string | Function} tag HTML 태그 이름 또는 자식 컴포넌트 함수
 * @param {object | null} props 속성 (자식 컴포넌트에는 그대로 props로 전달)
 * @param {...any} children 자식 (문자열, 숫자, VNode, 중첩 배열 모두 가능)
 * @returns {object} VNode
 * @throws {TypeError} tag가 문자열도 함수도 아니거나, props·자식·컴포넌트 반환값이 올바르지 않을 때
 */
export function createElement(tag, props, ...children) {
  if (typeof tag === 'function') {
    return createComponentVNode(tag, props, children);
  }

  if (typeof tag !== 'string') {
    throw new TypeError(INVALID_TAG_MESSAGE);
  }

  return {
    type: NODE_TYPES.ELEMENT,
    tag: tag.toLowerCase(),
    props: copyProps(props),
    children: normalizeChildren(children),
  };
}

// props는 복사해서 쓴다. 호출한 쪽의 객체와 VNode가 같은 객체를 공유하지 않게 하기 위해서다.
function copyProps(props) {
  if (props === null || props === undefined) {
    return {};
  }

  if (typeof props !== 'object' || Array.isArray(props)) {
    throw new TypeError(INVALID_PROPS_MESSAGE);
  }

  return { ...props };
}

// 자식 목록을 평탄화하고 VNode 배열로 바꾼다. 렌더링 대상이 아닌 값은 뺀다.
function normalizeChildren(children) {
  const childVNodes = [];

  for (const child of children.flat(Infinity)) {
    const childVNode = toChildVNode(child);

    if (childVNode !== null) {
      childVNodes.push(childVNode);
    }
  }

  return childVNodes;
}

// 자식 하나를 VNode로 바꾼다. 렌더링 대상이 아니면 null이다.
function toChildVNode(child) {
  if (child === null || child === undefined || typeof child === 'boolean') {
    return null;
  }

  if (isVNode(child)) {
    return child;
  }

  if (typeof child === 'string' || typeof child === 'number') {
    return findTextVNode(String(child));
  }

  throw new TypeError(INVALID_CHILD_MESSAGE);
}

function findTextVNode(text) {
  if (WHITESPACE_ONLY_PATTERN.test(text)) {
    return null;
  }

  return { type: NODE_TYPES.TEXT, text };
}

// 자식 컴포넌트를 부른다. 자식 안에서 Hook을 쓰면 현재 컴포넌트가 null이라 에러가 난다.
function createComponentVNode(component, props, children) {
  // 자식 컴포넌트가 props를 바꾸지 못하게 얼려서 넘긴다. (Phase 2 문서 2장)
  const componentProps = Object.freeze({
    ...copyProps(props),
    children: normalizeChildren(children),
  });
  const parentComponent = getCurrentComponent();
  let vnode = null;

  setCurrentComponent(null);

  try {
    vnode = component(componentProps);
  } finally {
    setCurrentComponent(parentComponent);
  }

  if (!isVNode(vnode)) {
    throw new TypeError(INVALID_COMPONENT_RESULT_MESSAGE);
  }

  return vnode;
}
