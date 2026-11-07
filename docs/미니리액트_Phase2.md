# 미니 리액트 협약 — Phase 2

> 이 문서는 Phase 2(함수형 컴포넌트, State, Hooks) 전용 내용만 담는다. 이름 규칙, 코드 형식, 에러 처리, 디렉터리 구조, Virtual DOM 형식, Diff·Patch 규칙, AI 사용 지침 같은 **공통 규칙은 `미니리액트_Phase1.md`를 그대로 따른다.** 두 문서가 충돌하면 먼저 문서를 고치고 코드를 맞춘다.

## 0. 문서 사용법

- 대상 독자 : 팀원, AI
- 이 문서의 위치 : `docs/미니리액트_Phase2.md`
- 상태 : 팀원 3명이 각자 만든 문서를 취합한 초안 (팀 검토 필요)
- 적용 수준 : Phase 1 문서의 필수/권장/자율 3단계를 그대로 쓴다. 표시가 없는 규칙은 필수다.
- AI에게 일을 시킬 때는 이 문서의 해당 장 번호를 가리킨다. (예: "7장의 useState만 구현해")
- 변경 방법 : 팀원 전원 합의 후 문서를 먼저 수정하고, 20번 변경 이력에 기록한다.

## 1. 목적과 범위

- 함수형 컴포넌트, State, `useState`, `useEffect`, `useMemo`를 직접 구현한다.
- Phase 1의 `domToVNode`·`render`·`diff`·`applyPatches`를 재사용한다. Phase 2 전용으로 같은 기능을 중복 구현하지 않는다.
- 결과물 : 사용자의 입력과 클릭에 따라 부분 업데이트되는 웹 페이지 (데모 주제는 19번 미정 사항)
- 일정은 Phase 1 문서 1장을 따른다. (11월 10일 각자 Phase 2 완성, 9시 Phase 2 통합)

## 2. 추가 제약

- Hook은 루트 컴포넌트에서만 사용한다.
- State는 루트 컴포넌트에서만 관리한다. (Lifting State Up)
- 자식 컴포넌트는 상태 없이 props만 받아 Virtual DOM을 반환하는 순수 함수로 만든다.
- 자식 컴포넌트는 전달받은 props를 변경하지 않는다.
- 컴포넌트는 반드시 함수형으로 만든다.
- 컴포넌트 실행 중 직접 DOM을 수정하지 않는다.
- 루트 컴포넌트는 하나의 element Virtual DOM 루트를 반환한다.
- Phase 2에서 생성하는 Virtual DOM도 Phase 1 문서 6장과 같은 형식을 쓴다.

## 3. 관련 디렉터리

```text
src/core/component/
├── createElement.js
├── FunctionComponent.js
└── hookContext.js
src/core/hooks/
├── useState.js
├── useEffect.js
└── useMemo.js
src/demos/phase_two/
├── app.js
├── components/
└── styles.css
tests/core/component/
tests/core/hooks/
tests/demos/phase_two/
```

- 파일 이름 규칙과 배치 규칙은 Phase 1 문서 3·4장을 따른다.
- `isDepsChanged` 같은 내부 보조 함수는 `hooks` 모듈 안에 보조 파일로 둘 수 있다. (자율)

## 4. 모듈 책임

| 모듈 | 책임 |
|---|---|
| `component` | `createElement`, 루트 컴포넌트의 실행·갱신·해제, Hook 실행 문맥 관리 |
| `hooks` | 상태, Effect, 메모이제이션 관리 |
| `demos/phase_two` | 루트·자식 컴포넌트 작성, 화면 구성 |

- 의존 방향 : `demos → hooks → component → vdom·diff·patch`. 반대 방향 import는 금지한다.
- `core`는 `demos`를 import하지 않는다.
- 렌더링과 갱신 때 DOM을 바꾸는 일은 Phase 1의 `render`와 `applyPatches`만 한다.

## 5. 공개 함수 목록

| 함수 | 역할 |
|---|---|
| `createElement(tag, props, ...children)` | 요소 Virtual DOM 생성 |
| `useState(initialValue)` | 상태와 상태 변경 함수 반환 |
| `useEffect(effect, dependencies)` | DOM 반영 이후 실행할 작업 등록 |
| `useMemo(factory, dependencies)` | 의존성에 따라 계산 결과 재사용 |
| `new FunctionComponent(RootComponent, props, container)` | 루트 컴포넌트 실행 관리 |

- 데모는 `core/index.js`에 공개한 기능만 사용한다.
- 주요 함수의 JSDoc에는 입력, 반환값, 오류 조건을 기록한다.

### 5.1. `createElement`

- 시그니처 : `createElement(tag: string | Function, props: object | null, ...children: any[]): VNode`
- 매개변수 : `tag`(HTML 태그 이름 또는 자식 컴포넌트 함수), `props`, `children`(문자열, 숫자, VNode, 배열 모두 가능)
- 반환값 : VNode
- 자식 처리 :
    - 중첩된 자식 배열은 평탄화한다.
    - 문자열과 숫자는 텍스트 노드(`{ type: 'text', text }`)로 변환한다.
    - `null`, `undefined`, 참·거짓 값은 렌더링 대상에서 제외한다.
    - 이미 생성된 Virtual DOM은 자식으로 사용할 수 있다.
- 자식 컴포넌트 처리 : `tag`가 함수(자식 컴포넌트)면 `tag({ ...props, children })`를 바로 호출해 나온 VNode를 반환한다.
    - 호출하는 동안 현재 렌더링 중인 컴포넌트를 `null`로 바꿨다가 되돌린다. (자식 안에서 Hook을 쓰면 에러가 나게 하기 위해)
    - 자식 컴포넌트는 props를 받아 Virtual DOM을 반환하고, 그 결과가 자식 노드로 쓰인다.
- 에러 : `tag`가 문자열도 함수도 아니면 `TypeError` — `[createElement] tag가 올바르지 않습니다`
- 부수 효과 : 자식 컴포넌트 호출 중 현재 컴포넌트를 잠깐 바꾼다.
- 예시 : `createElement('p', { class: 'a' }, 'hi', 3)` → `p` 요소 VNode, 자식은 text `'hi'`, text `'3'`

## 6. FunctionComponent

루트 함수형 컴포넌트는 `FunctionComponent` 인스턴스가 관리한다.

```javascript
const instance = new FunctionComponent(
  RootComponent,
  props,
  container,
);
```

### 6.1. 구성

| 구성 | 역할 |
|---|---|
| `hooks` | Hook별 값을 렌더링 사이에 유지 |
| `hookIndex` | 렌더링 중 현재 Hook 위치 |
| `currentVNode` | 마지막으로 반영한 Virtual DOM |
| `rootNode` | 현재 루트 DOM 노드 |
| `pendingEffects` | 렌더링 중 등록되고 DOM 반영 뒤에 실행할 Effect 목록 |
| `mount()` | 최초 렌더링 |
| `update()` | 다시 실행하고 Diff·Patch 적용 |
| `unmount()` | Effect 정리 및 관리 대상 DOM 해제 |

### 6.2. `mount()`

1. 이미 mount되었으면 `Error` — `[FunctionComponent] 이미 mount되었습니다`
2. `hookIndex`를 0으로 되돌리고, 현재 렌더링 중인 컴포넌트를 이 인스턴스로 설정한다.
3. `RootComponent(props)`를 실행하고, 현재 컴포넌트를 `null`로 되돌린다.
4. 반환값이 element VNode가 아니면 `TypeError` — `[FunctionComponent] 루트는 element VNode여야 합니다`
5. `render`로 만든 DOM을 `container`에 넣는다. (`container` 안의 자식은 이 DOM 하나뿐이다)
6. `currentVNode`와 `rootNode`를 저장하고, `pendingEffects`를 실행한다.

### 6.3. `update()`

1. 위 2~4와 같이 새 VNode를 만든다.
2. `diff(currentVNode, 새 VNode)`로 `patches`를 구한다.
3. `rootNode = applyPatches(rootNode, patches)`로 변경을 반영한다. (루트가 교체되어도 반환된 루트를 쓰므로 안전하다)
4. `currentVNode`를 새 VNode로 바꾸고, `pendingEffects`를 실행한다.

### 6.4. `unmount()`

- 남아 있는 Effect의 cleanup을 호출한다.
- `container`의 관리 대상 DOM을 해제한다.
- 해제 뒤에는 상태 변경 함수가 호출되어도 갱신하지 않는다.

### 6.5. 에러

- `RootComponent`가 함수가 아니면 `TypeError`
- 위 6.2의 에러 조건
- 컴포넌트가 아직 mount되지 않았는데 `update()`를 호출하면 `Error` — `[FunctionComponent] mount 전에 update할 수 없습니다`

## 7. Hook 공통 규칙

- Hook은 루트 컴포넌트 함수의 최상위에서 호출한다.
- 조건문, 반복문, 중첩 함수 안에서 호출하지 않는다.
- 렌더링마다 호출 순서가 동일해야 한다.
- 렌더링 시작 시 Hook 탐색 위치(`hookIndex`)를 초기화한다.
- Hook 값은 렌더링이 끝나도 유지한다.
- 유효한 렌더링 문맥 밖(자식 컴포넌트 안 포함)에서 Hook을 호출하면 `Error`를 발생시킨다.
    - 메시지 예: `[useState] 루트 컴포넌트 렌더링 중에만 호출할 수 있습니다`
- 같은 칸의 Hook 종류가 렌더링마다 달라지면 `Error` — `[useState] Hook 호출 순서가 바뀌었습니다`
- (권장) `hooks` 배열의 칸 하나는 아래 셋 중 하나로 저장한다. 내부 구조는 자율이다.
    - state : `{ kind: 'state', value }`
    - effect : `{ kind: 'effect', dependencies, cleanup }`
    - memo : `{ kind: 'memo', dependencies, value }`

### 7.1. `useState`

```javascript
const [count, setCount] = useState(0);
```

- 초기값은 최초 상태 생성 시 적용한다. 초기값이 함수면 한 번만 호출해서 값으로 쓴다.
- 상태 변경 함수는 값과 함수형 갱신(`setCount(prev => prev + 1)`)을 지원한다.
- 함수형 갱신에는 해당 상태의 최신 값을 전달한다.
- 상태가 바뀌면 루트 컴포넌트를 다시 실행한다. (`update()`)
- 이전 값과 다음 값이 `Object.is` 기준으로 같으면 불필요한 갱신을 생략한다.
- 렌더링 함수 본문에서 상태 변경 함수를 호출하지 않는다.
- Batching은 선택 기능이다. 기본 구현은 상태 변경 함수가 호출될 때마다 즉시 `update()`한다. Batching을 도입하면 도입 여부, 갱신 시점, 테스트 대기 방법을 구현 안내에 기록한다.

### 7.2. `useEffect`

```javascript
useEffect(() => {
  // DOM 반영 후 수행할 작업

  return () => {
    // 이전 작업 정리
  };
}, [dependency]);
```

- Effect는 DOM 반영이 끝난 뒤 실행한다. 렌더링 중에는 `pendingEffects`에 넣어 두기만 한다.
- 의존성을 생략하면 매번 DOM 반영 후 실행한다.
- 빈 배열은 최초 DOM 반영 후 한 번만 실행한다.
- 의존성 배열은 각 항목을 `Object.is`로 비교하고, 하나라도 다르면 실행한다.
- 의존성이 달라져 다시 실행할 때 이전 cleanup을 먼저 호출한다.
- 컴포넌트를 해제할 때 남아 있는 cleanup을 호출한다.
- 콜백이 함수를 반환하면 cleanup으로 저장한다.
- Effect를 등록하는 함수 자체를 `async`로 작성하지 않는다.
- 에러 : 콜백이 함수가 아니면 `TypeError` — `[useEffect] 콜백 함수가 필요합니다`

### 7.3. `useMemo`

```javascript
const result = useMemo(() => {
  return calculateResult(value);
}, [value]);
```

- 최초 렌더링에서는 계산한다.
- 의존성이 같으면 이전 결과를 반환한다.
- 의존성이 달라지면 다시 계산한다.
- 의존성을 생략하면 렌더링마다 계산한다.
- 계산 함수에서 DOM 변경, 상태 변경, 외부 요청을 수행하지 않는다.
- 에러 : 계산 함수가 함수가 아니면 `TypeError` — `[useMemo] 계산 함수가 필요합니다`

## 8. 내부 함수 (자율)

아래는 권장 분리이며, 이름과 위치는 자율이다.

- `isDepsChanged(previousDependencies, nextDependencies): boolean` : 이전 의존성이 `undefined`이거나 길이가 다르거나, 하나라도 `Object.is`가 `false`면 `true`
- `hookContext` : 지금 렌더링 중인 컴포넌트를 모듈 안 변수 하나에 저장하고 꺼낸다. Hook들과 `createElement`는 이것으로 자기가 속한 컴포넌트를 찾는다. 이름 예: `setCurrentComponent`, `getCurrentComponent`

## 9. 이벤트와 컴포넌트 규칙

- Phase 2 컴포넌트 이벤트는 Virtual DOM의 이벤트 props(`onClick`, `onInput` 등)로 전달한다.
- 상태를 바꾸는 함수(`onAdd`, `onToggle` 등)는 루트 컴포넌트가 자식에게 props로 내려준다.
- 렌더링이나 갱신마다 같은 이벤트가 누적 등록되지 않도록 한다.
- 해제해야 하는 이벤트와 자원은 `useEffect`의 cleanup 또는 `unmount()`에서 정리한다.
- 입력창에 글자를 입력해 리렌더링해도 입력창 포커스가 유지되어야 한다. (DOM 노드가 교체되지 않아야 한다)

## 10. Phase 2 데이터 흐름

- 최초 : `mount()` → 루트 컴포넌트 실행(→ `useState`/`useMemo`/`useEffect`, `createElement`) → `render` → `pendingEffects` 실행
- 갱신 : 상태 변경 함수 → `update()` → 루트 컴포넌트 실행 → `diff` → `applyPatches` → `pendingEffects` 실행
- 모듈 사이에서 오가는 데이터 : VNode, Patch 목록, `hooks` 배열

## 11. Phase 2 데모

주제는 19번 미정 사항에서 정한다. 어떤 주제든 아래 조건을 만족한다.

- 입력과 클릭에 따라 화면이 부분 업데이트된다.
- 루트 컴포넌트가 State를 가지고, 세 Hook(`useState`, `useMemo`, `useEffect`)을 모두 사용한다.
- 자식 컴포넌트는 props만 받는 순수 함수이다.
- 입력 중에도 입력창 포커스가 유지된다.
- 바뀐 부분만 DOM에서 갱신된다.

## 12. 에러 처리

- 에러 메시지 형식과 던지는 기준은 Phase 1 문서 11장을 따른다.
- Hook 규칙 위반, mount 중복은 `Error`, 인자 타입 오류는 `TypeError`를 쓴다.
- `core`는 던지기만 하고, `demos`의 이벤트 핸들러에서 잡아 화면에 보여준다.

## 13. 테스트 규칙

- 각자 자기가 작성한 코드의 테스트를 직접 만들고, 코어타임 통합 전에 다른 사람 테스트도 모두 통과해야 한다.
- 테스트 도구는 지정하지 않는다. 자유롭게 선택한다.
- 테스트 파일은 `tests/core/component/`, `tests/core/hooks/` 아래에 `대상.test.js`로 둔다.

### 13.1. 필수 단위 테스트 (Phase 2)

| 대상 | 주요 검증 내용 |
|---|---|
| `createElement` | 자식 정규화(문자열·숫자·`null`·중첩 배열), 자식 컴포넌트 호출, 자식 안 Hook 에러 |
| `FunctionComponent` | 최초 렌더링, 갱신, 해제, 중복 mount 에러 |
| `useState` | 값 유지, 함수형 갱신, 상태별 독립성, 같은 값이면 리렌더링 없음 |
| `useEffect` | 의존성 비교(생략·빈 배열·값 있음), 실행 시점, cleanup 호출 순서 |
| `useMemo` | 의존성이 같으면 계산 함수 재실행 없음, 의존성 변경 시 재계산 |

### 13.2. 기능 테스트 (Phase 2)

- 입력·클릭으로 State와 화면이 갱신된다.
- 연속 조작 후에도 이벤트가 중복 실행되지 않는다.
- Effect 정리 이후 불필요한 작업이 남지 않는다.
- 입력창에 타이핑하는 동안 포커스가 유지된다.

## 14. 엣지 케이스 체크리스트 (Phase 2)

- 같은 State 값으로 상태 변경 함수 호출 (렌더링 안 함)
- `useEffect` 의존성이 비어 있는 경우 / 아예 없는 경우
- `useEffect` cleanup 호출 순서
- Hook 호출 순서가 렌더링마다 달라지는 경우 (조건문 안의 Hook)
- 자식 컴포넌트 안에서 Hook을 호출한 경우
- 루트 컴포넌트가 element가 아닌 값을 반환한 경우
- `children`에 `null`, `false`, 중첩 배열, 숫자가 섞인 경우
- 입력창에 타이핑 중 리렌더링 (포커스 유지)
- `unmount()` 이후 상태 변경 함수 호출
- 렌더링 함수 본문에서 상태 변경 함수를 호출한 경우

## 15. 구현 안내와 공유 전 확인

### 15.1. 구현 안내

`docs/implementation_notes.md`에 Phase 1 문서 13.1의 항목과 함께 다음을 기록한다.

- Hook 값이 유지되는 방식 (`hooks` 배열 구조)
- 렌더링 시점과 Batching 지원 여부
- 추가 구현 요소 (차별화 요소)

### 15.2. 공유 전 확인 목록 (Phase 2)

- [ ] Phase 1의 `render`·`diff`·`applyPatches`를 재사용하는가?
- [ ] State와 Hook을 루트 컴포넌트에서만 사용하는가?
- [ ] 자식 컴포넌트가 props만 받는 순수 함수인가?
- [ ] 이벤트 등록과 해제가 올바르게 처리되는가?
- [ ] Effect cleanup이 의존성 변경과 해제 때 호출되는가?
- [ ] 입력 중에도 포커스가 유지되는가?
- [ ] 단위 테스트와 기능 테스트를 통과했는가?
- [ ] 지원 범위와 제한을 문서화했는가?

## 16. AI 사용 지침

Phase 1 문서 14장의 AI 사용 지침과 프롬프트 템플릿을 그대로 따른다. Phase 2에서 특히 지킬 것:

- 자식 컴포넌트에 State나 Hook을 넣지 않는다.
- Phase 1의 `render`·`diff`·`applyPatches`를 복사해 따로 만들지 않는다.
- 시그니처(`FunctionComponent(RootComponent, props, container)`, `mount`/`update`/`unmount`)를 임의로 바꾸지 않는다.

구현을 시킬 때 예시:

```
docs/미니리액트_Phase2.md 를 읽고, 7.1의 useState 를 구현해줘.
- 파일 : src/core/hooks/useState.js
- 시그니처, 에러 조건, 부수 효과는 협약 그대로 지켜줘.
- 협약에 없는 이름이 필요하면 만들기 전에 먼저 알려줘.
- 구현 후 협약과 다른 점이 있는지 점검해서 알려줘.
```

## 17. 용어 정리

- Hook : `useState`, `useEffect`, `useMemo`처럼 함수형 컴포넌트에서 상태나 부수 효과를 다루는 함수
- 루트 컴포넌트 : State와 Hook을 가질 수 있는 유일한 컴포넌트
- 자식 컴포넌트 : props만 받고 VNode를 돌려주는 순수 함수
- Lifting State Up : 자식들이 쓰는 상태를 루트로 끌어올려 관리하는 방식
- deps : Hook이 "값이 바뀌었는지"를 판단할 때 쓰는 의존성 배열
- Batching : 여러 상태 변경을 모아서 렌더링을 한 번만 하는 최적화
- cleanup : Effect가 다시 실행되기 전이나 컴포넌트가 해제될 때 호출되는 정리 함수

## 18. 코어타임

- 11월 10일 9시 Phase 2 통합, 11월 11일 9시 최종 통합 (Phase 1 문서 1장 일정 참고)
- 코어타임 흐름과 완료 기준은 Phase 1 문서 15장을 따른다.

## 19. 미정 사항

| 항목 | 후보 | 결정 날짜 | 결정자 |
|---|---|---|---|
| Phase 2 데모 주제 | 할 일 목록 (AI 제안: 입력·추가·완료 토글·삭제·필터·남은 개수), 변경 가능 | 11월 8일 (제안) | 팀 전원 |
| Batching 구현 여부 | 도입 시 여러 상태 변경을 microtask로 모아 한 번만 `update()` | 11월 11일 | 팀 전원 |
| 차별화 추가 구현 요소 | 1) Batching 2) `key` 기반 자식 비교 3) Patch 로그 시각화 패널 4) `useRef`, `useReducer` 추가 5) Diff 성능 비교 화면 | 11월 11일 | 팀 전원 |

## 20. 변경 이력

| 날짜 | 변경 내용 | 작성자 |
|---|---|---|
| 2026-11-07 | 팀원 3명의 문서를 취합해 Phase 2 문서 초안 작성 | |
