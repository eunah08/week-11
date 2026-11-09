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
    - 부모별 "보이는 자식 목록"(무시하는 노드를 뺀 자식)은 한 번 센 뒤 기억해 두고, DOM을 바꿀 때 같은 이름의
      메서드로 함께 고친다(`src/core/patch/VisibleChildren.js`의 `VisibleChildren` 클래스). 같은 부모 아래 Patch가
      많아도 O(N²)이 되지 않는다.
    - 적용 전에 모든 Patch의 종류(알 수 없는 `type`이면 `TypeError`)와 형태(`path`가 배열인지, `INSERT`에 정수
      `index`와 VNode `node`가 있는지 등, 틀리면 `TypeError`)를 먼저 확인한다. 그래서 이 에러에서는 DOM이 그대로다.
      경로·index 범위 에러(`RangeError`)는 적용하다가 만나므로 앞선 Patch는 이미 적용된 상태다.
    - `<select>`의 `value`는 그 값의 `<option>`이 있어야 먹히므로, Patch 목록에서 option이 뒤에 `INSERT`되어도
      모든 Patch를 적용한 뒤 마지막 `value`를 한 번 더 맞춘다. `render`도 자식을 붙인 뒤 `value`, `checked`를
      다시 맞춘다(`restoreCurrentProps`).
- 속성·이벤트를 넣고 빼는 규칙은 `src/core/vdom/props.js`의 `setProp`, `removeProp`에 모아
  `render`와 `applyPatches`가 같이 쓴다.

## Hook 값이 유지되는 방식

Phase 2에서 작성한다.

## 지원하는 DOM·속성·이벤트 범위

- 노드 : 요소, 텍스트. 주석·공백 텍스트·`script`·`style`은 무시한다.
    - "공백"은 HTML 소스 공백(스페이스, 탭, 줄바꿈, CR, FF)만 뜻한다. `&nbsp;`만 있는 텍스트는
      사용자가 친 띄어쓰기이므로 남긴다. (협약 6.2가 공백 범위를 정하지 않아 정한 해석, 팀 확인 필요)
- 속성 값 : 문자열, 불리언(`true`는 빈 문자열 속성, `false`·`null`·`undefined`는 제거), 함수(이벤트).
  그 밖의 값은 `TypeError`.
- `value`, `checked`는 속성과 함께 DOM 프로퍼티도 맞춘다.
- `style`은 문자열만 지원한다.
- 이벤트 : `onClick`처럼 `on` + 이벤트 이름. 이전 핸들러를 DOM의 `handlers` 객체에 기억해 두고,
  바뀌면 이전 것을 `removeEventListener`로 뺀 뒤 새로 단다.
    - `on`으로 시작하는 키(대소문자 무관)에 함수가 아닌 값(문자열 등)이 오면 `TypeError`.
      문자열 `onclick="..."`을 속성으로 넣으면 브라우저가 코드로 실행하기 때문이다.
      (협약 6.3 "이벤트 값은 함수"를 8.4 "문자열이면 setAttribute"보다 우선한 해석, 팀 확인 필요)
    - 그래서 `onclick="..."` 속성이 있는 DOM을 `domToVNode`로 읽어 `render`·`applyPatches`에 넘기면
      왕복 변환이 되지 않고 `TypeError`가 난다. (의도한 동작)
    - 이벤트 키에 `null`·`false`·`undefined`가 오면 리스너를 뺀다. 이벤트를 지울 때는 HTML로
      직접 들어 있던 문자열 이벤트 속성도 함께 지운다.
- `domToVNode`는 속성 값을 문자열 그대로 읽는다. 값이 없는 속성(`disabled`, `checked`)은 `''`이다.

## 규칙을 바꿀 때 고칠 곳

종류별로 다르게 처리하는 부분은 모두 `Map` 표로 모았다. 새 종류는 표에 한 줄을 추가하면 된다.
일반 객체 대신 `Map`을 쓰는 이유는 `'toString'`, `'constructor'` 같은 기본 객체 이름을 표에 있는
것으로 착각하지 않게 하기 위해서다.

| 바꾸고 싶은 것 | 파일 | 고칠 곳 |
|---|---|---|
| 무시할 태그 | `vdom/isIgnoredNode.js` | `IGNORED_TAGS` |
| 공백 텍스트 기준 | `vdom/isIgnoredNode.js` | `WHITESPACE_ONLY_PATTERN` |
| 노드 종류별 무시 여부 | `vdom/isIgnoredNode.js` | `IGNORE_RULE_BY_NODE_TYPE` |
| DOM 노드 종류별 VNode 변환 | `vdom/domToVNode.js` | `CONVERTER_BY_NODE_TYPE` |
| VNode 형식 검사 (diff, StateHistory, applyPatches 공용, 자식까지 확인) | `vdom/isVNode.js` | `SHAPE_CHECKER_BY_VNODE_TYPE` |
| VNode 종류별 DOM 생성 | `vdom/render.js` | `RENDERER_BY_VNODE_TYPE` |
| 이벤트로 볼 키 | `vdom/props.js` | `EVENT_PREFIX` |
| 프로퍼티까지 맞출 속성 (`value`, `checked`) | `vdom/props.js` | `PROPERTY_SYNCER_BY_KEY` |
| VNode 종류별 비교 | `diff/diff.js` | `SAME_TYPE_DIFFER_BY_VNODE_TYPE` |
| Patch 종류별 적용 | `patch/applyPatches.js` | `PATCH_APPLIER_BY_TYPE` |
| Patch 종류별 필수 값 검사 | `patch/applyPatches.js` | `SHAPE_CHECKER_BY_PATCH_TYPE` |
| 화면 요소의 id·data-action | `demos/phase_one/app.js` | `PAGE_SELECTORS` |
| 버튼과 동작 연결 | `demos/phase_one/app.js` | `BUTTON_ACTIONS` |

- Patch 종류를 새로 만들면 `PATCH_APPLIER_BY_TYPE`과 `SHAPE_CHECKER_BY_PATCH_TYPE` 표에 함께 추가한다.
- VNode 종류를 새로 만들면 `isVNode`, `render`, `diff`의 표에 함께 추가한다. 무시하지 않는 DOM 노드
  종류를 늘리면 `isIgnoredNode`와 `domToVNode`의 표에 함께 추가한다. 하나를 빠뜨리면 조용히 넘어가지
  않고 에러가 난다.

## 렌더링 시점과 Batching 지원 여부

- Phase 1 : Patch 버튼을 누를 때만 diff와 반영이 일어난다. Batching은 없다.
- Phase 2 내용은 Phase 2에서 작성한다.

## 알려진 제한

- 자식은 인덱스로만 비교하므로 중간 자식을 지우면 뒤 자식들이 전부 변경으로 보인다. (`key` 미지원)
- 테스트 영역(`contenteditable`)에서는 속성을 직접 편집할 수 없다. checkbox를 클릭해도
  속성이 아니라 프로퍼티만 바뀌므로 Patch에 반영되지 않는다.
- 테스트 영역의 가장 바깥 노드를 지우면 비교할 루트가 없어 에러를 보여준다.
- 테스트 영역 최상위에 노드를 여러 개 만들면 첫 노드만 비교한다.
- 이력 이동 중 실제 영역 반영이 실패하면 이력 위치는 되돌리지만, 실패 전에 적용된 Patch는 되돌리지 않는다.
  Patch 버튼도 같아서, 중간에 실패하면 실제 영역이 일부만 바뀐 채 이력에는 저장되지 않는다.
  이 상태에서 다음 diff는 실제 DOM과 다른 VNode를 기준으로 계산되므로, 에러가 나면 새로고침해야 한다.
- 속성 이름이 `__proto__`이면 일반 객체인 `props`에 들어가지 않아 조용히 빠진다.

## 보안

- 막은 것
    - 문자열 이벤트 속성(`onerror="..."` 등) : `props.js`에서 `TypeError`. 데모에서는 에러 메시지로 보이고
      실제 영역에는 반영되지 않는다.
    - `script`, `style` : `domToVNode`가 버리므로 테스트 영역에서 실제 영역으로 넘어가지 않는다.
    - 텍스트 : `createTextNode`로 만들어 HTML로 해석되지 않는다. `innerHTML`은 코드에 적은 `SAMPLE_HTML`에만 쓴다.
    - 에러 메시지는 `textContent`로 보여 준다.
- 막지 못한 것 (협약과 부딪혀 코드로 막지 않음)
    - `tag`가 `script`인 VNode를 `render`하면 실행되는 `<script>`가 만들어진다. Phase 1에서는 경로가 없고,
      Phase 2에서 `createElement('script', …)`를 쓰면 생긴다. 막으려면 7.2 에러 조건을 바꿔야 한다.
    - `href="javascript:…"` 같은 URL 속성은 8.4 "문자열이면 setAttribute"대로 그대로 들어간다.
    - 테스트 영역은 살아 있는 DOM이라, 브라우저가 붙여넣기에서 걸러 내지 못한 이벤트 속성은 테스트 영역
      안에서 바로 실행될 수 있다. (실제 영역으로는 넘어가지 않는다)

## 추가 구현 요소

- 협약 7장 표에 없지만 `core/index.js`에서 공개하는 보조 함수
    - `isIgnoredNode(domNode)` : 데모가 비교할 루트를 찾을 때 쓴다.
    - `isVNode(value)` (`src/core/vdom/isVNode.js`) : VNode 형식 검사. 자식까지 끝까지 확인한다.
      `diff`, `StateHistory`, `applyPatches`가 같이 쓴다.
    - `cloneVNode(vnode)` (`src/core/vdom/cloneVNode.js`) : VNode 깊은 복사. 지금 core 안에서는 쓰지 않는다.
      Phase 2에서 이전 VNode를 보관할 때 쓰려고 두었다. (협약 4장 "미리 만든 보조 파일 금지"와 부딪히면 지운다)

## 핵심 로직을 설명할 때 참고할 파일

- `src/core/vdom/domToVNode.js`, `src/core/vdom/render.js`, `src/core/vdom/props.js`
- `src/core/diff/diff.js`, `src/core/patch/applyPatches.js`
- `src/demos/phase_one/StateHistory.js`, `src/demos/phase_one/app.js`
- 테스트 : 로컬 서버를 띄우고 `/tests/index.html`을 연다.
