// vdom 모듈의 공개 진입점. 다른 core 폴더는 이 파일을 통해 불러온다. (Phase 1 문서 3.5)
export { NODE_TYPES } from './nodeTypes.js';
export { domToVNode } from './domToVNode.js';
export { render } from './render.js';
export { setProp, removeProp } from './props.js';
export { isIgnoredNode } from './isIgnoredNode.js';
