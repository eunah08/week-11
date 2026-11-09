# 구현 안내

> Phase 1 문서 13.1, Phase 2 문서 15.1 기준으로 채운다.

## Diff와 Patch의 동작 방식

- `diff(previousVNode, nextVNode)`는 루트부터 재귀로 내려가며 노드를 하나씩 비교한다.
    - `type`이나 `tag`가 다르면 `REPLACE` 하나만 만들고 그 아래는 보지 않는다.
    - 둘 다 텍스트면 `text`가 다를 때만 `UPDATE_TEXT`를 만든다.
    - 같은 태그면 props를 비교해 `UPDATE_PROPS`(`set`, `remove`)를 만들고 자식을 비교한다.
    - 자식은 같은 인덱스끼리 비교한다. 남는 새 자식은 `INSERT`(index 오름차순),
      남는 이전 자식은 `REMOVE`(index 내림차순)로 만든다.
    - 입력 VNode는 바꾸지 않고, 결과 Patch는 새 배열로 만든다.
- `applyPatches(rootNode, patches)`는 Patch를 앞에서부터 순서대로 하나씩 적용한다.
    - `path`를 따라갈 때 `domToVNode`가 무시한 노드(주석, 공백 텍스트, `script`, `style`)는
      빼고 센다. 그래서 들여쓰기가 있는 HTML에도 VNode 인덱스가 그대로 맞는다.
    - 루트가 `REPLACE`되면 새 루트를 돌려주고, 호출한 쪽은 이 값을 다음 처리에 쓴다.
    - 바뀌지 않은 노드는 건드리지 않으므로 DOM 노드 동일성이 유지된다.
- 속성·이벤트를 DOM에 넣는 일은 `render`와 `applyPatches`가 함께 쓰는
  `src/core/vdom/domProps.js`(`setProp`, `removeProp`)에 모았다.

## Hook 값이 유지되는 방식

(Phase 2에서 작성)

## 지원하는 DOM·속성·이벤트 범위

- 노드 : 요소와 텍스트. 주석·공백만 있는 텍스트·`script`·`style`은 VNode로 만들지 않는다.
  그 밖의 Node(DocumentFragment 등)도 `domToVNode`에서 `null`을 돌려준다.
- 속성 값 : 문자열, 불리언, 함수(이벤트)
    - `true`는 `setAttribute(key, '')`, `false`·`null`·`undefined`는 속성 제거로 처리한다.
    - `value`, `checked`는 속성과 함께 DOM 프로퍼티도 맞춘다. 제거하면 `''`, `false`로 되돌린다.
    - `style`은 문자열만 지원한다.
- 이벤트 : `on`으로 시작하고 값이 함수인 props (`onClick` → `click`)
    - 핸들러는 DOM 노드의 `handlers` 객체에 기억해 두고, 교체·제거 때 이전 리스너를 먼저 뗀다.
    - 같은 키에 다른 값이 오면 이전 리스너를 먼저 떼므로 중복 등록되지 않는다.
- Phase 1 테스트 영역(`contenteditable`) 편집 범위 : 글자 수정·추가·삭제, Enter로 줄·항목 추가,
  Ctrl+B·Ctrl+I 서식. 속성은 화면에서 직접 바꿀 수 없어 개발자 도구로 바꾼다.

## 렌더링 시점과 Batching 지원 여부

- Phase 1 : 처음 페이지를 열 때 샘플 HTML을 실제 영역에 한 번 넣고, 같은 VNode로 테스트 영역을 `render`한다.
  이후 실제 영역은 Patch 버튼과 뒤로가기·앞으로가기 때만 `applyPatches`로 바뀐다.
- Batching : Phase 1에는 해당 없음 (Phase 2에서 작성)

## 알려진 제한

- 자식 비교가 인덱스 기준이라 중간 자식을 지우면 뒤 자식들이 모두 변경으로 보인다. (`key` 미지원)
- 공백만 있는 텍스트는 VNode로 만들지 않으므로, Phase 2에서 `' '` 같은 텍스트 VNode를 만들면
  `applyPatches`의 경로 계산과 어긋날 수 있다.
- 테스트 영역은 맨 바깥 요소 하나만 비교한다. 그 요소를 모두 지우면 Patch 때 에러를 보여 준다.
- `onclick="…"`처럼 HTML 문자열로 들어온 이벤트 속성은 이벤트가 아니라 일반 속성으로 다룬다.
- 이력 이동 중 `applyPatches`가 실패하면 History 위치는 이미 옮겨진 상태로 남는다.

## 추가 구현 요소

- `StateHistory.getPosition()` : 화면의 `#history-position`(이력 n / 전체) 표시용 읽기 전용 메서드
  (협약에 없는 이름, 팀 합의 필요)

## 테스트 실행

- 도구 : Node 기본 테스트 러너(`node:test`) + jsdom(개발 의존성)
- 실행 : `npm install` 후 `npm test`

## 핵심 로직을 설명할 때 참고할 파일

- `src/core/vdom/domToVNode.js` : DOM → VNode
- `src/core/vdom/render.js`, `src/core/vdom/domProps.js` : VNode → DOM, 속성·이벤트 적용
- `src/core/diff/diff.js` : 비교와 Patch 생성
- `src/core/patch/applyPatches.js` : Patch 적용, 경로 찾기
- `src/demos/phase_one/StateHistory.js`, `src/demos/phase_one/app.js` : 이력과 화면 연결
