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

- 루트 컴포넌트마다 `FunctionComponent` 인스턴스가 `hooks` 배열 하나를 가진다.
  Hook을 한 번 부를 때마다 배열의 칸 하나를 쓰고, 렌더링이 시작될 때 `hookIndex`를 0으로 되돌려
  같은 순서로 같은 칸을 다시 찾아간다. 그래서 값이 렌더링이 끝나도 유지된다.
- 칸을 꺼내는 일은 `hooks/getHookSlot.js`의 `getHookSlot` 한 곳에서 한다. 이 함수가
  렌더링 문맥(`component/hookContext.js`)과 Hook 종류(`HOOK_KINDS`)를 검사하므로 세 Hook이
  같은 에러 메시지를 낸다.
- 칸의 모양
    - `useState` : `{ kind: 'state', value, setValue }`. `setValue`는 칸을 만들 때 한 번만 만들어
      렌더링이 바뀌어도 같은 함수다.
    - `useEffect` : `{ kind: 'effect', dependencies, cleanup }`
    - `useMemo` : `{ kind: 'memo', dependencies, value }`
- 의존성 비교는 `hooks/isDepsChanged.js` 한 곳에서 한다. 이전 의존성이 `undefined`(첫 실행)이거나
  이번 의존성이 `undefined`(생략)이면 바뀐 것으로 본다. 그래서 생략하면 매번, `[]`이면 첫 렌더링에서만 실행된다.
- Effect는 렌더링 중에는 `pendingEffects`에 넣어 두기만 하고, `render`·`applyPatches`로 DOM을 반영한 뒤
  등록한 순서대로 실행한다. 각 Effect는 실행 직전에 자기 이전 cleanup을 먼저 부르고, 의존성은
  실제로 실행한 뒤에 기억한다. `unmount()`는 `hooks`를 앞에서부터 돌며 남은 cleanup을 부른다.
- `unmount()`는 `hooks`를 비운다. 상태 변경 함수는 자기 칸이 아직 `hooks`에 있는지 확인하므로,
  해제된 뒤(다시 mount한 뒤의 옛 함수 포함)에는 아무 일도 하지 않는다.

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
- Phase 2 : **Batching을 지원한다.** (협약 19장 결정 필요 — 팀 확인 전 임시 채택)
    - 상태 변경 함수는 값만 바꾸고 `requestUpdate()`로 마이크로태스크 1개를 예약한다.
      같은 핸들러 안에서 상태를 여러 번 바꿔도 `update()`는 한 번만 실행된다. (코드 표시: `// Batching`)
    - 그래서 setter 직후의 화면은 아직 그대로다. 코드에서 바로 확인해야 하면 `flushUpdate()`를 부른다.
      (테스트는 `flushUpdate()`로 동기 반영, 실제 마이크로태스크 자동 반영은 브라우저에서 확인함)
    - 마이크로태스크에서 난 에러는 `instance.onError`가 있으면 거기로 넘기고, 없으면 그대로 던진다.
      데모는 `app.js`에서 `onError`로 에러 문구를 화면에 보여 준다.
    - 렌더링 중에 상태를 바꾸면 `Error`다. (무한 반복 방지, 협약 14장 엣지 케이스)
    - Effect 안에서 상태를 바꾸면 `update()`가 중첩해서 실행된다. 이때 아직 실행하지 않은
      낡은 렌더링의 Effect는 건너뛴다. (`FunctionComponent`의 `renderCount`)

## 차별화 요소 (Phase 2 추가)

- Debouncing (`hooks/useDebouncedValue.js`) : `useState` + `useEffect`(cleanup에서 `clearTimeout`)로 만든
  Hook. 값이 `delayMs` 동안 안 바뀌어야 따라온다. 데모 검색창은 300ms(`SEARCH_DELAY_MS`).
- Batching : 위 "렌더링 시점" 참고. 관련 파일 `FunctionComponent.js`, `hooks/useState.js`.
- Memoization : `memo(component)`(`component/memo.js`), `useCallback`(`hooks/useCallback.js`),
  `useMemo`. `memo` 컴포넌트는 `MemoCache`(`component/MemoCache.js`)가 "컴포넌트 + 호출 순번" 기준으로
  props를 얕게 비교해 같으면 이전 VNode를 그대로 돌려준다. 배열 props는 항목별로 비교한다.
- Selective Rendering : `memo`가 같은 VNode 객체를 돌려주면 `diff`가 참조가 같다고 보고 하위 비교를
  통째로 건너뛴다. (`diff.js` `diffNode` 맨 위, **Phase 1 파일을 수정한 유일한 곳**)
- 각 코드 위에 `// Batching`, `// Memoization`, `// Selective Rendering`, `// Debouncing` 주석을 달았다.

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

## Phase 2 알려진 제한

- `createElement`는 공백만 있는 문자열 자식(`''`, `' '`, `'\n'`)을 버린다. Phase 1 문서 6.2 "공백만 있는
  텍스트는 VNode로 만들지 않는다"와 `applyPatches`의 path 계산을 맞추기 위해서다. 공백 기준은
  `vdom/isIgnoredNode.js`의 `WHITESPACE_ONLY_PATTERN`과 같은 정규식을 `createElement.js`에도 적어 두었다.
  (한쪽만 바꾸면 path가 어긋나므로 같이 고친다. 이 파일을 export해서 공유하자는 제안은 팀 확인 필요)
- 글자 앞뒤 공백(`' 남은 개수 '`)은 그대로 둔다. `&nbsp;`만 있는 글자는 남는다.
- Hook 호출 순서 검사는 "같은 칸의 Hook 종류가 바뀌면 `Error`"까지다. 조건문 때문에 호출 개수만
  줄어들고 종류는 그대로 이어지는 경우(예: `useState`가 연달아 있을 때)는 잡지 못한다.
- `useEffect`·`useMemo`의 의존성이 배열이 아닌 값이면 검사하지 않는다. (협약에 에러 조건이 없다)
- Effect의 cleanup은 Effect마다 "자기 cleanup → 자기 Effect" 순서다. 여러 Effect의 cleanup을
  모두 먼저 부르는 방식(React)과 순서가 다를 수 있다.
- 자식 컴포넌트는 인덱스로만 비교되므로(`key` 미지원) 목록 중간을 지우면 뒤 항목들이 모두 변경으로
  처리된다. 화면은 맞게 나오지만 DOM 노드가 재사용되는 짝이 달라질 수 있다.
- `update()` 도중 `applyPatches`가 실패하면 DOM과 `currentVNode`가 어긋날 수 있다. (Phase 1과 같은 제한)
- Memoization 캐시는 `key`가 없어 인덱스(호출 순번) 기준이다. 목록 중간을 지우면 적중률만 떨어질 뿐
  화면은 항상 맞다.
- `diff`는 참조 shortcut 전에도 `isVNode`로 전체 트리를 검사하므로 비용은 줄지만 0은 아니다.
- `memo`는 자식(`children`) 배열도 props로 비교한다. 자식이 매번 새로 만들어지면 적중하지 않는다.
- `FunctionComponent`는 `unmount()` 후 다시 `mount()`할 수 있고, 이때 Hook 값은 새로 시작한다.
  mount하지 않은 상태의 `unmount()`는 아무 일도 하지 않는다. (협약에 없어 정한 해석, 팀 확인 필요)

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

- Phase 2에서 협약 5·6·7장 밖에 정한 것 (모두 팀 확인 필요)
    - `createElement` : `props`가 객체·`null`이 아니면 `TypeError`, 자식으로 쓸 수 없는 값(일반 객체,
      심볼 등)이면 `TypeError`, 자식 컴포넌트가 VNode를 돌려주지 않으면 `TypeError`.
    - 자식 컴포넌트에는 `Object.freeze`한 `{ ...props, children }`를 넘긴다. `children`은 정규화한
      VNode 배열이다. 협약 2장 "props를 변경하지 않는다"를 실제로 막는다.
    - `FunctionComponent` 생성자는 `container`가 DOM 요소가 아니면 `TypeError`다.
    - `useState` 상태 변경 함수를 렌더링 중에 부르면 `Error` — `[useState] 렌더링 중에는 상태를 바꿀 수 없습니다`
    - 보조 파일 : `component/index.js`, `patch/index.js` (다른 폴더는 `index.js`로 불러오는 규칙에 맞추기 위해),
      `hooks/getHookSlot.js`, `hooks/isDepsChanged.js`
- 데모(`src/demos/phase_two`) : 할 일 목록. 루트 `todoApp`이 `useState` 3개(할 일, 입력 중인 글자, 필터),
  `useMemo` 2개(보이는 할 일, 남은 개수), `useEffect` 1개(탭 제목에 남은 개수 붙이기, cleanup으로 되돌리기)를
  쓰고, 자식 `todoForm`·`todoFilter`·`todoList`·`todoItem`·`todoSummary`는 props만 받는 순수 함수다.
    - 함수 이름은 협약 3장(`camelCase`)대로 `todoApp`처럼 소문자로 시작한다.
    - 필터를 추가하려면 `todoFilter.js`의 `FILTERS`, `SHOW_RULE_BY_FILTER`, `FILTER_BUTTONS`에 한 줄씩 더한다.
    - 화면 요소는 `data-role`로 찾는다. 바꾸려면 `app.js`의 `PAGE_SELECTORS`를 고친다.

## 핵심 로직을 설명할 때 참고할 파일

- `src/core/vdom/domToVNode.js`, `src/core/vdom/render.js`, `src/core/vdom/props.js`
- `src/core/diff/diff.js`, `src/core/patch/applyPatches.js`
- `src/demos/phase_one/StateHistory.js`, `src/demos/phase_one/app.js`
- Phase 2 : `src/core/component/FunctionComponent.js`, `src/core/component/createElement.js`,
  `src/core/hooks/getHookSlot.js`, `src/core/hooks/useState.js`, `src/core/hooks/useEffect.js`,
  `src/core/hooks/useMemo.js`, `src/core/component/MemoCache.js`, `src/core/hooks/useDebouncedValue.js`, `src/demos/phase_two/components/todoApp.js`
- 테스트 : 로컬 서버를 띄우고 `/tests/index.html`을 연다.
