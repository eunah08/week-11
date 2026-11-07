# 구현 안내

> Phase 1 문서 13.1, Phase 2 문서 15.1 기준으로 채운다.

## Diff와 Patch의 동작 방식

- `diff`(`src/core/diff/diff.js`)는 루트부터 재귀로 내려가며 노드 하나씩 비교한다.
    - 타입이나 `tag`가 다르면 `REPLACE` 하나만 만들고 그 아래는 보지 않는다.
    - 텍스트끼리는 `text`가 다를 때 `UPDATE_TEXT`를 만든다.
    - 같은 `tag`의 요소는 props를 `Object.is`로 비교해 `set`(추가·변경)과 `remove`(삭제)를 모은
      `UPDATE_PROPS`를 만들고, 자식을 같은 인덱스끼리 비교한다.
    - 자식이 늘면 `INSERT`(index 오름차순), 줄면 `REMOVE`(index 내림차순)를 만든다.
- `applyPatches`(`src/core/patch/applyPatches.js`)는 Patch를 앞에서부터 하나씩 적용한다.
    - `path`는 루트의 `childNodes`를 따라가되, `domToVNode`가 무시하는 노드(주석, 공백 텍스트,
      `script`, `style`)는 건너뛰고 센다. 기준은 `src/core/vdom/isIgnoredNode.js` 하나로 둘이 공유한다.
    - 루트가 `REPLACE`되면 새 루트를 반환한다.
- 속성·이벤트를 넣고 빼는 규칙은 `src/core/vdom/props.js`의 `setProp`, `removeProp`에 모아
  `render`와 `applyPatches`가 같이 쓴다.

## Hook 값이 유지되는 방식

Phase 2에서 작성한다.

## 지원하는 DOM·속성·이벤트 범위

- 노드 : 요소, 텍스트. 주석·공백 텍스트·`script`·`style`은 무시한다.
- 속성 값 : 문자열, 불리언(`true`는 빈 문자열 속성, `false`·`null`·`undefined`는 제거), 함수(이벤트).
  그 밖의 값은 `TypeError`.
- `value`, `checked`는 속성과 함께 DOM 프로퍼티도 맞춘다.
- `style`은 문자열만 지원한다.
- 이벤트 : `onClick`처럼 `on` + 이벤트 이름. 이전 핸들러를 DOM의 `handlers` 객체에 기억해 두고,
  바뀌면 이전 것을 `removeEventListener`로 뺀 뒤 새로 단다.
- `domToVNode`는 속성 값을 문자열 그대로 읽는다. 값이 없는 속성(`disabled`, `checked`)은 `''`이다.

## 렌더링 시점과 Batching 지원 여부

- Phase 1 : Patch 버튼을 누를 때만 diff와 반영이 일어난다. Batching은 없다.
- Phase 2 내용은 Phase 2에서 작성한다.

## 알려진 제한

- 자식은 인덱스로만 비교하므로 중간 자식을 지우면 뒤 자식들이 전부 변경으로 보인다. (`key` 미지원)
- 테스트 영역(`contenteditable`)에서는 속성을 직접 편집할 수 없다. checkbox를 클릭해도
  속성이 아니라 프로퍼티만 바뀌므로 Patch에 반영되지 않는다.
- 테스트 영역의 가장 바깥 노드를 지우면 비교할 루트가 없어 에러를 보여준다.
- 테스트 영역 최상위에 노드를 여러 개 만들면 첫 노드만 비교한다.

## 추가 구현 요소

- 없음 (Phase 1 기본 범위)

## 핵심 로직을 설명할 때 참고할 파일

- `src/core/vdom/domToVNode.js`, `src/core/vdom/render.js`, `src/core/vdom/props.js`
- `src/core/diff/diff.js`, `src/core/patch/applyPatches.js`
- `src/demos/phase_one/StateHistory.js`, `src/demos/phase_one/app.js`
- 테스트 : 로컬 서버를 띄우고 `/tests/index.html`을 연다.
