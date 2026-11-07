# 미니 리액트 협약 — 공통 규칙 + Phase 1

> 이 문서는 팀원 모두와 AI가 같은 기준으로 코드를 짜기 위한 약속이다. 여기에 적힌 이름과 형태가 코드보다 우선한다. 바꾸고 싶으면 코드를 먼저 고치지 말고 이 문서를 먼저 고친다.
> 이 문서는 **공통 규칙(1~8, 10~16장)** 과 **Phase 1 전용 내용(7~9장)** 을 담는다. Phase 2 전용 내용은 `미니리액트_Phase2.md`에 있고, 그 문서도 이 문서의 공통 규칙을 그대로 따른다.

## 0. 문서 사용법

- 대상 독자 : 팀원, AI
- 우선순위 : 이 문서와 Phase 2 문서 > 개별 코드
- 변경 방법 : 팀원 전원 합의 후 문서를 먼저 수정하고, 18번 변경 이력에 기록한다
- 상태 : 팀원 3명이 각자 만든 문서를 취합한 초안 (팀 검토 필요)
- 이 문서의 위치 : `docs/미니리액트_Phase1.md`
- AI에게 일을 시킬 때는 이 문서의 해당 장 번호를 가리킨다 (예: "8장의 diff만 구현해")

### 적용 수준

**가독성과 수정 용이성을 우선하며, 모듈의 경계와 연결 방식은 통일한다. 알고리즘·내부 자료구조·보조 함수는 각자 선택한다.**

- **필수:** 모든 구현이 따라야 한다.
- **권장:** 기본적으로 따르되, 다른 선택의 이유를 설명할 수 있다.
- **자율:** 공통 인터페이스를 지키는 범위에서 각자 선택한다.

별도 표시가 없는 규칙은 필수다.

## 1. 프로젝트 개요와 일정

- 프로젝트 이름 : 미니 리액트 구현
- 한 줄 목표 : 동작하는 미니 리액트를 두 단계로 직접 구현한다
    - Phase 1 : Virtual DOM 변환, Diff, Patch, 상태 이력
    - Phase 2 : 함수형 컴포넌트, State, Hooks (`useState`, `useEffect`, `useMemo`)
- 최종 결과물 : 사용자의 입력과 클릭에 따라 부분 업데이트되는 동작하는 웹 페이지
- 일정 (제안, 모든 시간은 저녁) :

| 날짜 | 개인 작업 | 코어타임 |
|---|---|---|
| 11월 8일 | 이 협약 합의 | 없음 |
| 11월 9일 | 각자 Phase 1 완성 | 7시~9시 사이 Phase 1 통합 |
| 11월 10일 | 각자 Phase 2 완성 | 9시 Phase 2 통합 |
| 11월 11일 | 차별점 구상과 구현 | 9시 최종 통합 |

- 팀의 승부처 : Phase 1, 2는 모든 팀이 같으므로 차별 구현이 승부처다 (후보는 17번 미정 사항)

## 2. 기술 기준

| 항목 | 규칙 |
|---|---|
| 구현 언어 | HTML, CSS, JavaScript (TypeScript는 쓰지 않는다) |
| JavaScript 모듈 | ES Modules (`file://`로 열면 module 로딩이 막히므로 로컬 정적 서버로 실행한다) |
| UI 작성 | DOM API와 직접 구현한 Virtual DOM 사용 |
| 컴포넌트 | 함수형 컴포넌트 |
| 컴포넌트 실행 관리 | 직접 작성한 `FunctionComponent` 클래스 |
| JSX | 사용하지 않음 |
| 외부 UI 프레임워크 | 사용하지 않음 (React, Vue 등) |
| 개발·테스트 도구 | 지정하지 않는다. 각자 자유롭게 선택한다 |

- Virtual DOM·Diff·Patch·Hooks의 핵심 구현을 외부 라이브러리로 대체하지 않는다.
- 공개 함수와 데이터 형식은 JSDoc으로 설명한다.
- 포맷터 등 도구를 쓸 때는 3.2의 코드 형식에 맞게 설정한다.

## 3. 코딩 컨벤션

### 3.1. 이름 규칙

| 대상 | 규칙 | 예시 |
|---|---|---|
| 폴더 | 소문자 `snake_case` | `core`, `phase_one` |
| 파일 | 내보내는 함수·클래스 이름과 같게 | `createElement.js`, `FunctionComponent.js` |
| 진입·설정·여러 개를 내보내는 파일 | 소문자 `camelCase` | `main.js`, `app.js`, `sample.js`, `index.js` |
| HTML·CSS 파일 | 소문자 | `index.html`, `base.css` |
| 함수·메서드 | `camelCase` | `createElement`, `applyPatches` |
| 변수·매개변수 | `camelCase` | `previousVNode`, `nextState` |
| 클래스 | `PascalCase` | `FunctionComponent`, `StateHistory` |
| 고정 설정·상수 묶음 | `UPPER_SNAKE_CASE` | `PATCH_TYPES`, `NODE_TYPES` |
| 참·거짓 값 | `is`, `has`, `can`으로 시작 | `isMounted`, `hasChanges` |
| 테스트 파일 | `대상.test.js` | `diff.test.js` |
| CSS 클래스 | `kebab-case` | `history-controls` |
| HTML id | 소문자 `kebab-case` | `real-area`, `patch-button` |
| DOM 식별용 속성 | `data-*` | `data-action="patch"` |

### 3.2. 코드 형식

- 들여쓰기: 공백 2칸
- 문자열: 작은따옴표
- 문자열 보간: 백틱
- 세미콜론: 사용
- 줄 길이: 100자
- 마지막 쉼표: 허용되는 위치에 사용
- 조건문·반복문: 한 줄이어도 중괄호 사용
- 파일 마지막: 줄바꿈 1개

### 3.3. 변수 이름

- 무엇이 들어 있는지 드러나는 명사로 짓는다.
- 같은 개념을 `vnode`, `virtualNode`, `vdomNode`처럼 섞어 쓰지 않는다. 아래 용어를 일관되게 쓴다.

| 개념 | 사용할 이름 |
|---|---|
| 실제 DOM 노드 | `domNode` |
| Virtual DOM 노드 | `vnode` |
| 이전·다음 Virtual DOM | `previousVNode`, `nextVNode` |
| DOM 컨테이너 | `container` |
| 변경 목록 | `patches` |
| 컴포넌트 입력 | `props` |
| 자식 노드 목록 | `children` |
| Hook 저장 위치 | `hookIndex` |
| Effect 정리 함수 | `cleanup` |

- 줄임말은 누구나 아는 것만 쓴다. 허용 목록: `id`, `db`, `url`, `max`, `min`, `props`, `deps`, `DOM`, `VNode`, `HTML`. 그 밖의 줄임말(`cnt`, `tmp`, `res` 등)은 쓰지 않는다.
- 한 글자 이름은 짧은 반복문의 `i`, `j`만 허용한다. 그 밖의 한 글자·숫자 이름과 한글 이름은 쓰지 않는다.
- 단위가 있으면 이름에 넣는다. 예: `timeoutMs`, `sizeBytes`
- 여러 개면 복수형, 개수면 `count`를 붙인다. 예: `nodes`, `nodeCount`
- 참·거짓 값은 조건을 나타내는 이름으로 짓는다. 예: `isDone`, `hasChildren`
- 찾는 표는 무엇으로 찾는지 적는다. 예: `userById`
- 숫자·문자열을 바로 쓰지 않고 상수로 이름을 붙인다. 예: `'INSERT'` 대신 `PATCH_TYPES.INSERT`

### 3.4. 함수 이름과 설계

- 이름은 "동사 + 대상"으로 짓는다. 예: `createNode`, `parseCommand`, `applyPatches`
- 참·거짓을 돌려주는 함수는 `is`, `has`, `can`으로 시작한다. 예: `isDepsChanged`
- 같은 뜻에는 같은 동사를 쓴다.

| 동사 | 뜻 |
|---|---|
| `get` | 반드시 있는 값을 가져온다 (없으면 에러) |
| `find` | 없을 수도 있다 (없으면 `null`) |
| `create` / `remove` | 만든다 / 없앤다 |
| `update` | 이미 있는 것을 고친다 |
| `parse` / `to…` | 다른 형식으로 바꾼다 |

- 함수 하나는 한 가지 일만 한다. 이름에 "그리고"가 들어가야 설명되면 나눈다.
- 한 함수는 30줄을 넘기지 않고, 중첩은 3단계까지 쓴다. 넘으면 함수를 쪼갠다.
- 인자는 3개 이하로 한다. 넘으면 객체 하나로 묶는다.
- 받은 값을 함부로 고치지 않는다. 인자를 고쳐야 한다면 함수 이름이나 주석에 그 사실을 적는다.
- 매개변수를 알 수 없는 순서로 변경하거나 전역 변수에 결과를 저장하지 않는다.

### 3.5. JavaScript 작성 규칙

- 기본 선언은 `const`, 재할당이 필요한 경우에만 `let`을 쓴다. `var`는 쓰지 않는다.
- 한 줄에 변수 하나만 선언하고, 처음 쓰는 곳 가까이에서 선언한다.
- 동등 비교는 `===`, `!==`를 쓴다. `==`는 쓰지 않는다.
- 상태와 의존성 값의 동일성 비교에는 `Object.is`를 쓴다.
- 비동기 작업이 없는 함수에 `async`를 붙이지 않는다.
- 선택적 값 처리와 오류 처리를 구분한다. 필수 값이 없는 문제를 선택적 체이닝으로 숨기지 않는다.
- import 경로에는 `.js` 확장자를 붙인다.
- named export만 쓴다. (default export 금지)
- 다른 폴더의 모듈은 그 폴더의 `index.js`를 통해서 불러온다.
- 순환 import를 만들지 않는다.

**권장**

- 주요 함수는 함수 선언문을 쓴다.
- 짧은 콜백은 화살표 함수를 쓴다.
- 중첩 삼항 연산자를 쓰지 않는다.
- 복잡한 조건식에는 의미 있는 이름을 붙인다.

### 3.6. 주석

- 밖에서 쓰는(공개) 함수 위에는 JSDoc `/** … */`을 단다. 입력, 반환값, 실패할 때의 동작을 적는다.

```js
/**
 * id로 사용자를 찾는다.
 * @param {number} userId 찾을 사용자 id
 * @returns {User | null} 찾은 사용자 (없으면 null)
 */
function findUser(userId) { … }
```

- 함수 안 주석은 "어떻게"가 아니라 "무엇을, 왜"를 쓴다. 코드만 봐서는 알 수 없는 이유, 조건, 주의할 점만 적는다.
- 코드를 그대로 다시 말하는 주석은 쓰지 않는다. 예: `i++; // i를 1 늘린다` ✗
- 주석은 한국어로 쓰고, `//` 뒤에 한 칸 띄운다.
- 할 일은 `TODO(이름): 내용` 형식으로 남긴다. 예: `// TODO(HeoJunekang): 빈 배열 처리`
- 지운 코드를 주석으로 남겨 두지 않는다.

## 4. 디렉터리 구조

```text
mini_react/
├── index.html
├── README.md
├── CLAUDE.md                  (Claude용 진입 파일)
├── GEMINI.md                  (Gemini용 진입 파일)
├── AGENTS.md                  (GPT/Codex용 진입 파일)
├── docs/
│   ├── 미니리액트_Phase1.md     (이 문서)
│   ├── 미니리액트_Phase2.md
│   └── implementation_notes.md
├── src/
│   ├── main.js
│   ├── core/
│   │   ├── index.js
│   │   ├── vdom/
│   │   │   ├── nodeTypes.js
│   │   │   ├── domToVNode.js
│   │   │   └── render.js
│   │   ├── diff/
│   │   │   ├── diff.js
│   │   │   └── patchTypes.js
│   │   ├── patch/
│   │   │   └── applyPatches.js
│   │   ├── component/
│   │   │   ├── createElement.js
│   │   │   ├── FunctionComponent.js
│   │   │   └── hookContext.js
│   │   └── hooks/
│   │       ├── useState.js
│   │       ├── useEffect.js
│   │       └── useMemo.js
│   ├── demos/
│   │   ├── phase_one/
│   │   │   ├── app.js
│   │   │   ├── StateHistory.js
│   │   │   ├── sample.js
│   │   │   └── styles.css
│   │   └── phase_two/
│   │       ├── app.js
│   │       ├── components/
│   │       └── styles.css
│   └── styles/
│       └── base.css
└── tests/
    ├── core/
    │   ├── vdom/
    │   ├── diff/
    │   ├── patch/
    │   ├── component/
    │   └── hooks/
    └── demos/
        ├── phase_one/
        └── phase_two/
```

- 테스트·개발 도구를 쓰면 그 설정 파일은 저장소 루트에 둔다. 도구는 지정하지 않는다.

### 배치 규칙

- `core` : 데모의 주제에 의존하지 않는 미니 리액트 구현
- `demos` : 각 Phase의 화면과 애플리케이션 고유 처리
- `tests` : `src`와 같은 모듈 구조로 테스트 배치
- `docs` : 협약, 설계 판단, 제약, 추가 기능 설명
- `main.js` : 페이지 전체 초기화와 데모 시작
- 보조 파일은 해당 모듈 안에 추가할 수 있다. 주요 파일의 이름·위치와 모듈의 책임 변경은 합의 후 반영한다.
- AI는 트리에 있는 주요 파일만 만들고, 트리에 없는 주요 파일·폴더를 임의로 만들지 않는다.
- 내용이 없는 공통 폴더나 예상만으로 만든 보조 파일은 추가하지 않는다.
- 새로 만들거나 이름을 바꾼 주요 파일은 위 트리에 반영한다.

## 5. 모듈 책임과 의존 관계

| 모듈 | 책임 |
|---|---|
| `vdom` | DOM을 Virtual DOM으로 읽기, Virtual DOM을 새 DOM 노드로 만들기 |
| `diff` | 두 Virtual DOM을 비교해 변경 목록 생성 |
| `patch` | 변경 목록을 실제 DOM에 적용 |
| `component` | Virtual DOM 생성(`createElement`), 루트 컴포넌트의 실행·갱신·Hook 실행 문맥 관리 |
| `hooks` | 상태, Effect, 메모이제이션 관리 |
| `phase_one/StateHistory` | Virtual DOM 이력과 현재 위치 관리 |
| `demos` | 이벤트 연결, 화면 구성, 기능 사용 |

### 필수 경계

- `diff`는 실제 DOM을 읽거나 수정하지 않는다.
- `patch`는 Hook 상태나 History를 관리하지 않는다.
- `core`는 `demos`를 import하지 않는다.
- `StateHistory`는 실제 DOM을 직접 수정하지 않는다.
- DOM을 변경하는 코드는 최초 렌더링(`render`)과 Patch 모듈(`applyPatches`)에 모은다.
- 데모는 `core/index.js`에 공개한 기능을 사용한다.
- 의존 방향은 `demos → core/hooks → core/component → core/vdom·diff·patch` 이다. 반대 방향 import는 금지한다.
- 순환 import를 만들지 않는다.

Phase 2는 Phase 1의 Virtual DOM·Diff·Patch를 재사용한다. Phase 2 전용으로 같은 기능을 중복 구현하지 않는다.

## 6. Virtual DOM 공통 형식

### 6.1. 노드 구분

요소 노드와 텍스트 노드를 명시적으로 구분한다. `type` 값은 `NODE_TYPES`(`'element'`, `'text'`) 상수로 쓴다.

요소 노드:

```javascript
{
  type: 'element',
  tag: 'button',
  props: {
    class: 'counter-button',
    onClick: handleClick,
  },
  children: [
    {
      type: 'text',
      text: '증가',
    },
  ],
}
```

텍스트 노드:

```javascript
{
  type: 'text',
  text: '안녕하세요',
}
```

### 6.2. 데이터 규칙

- HTML 태그 이름은 소문자로 저장한다.
- 요소 노드는 항상 `props` 객체와 `children` 배열을 가진다.
- 텍스트 노드의 `text`는 문자열이다.
- Virtual DOM에 실제 DOM 노드 참조를 넣지 않는다.
- VNode는 만든 뒤 수정하지 않는다. 바꾸고 싶으면 새로 만든다.
- Diff는 전달받은 Virtual DOM을 변경하지 않는다.
- History에 저장한 Virtual DOM을 이후 처리에서 변경하지 않는다.
- Phase 2에서 생성하는 노드도 같은 형식을 사용한다.
- 주석 노드와 공백만 있는 텍스트 노드는 VNode로 만들지 않는다.
- Virtual DOM 전체를 JSON 변환으로 복제하지 않는다. 이벤트 함수처럼 JSON으로 보존되지 않는 값이 있다.

### 6.3. 속성 규칙

- HTML 속성은 `class`, `id`, `title`처럼 실제 속성 이름을 쓴다.
- `props` 값은 문자열, 불리언, 함수(이벤트 핸들러)만 허용한다.
- 이벤트는 `onClick`, `onInput`처럼 `on` 접두사를 쓰고, 값은 함수로 전달한다. `on`을 떼고 소문자로 바꾼 것이 DOM 이벤트 이름이다. (`onClick` → `click`)
- 불리언 `true`는 속성이 존재하는 것, `false`·`null`·`undefined`는 속성이 없는 것으로 본다.
- `style`의 기본 지원 형식은 문자열이다.
- `value`, `checked`처럼 현재 상태가 DOM 프로퍼티에 저장되는 항목은 프로퍼티 기준의 동작을 함께 처리한다.
- 속성 제거와 값 변경을 구분한다.
- 추가 속성 형식이나 기능을 도입하면 지원 범위와 테스트를 함께 추가한다.

## 7. 공개 함수 규칙

다음 이름과 역할을 공통으로 사용한다. (Phase 2 함수의 자세한 계약은 Phase 2 문서)

| 함수 | 역할 |
|---|---|
| `createElement(tag, props, ...children)` | 요소 Virtual DOM 생성 |
| `domToVNode(domNode)` | 실제 DOM을 Virtual DOM으로 변환 |
| `render(vnode)` | Virtual DOM을 새 DOM 노드로 변환 |
| `diff(previousVNode, nextVNode)` | 변경 목록 반환 |
| `applyPatches(rootNode, patches)` | 실제 DOM에 변경 반영 후 현재 루트 반환 |
| `useState(initialValue)` | 상태와 상태 변경 함수 반환 |
| `useEffect(effect, dependencies)` | DOM 반영 이후 실행할 작업 등록 |
| `useMemo(factory, dependencies)` | 의존성에 따라 계산 결과 재사용 |

### 함수별 경계

- `render`는 DOM 노드를 반환하고, 전달받지 않은 전역 컨테이너에 자동 삽입하지 않는다.
- `diff`는 비교와 변경 목록 생성만 수행한다.
- `applyPatches`는 루트 교체 가능성을 고려해 현재 루트 DOM을 반환한다.
- 주요 함수의 JSDoc에는 입력, 반환값, 오류 조건을 기록한다.
- 모듈 내부의 보조 함수 이름과 분리 방식은 자율로 정한다.

### 7.1. `domToVNode`

- 시그니처 : `domToVNode(domNode: Node): VNode | null`
- 반환값 : VNode. 주석 노드와 공백만 있는 텍스트 노드는 `null`
- 동작 :
    - Element면 `tag`(소문자), `props`(attributes 전부), `children`(자식을 재귀 변환, `null`은 제외)을 담는다.
    - Text면 `{ type: 'text', text }`를 만든다. 텍스트 앞뒤 공백은 그대로 둔다.
    - `script`, `style` 태그는 `null`로 처리한다.
- 에러 : `domNode`가 Node가 아니면 `TypeError` — `[domToVNode] Node가 필요합니다`
- 부수 효과 : 없음 (DOM을 읽기만 한다)

### 7.2. `render`

- 시그니처 : `render(vnode: VNode): Node`
- 반환값 : 새로 만든 DOM 노드 (document에 붙이지 않은 상태)
- 동작 : text면 `document.createTextNode`, element면 `document.createElement(tag)` 후 속성을 넣고 자식을 재귀로 만들어 붙인다.
- 에러 : `vnode.type`이 알 수 없는 값이면 `TypeError` — `[render] 알 수 없는 VNode 타입입니다`
- 부수 효과 : 없음 (document에 붙이는 일은 호출한 쪽이 한다)

### 7.3. `diff`

8장 참고.

### 7.4. `applyPatches`

8장 참고.

## 8. Diff와 Patch 규칙

### 8.1. 공통 변경 종류

다음 다섯 종류를 구분한다. 값은 `PATCH_TYPES` 상수로 쓴다.

| 변경 종류 | 의미 |
|---|---|
| `INSERT` | 노드 추가 |
| `REMOVE` | 노드 삭제 |
| `REPLACE` | 노드 종류나 태그 변경에 따른 교체 |
| `UPDATE_TEXT` | 텍스트 변경 |
| `UPDATE_PROPS` | 속성과 이벤트 변경 |

### 8.2. Patch 객체 구조 (필수)

모든 Patch는 `type`과 `path`를 가진다. `path`는 루트에서 해당 노드까지 `children` 인덱스를 이어 놓은 배열이고, 루트 자신은 `[]`이다. Diff와 Patch 구현은 아래 형식을 그대로 사용한다.

| 종류 | 형식 |
|---|---|
| `INSERT` | `{ type: 'INSERT', path: 부모 경로, index: number, node: VNode }` |
| `REMOVE` | `{ type: 'REMOVE', path: 삭제할 노드 경로 }` |
| `REPLACE` | `{ type: 'REPLACE', path: 교체할 노드 경로, node: VNode }` |
| `UPDATE_TEXT` | `{ type: 'UPDATE_TEXT', path: 텍스트 노드 경로, text: string }` |
| `UPDATE_PROPS` | `{ type: 'UPDATE_PROPS', path: 노드 경로, set: object, remove: string[] }` |

예시 : `{ type: 'UPDATE_PROPS', path: [0, 1], set: { class: 'on' }, remove: ['id'] }`

- 한 부모 안에서 `INSERT`는 `index` 오름차순, `REMOVE`는 인덱스가 큰 것부터 만든다. (앞의 인덱스가 밀리지 않게 하기 위해)
- `applyPatches`는 `patches`를 앞에서부터 순서대로 하나씩 적용한다.

### 8.3. `diff`

- 시그니처 : `diff(previousVNode: VNode, nextVNode: VNode): Patch[]`
- 반환값 : Patch 배열. 바뀐 게 없으면 빈 배열
- 입력 VNode를 수정하지 않는다.
- 에러 : 둘 중 하나라도 VNode 형태가 아니면 `TypeError` — `[diff] VNode가 필요합니다`
- 동작 : 루트부터 내려가며 노드 하나씩 비교한다.
    - 타입이나 `tag`가 다르면 `REPLACE` 하나를 만들고 그 아래는 더 보지 않는다. (element ↔ text 변경 포함)
    - 둘 다 text면 `text`가 다를 때 `UPDATE_TEXT`를 만든다.
    - 둘 다 같은 `tag`의 element면 props를 비교해 바뀐 게 있을 때 `UPDATE_PROPS`를 만들고, 자식을 비교한다.
    - 새 값에 있고 이전과 다르면 `set`에, 이전에만 있는 키는 `remove`에 넣는다. 함수 값은 참조가 다르면 다른 값으로 본다.
- 자식 비교 방식 (필수) : 같은 인덱스끼리 비교한다.
    - 공통 길이까지는 같은 인덱스끼리 재귀 비교한다.
    - 다음 쪽이 더 길면 남는 만큼 `INSERT` (index 오름차순)
    - 이전 쪽이 더 길면 남는 만큼 `REMOVE` (index 내림차순)
    - `key` 기반 재정렬은 확장 기능이다. 지원하면 범위를 문서화한다.
- 내부 보조 함수(예: `diffNode`, `diffChildren`, `diffProps`)의 이름과 분리 방식은 자율이다. 단, 순환 import는 만들지 않는다.

### 8.4. `applyPatches`

- 시그니처 : `applyPatches(rootNode: Node, patches: Patch[]): Node`
- 매개변수 : `rootNode` (Patch의 `path` 기준이 되는 루트 DOM 노드. 부모를 가진 상태여야 한다), `patches`
- 반환값 : 적용 후의 현재 루트 DOM 노드 (루트가 교체되면 새 노드)
- 동작 :
    - `INSERT` : `render`로 만든 노드를 부모의 `index` 위치에 끼워 넣는다.
    - `REMOVE` : 해당 노드를 지운다.
    - `REPLACE` : `render`로 만든 노드로 `replaceWith`한다.
    - `UPDATE_TEXT` : `textContent`를 바꾼다.
    - `UPDATE_PROPS` : 속성을 설정·제거한다.
- 속성 적용 :
    - 키가 `on`으로 시작하는 함수면 이전 핸들러를 `removeEventListener`로 빼고 새 핸들러를 `addEventListener`로 단다. 이전 핸들러를 기억하는 방식은 자율이다. (권장: DOM에 `handlers` 객체로 저장)
    - `true`면 `setAttribute(key, '')`, 문자열이면 `setAttribute(key, value)`
    - `value`, `checked`는 속성과 함께 DOM 프로퍼티에도 직접 넣는다. (입력창의 현재 값이 어긋나지 않게 하기 위해)
    - `remove`의 키는 `removeAttribute`로 지우고, 이벤트면 리스너를 뺀다.
- 경로 찾기 : `rootNode`의 `childNodes`를 `path` 인덱스대로 따라간다. 단, `domToVNode`가 무시한 노드(주석, 공백 텍스트, `script`, `style`)는 건너뛰고 센다. (VNode의 인덱스와 맞추기 위해)
- 에러 : 알 수 없는 `patch.type`이면 `TypeError`, 경로를 찾을 수 없으면 `RangeError` — `[applyPatches] 경로를 찾을 수 없습니다`
- 부수 효과 : 실제 DOM 변경. 바뀌지 않은 노드는 건드리지 않는다. (DOM 노드 동일성이 유지되어야 한다)

### 8.5. 동작 기준

- 변경이 없으면 빈 변경 목록을 반환하고 DOM을 건드리지 않는다.
- 변경되지 않은 DOM 노드는 가능한 한 유지한다.
- 이벤트 변경 시 이전 이벤트가 중복 등록되지 않도록 한다.
- 여러 형제 노드를 추가·삭제할 때 대상이 밀려 잘못 적용되지 않도록 한다.
- 루트 노드 교체를 처리한다.
- 일반적인 업데이트에서 컨테이너 전체를 비우거나 `innerHTML`로 다시 생성하지 않는다.

### 구현 자율 범위

- 트리 순회 방식 (재귀·반복)
- 내부 자료구조
- 변경 적용 시 보조 함수 분리 방식
- 성능 개선

## 9. Phase 1 화면과 History 규칙

### 9.1. 화면 구성

다음 요소를 제공한다.

| 요소 | id |
|---|---|
| 실제 영역 (Patch가 적용되는 곳) | `real-area` |
| 수정 가능한 테스트 영역 | `test-area` |
| Patch 버튼 | `patch-button` |
| 뒤로가기 버튼 | `back-button` |
| 앞으로가기 버튼 | `forward-button` |
| 현재 이력 위치 표시 | `history-position` (권장 id) |
| 에러 표시 영역 | `error-message` |

- 버튼과 상태 표시는 비교 대상 DOM 바깥에 둔다.
- 테스트 영역의 편집 방식은 자율이다. (예: `contenteditable`) 지원하는 편집 범위를 화면에 안내한다.

### 9.2. StateHistory

- 설명 : Patch할 때마다 저장하는 VNode 기록. 뒤로가기·앞으로가기에 사용한다.
- 시그니처 : `new StateHistory(initialVNode: VNode)`
- 메서드 :
    - `push(vnode: VNode): void` : 현재 위치 뒤의 기록을 버리고 새 기록을 붙이고 위치를 옮긴다.
    - `back(): VNode | null` : 한 칸 뒤로 가서 그 VNode를 반환. 더 못 가면 `null`
    - `forward(): VNode | null` : 한 칸 앞으로. 더 못 가면 `null`
    - `getCurrent(): VNode` : 현재 VNode
    - `canGoBack(): boolean`, `canGoForward(): boolean` : 버튼 활성화 판단용
- 기록 개수에는 제한을 두지 않는다.
- History에 DOM 노드 자체를 저장하지 않는다.
- 에러 : 생성자에 VNode가 아닌 값이 오면 `TypeError` — `[StateHistory] VNode가 필요합니다`
- 부수 효과 : 없음

### 9.3. 초기화

1. 실제 영역의 샘플 DOM을 읽는다. (`domToVNode`)
2. Virtual DOM을 만든다.
3. 같은 Virtual DOM으로 테스트 영역을 렌더링한다. (`render`)
4. 초기 상태를 History에 저장한다.

비교 대상 루트와 화면을 담는 컨테이너를 구분한다.

### 9.4. Patch와 이력 이동

- Patch 시 테스트 영역의 현재 DOM을 다시 읽는다. (`domToVNode`)
- 현재 이력의 Virtual DOM과 비교한다. (`diff`)
- 실제 영역에는 변경 사항만 반영하고, 반환된 현재 루트를 이후 처리에 사용한다. (`applyPatches`)
- 반영이 성공하면 새 Virtual DOM을 History에 저장한다.
- 변경이 없으면 새 이력을 추가하지 않는다.
- 뒤로가기·앞으로가기는 이동할 VNode와 현재 VNode를 `diff`해서 실제 영역에 적용하고, 테스트 영역도 그 VNode로 다시 그린다.
- 이력 이동 시 테스트 영역에만 있던 미반영 수정은 선택한 이력으로 대체한다.
- 과거 이력에서 새 변경을 반영하면 이후 이력을 제거한다.
- 이동할 이력이 없으면 해당 버튼을 비활성화한다. (`canGoBack`, `canGoForward`가 `false`)
- 에러는 버튼 핸들러에서 `try/catch`로 잡아 `#error-message`에 보여주고 `console.error`로도 남긴다.

### 9.5. Phase 1 데이터 흐름

실제 영역 HTML → `domToVNode` → 테스트 영역 렌더링(`render`) → 사용자가 수정 → Patch 버튼 → 새 VNode 생성(`domToVNode`) → 이전 VNode와 `diff` → 변경 부분만 실제 영역에 반영(`applyPatches`) → `StateHistory.push`

## 10. HTML·CSS·이벤트 규칙

### 10.1. HTML

- 동작 버튼은 `<button>`을 쓴다.
- 폼 제출 목적이 아닌 버튼에는 `type="button"`을 지정한다.
- 입력 요소에는 연결된 `<label>`을 제공한다.
- CSS 선택용 클래스와 JavaScript 동작 연결용 속성을 구분한다.
- JavaScript 동작 연결에는 `data-action`, `data-role` 등을 쓴다.
- HTML 문자열에 이벤트 코드를 작성하지 않는다.

### 10.2. CSS

- 공통 스타일은 `src/styles/base.css`에 둔다.
- 데모 전용 스타일은 해당 데모의 `styles.css`에 둔다.
- Phase별 최상위 클래스 아래로 스타일 범위를 제한한다.
- 기본적으로 클래스 선택자를 쓴다.
- 특별한 이유 없이 `!important`를 쓰지 않는다.
- 상태에 따른 클래스 변경이 필요하면 클래스 이름을 명시적으로 관리한다.

### 10.3. 이벤트

- 일반 화면 이벤트는 `addEventListener`로 연결한다.
- Phase 2 컴포넌트 이벤트는 Virtual DOM의 이벤트 props로 전달한다.
- 렌더링이나 갱신마다 같은 이벤트가 누적 등록되지 않도록 한다.
- 해제해야 하는 이벤트와 자원은 cleanup 또는 화면 종료 처리에서 정리한다.

## 11. 에러 처리 규칙

- 잘못된 입력이나 규칙 위반을 발견하면 즉시 예외를 던진다. 조용히 무시하지 않는다. (빈 `catch` 금지)
- "바뀐 게 없음"(빈 Patch, 같은 State 값)은 에러가 아니다.
- 없을 수도 있는 값을 찾는 `find…` 함수는 에러 대신 `null`을 돌려준다. 반드시 있어야 하는 `get…`은 없으면 에러다.
- 에러 메시지 형식 : `[함수이름] 한글 설명` (예: `[diff] VNode가 필요합니다`)
    - `TypeError` : 인자 타입이 틀렸을 때
    - `RangeError` : path, index가 범위 밖일 때
    - `Error` : Hook 규칙 위반, mount 중복 등 그 밖의 경우
- 입력 검증은 공개 함수의 맨 앞(가드)에서만 한다. 내부 함수는 검증된 입력을 믿는다.
- `core`의 코드는 던지기만 한다. `demos`의 버튼과 이벤트 핸들러에서 `try/catch`로 잡고 화면에 보여준다.

## 12. 테스트 규칙

- 각자 자기가 작성한 코드의 테스트를 직접 만든다.
- 코어타임 통합 전에 자기 테스트뿐 아니라 다른 사람 테스트도 모두 통과해야 한다. 통과하지 않는 곳이 규칙이 어긋난 곳이다.
- 테스트 도구와 실행 방법은 지정하지 않는다. 자유롭게 선택한다.
- 테스트 파일은 `src`와 같은 구조로 `tests` 아래에 `대상.test.js`로 둔다.

### 12.1. 필수 단위 테스트 (Phase 1)

| 대상 | 주요 검증 내용 |
|---|---|
| DOM 변환 | 요소·텍스트·속성·중첩 구조, 주석·공백 텍스트 무시 |
| `render` | `domToVNode`와 왕복 변환 시 같은 구조 |
| `diff` | 추가·삭제·교체·텍스트 변경·속성 변경 각 1개 이상, 바뀐 게 없으면 빈 배열 |
| `applyPatches` | Patch 5종류 각각의 적용 결과, 루트 교체, 연속된 형제 변경 |
| 이벤트 | 추가·교체·제거, 중복 실행 방지 |
| `StateHistory` | 이동, 경계, 분기 후 이후 이력 제거 |

변경되지 않은 DOM 노드의 동일성이 유지되는지도 확인한다.

### 12.2. 기능 테스트 (Phase 1)

- 테스트 영역 수정 후 실제 영역에 변경이 반영된다.
- 뒤로가기·앞으로가기 시 두 영역이 함께 바뀐다.
- 연속 조작 후에도 이벤트가 중복 실행되지 않는다.

### 12.3. 엣지 케이스 체크리스트 (Phase 1)

- 빈 노드 (자식이 없는 element)
- 빈 문자열 텍스트, 공백만 있는 텍스트 (무시)
- 주석 노드 (무시)
- 텍스트 노드만 있는 트리
- 루트 태그가 바뀌는 경우
- 태그 종류 변경 (div → span)
- element ↔ text 서로 바뀜
- 자식 추가 (맨 뒤), 자식 삭제 (맨 뒤)
- 중간 자식 삭제 (인덱스 비교라 뒤 자식들이 전부 변경으로 보임)
- 깊게 중첩된 노드의 변경
- 같은 부모에서 여러 자식이 동시에 바뀜
- 속성 추가, 변경, 삭제를 한 번에
- `class`, `style`, `value`, `checked`, `disabled` 같은 특수 속성
- 이벤트 핸들러 교체 (이전 리스너가 남으면 안 됨)
- 바뀐 게 전혀 없음 (빈 Patch, DOM을 건드리지 않음)
- 뒤로가기 / 앞으로가기가 끝에 닿았을 때
- 뒤로가기 후 새 Patch (앞으로가기 기록 삭제)

## 13. 구현 안내와 공유 전 확인

### 13.1. 구현 안내

`docs/implementation_notes.md`에 다음을 기록한다.

- Diff와 Patch의 동작 방식
- Hook 값이 유지되는 방식
- 지원하는 DOM·속성·이벤트 범위
- 렌더링 시점과 Batching 지원 여부
- 알려진 제한
- 추가 구현 요소
- 핵심 로직을 설명할 때 참고할 파일

### 13.2. 공유 전 확인 목록 (Phase 1)

- [ ] 공통 명명 규칙과 포맷을 적용했는가?
- [ ] 주요 파일과 공개 함수의 이름을 지켰는가?
- [ ] `core`가 데모에 의존하지 않는가?
- [ ] `diff`가 입력 트리를 변경하지 않는가?
- [ ] 변경되지 않은 DOM을 불필요하게 교체하지 않는가?
- [ ] 이벤트 등록과 해제가 올바르게 처리되는가?
- [ ] History에 저장된 상태가 이후 변경되지 않는가?
- [ ] 단위 테스트와 기능 테스트를 통과했는가?
- [ ] 지원 범위와 제한을 문서화했는가?

## 14. AI 사용 지침

- AI에게 항상 먼저 읽힐 파일 : 이 문서와 `미니리액트_Phase2.md`
    - Claude는 `CLAUDE.md`, Gemini는 `GEMINI.md`, GPT(Codex 등)는 `AGENTS.md`가 자동 진입 파일이다.
    - 파일을 못 읽는 웹 채팅이면 이 문서 전체를 대화 맨 앞에 붙여 넣는다.
- AI가 해도 되는 것 :
    - 이 문서에 시그니처가 있는 함수의 본문 구현
    - 4장 트리에 있는 파일 작성
    - 12장 테스트 항목에 해당하는 테스트 작성
    - 가독성과 세분화를 위한 리팩터링 제안 (적용은 사람이 승인한 뒤에)
    - 모호한 부분에 대한 질문
- AI가 하면 안 되는 것 :
    - 시그니처 임의 변경 (이름, 매개변수, 반환값)
    - 4장 트리에 없는 주요 파일·폴더 생성
    - 외부 UI 프레임워크·라이브러리로 핵심 구현을 대체
    - 이 문서를 임의로 수정하거나 문서와 다른 이름을 새로 만드는 것
    - 3장 네이밍 규칙 위반 (대문자 시작 함수, 한 글자 변수(`i`, `j` 제외), 한글 이름)
    - 자식 컴포넌트에 State나 Hook을 넣는 것
    - 요청하지 않은 범위의 코드를 함께 고치는 것 (한 번에 한 모듈, 가능하면 한 함수)
    - 모르는 것을 추측해서 구현하는 것 (협약에 없으면 먼저 질문)
    - 테스트를 통과시키려고 테스트를 약하게 고치는 것
- 협약과 다른 코드를 만들었을 때 :
    - 코드를 협약에 맞게 고치는 것이 기본이다.
    - 협약 쪽이 틀렸다고 판단되면 코드를 고치지 말고 "협약 수정 제안"(무엇을, 왜, 어떻게 바꿀지)을 먼저 한다.
    - 팀 합의 후 이 문서를 먼저 고치고, 그다음 코드를 맞춘다.
- AI 응답 규칙 :
    - 코드를 주기 전에 어느 장 번호를 구현하는지 한 줄로 밝힌다.
    - 구현 후 "협약과 다른 점이 있는지"를 스스로 점검해서 알려준다.
    - 새로 정한 이름이 있으면 따로 목록으로 알려준다. (협약에 반영할 수 있게)

### 14.1. 진입 파일 내용 (`CLAUDE.md`, `GEMINI.md`, `AGENTS.md`에 같은 내용)

```
이 프로젝트는 외부 라이브러리 없이 JavaScript로 만드는 미니 리액트다.
작업을 시작하기 전에 docs/미니리액트_Phase1.md(공통 규칙 + Phase 1)와
docs/미니리액트_Phase2.md를 처음부터 끝까지 읽어라.
협약 문서가 코드보다 우선한다. Phase 1 문서 14장의 AI 사용 지침을 어기지 마라.
협약에 없는 내용은 추측하지 말고 먼저 질문해라.
한 번에 한 함수(또는 한 파일)만 구현해라.
```

### 14.2. 프롬프트 템플릿

구현을 시킬 때:

```
docs/미니리액트_Phase1.md 를 읽고, 8.3의 diff 를 구현해줘.
- 파일 : src/core/diff/diff.js
- 시그니처, 에러 조건, 부수 효과는 협약 그대로 지켜줘.
- 협약에 없는 이름이 필요하면 만들기 전에 먼저 알려줘.
- 구현 후 협약과 다른 점이 있는지 점검해서 알려줘.
```

테스트를 시킬 때:

```
Phase 1 문서 12장 기준으로 diff 테스트를 tests/core/diff/diff.test.js 에 작성해줘.
핵심 케이스 5가지와 12.3의 엣지 케이스를 포함해줘.
```

코드 리뷰를 시킬 때:

```
이 코드를 가독성, 세분화(함수·파일이 한 가지 책임만 하는가), 수정 편의 기준으로 평가해줘.
시그니처와 네이밍 규칙 위반이 있으면 먼저 알려주고, 고칠 점은 우선순위 순으로 3개만 말해줘.
```

협약을 바꾸고 싶을 때:

```
협약의 [장 번호] 가 [이유] 때문에 불편해.
코드는 고치지 말고, 협약 수정안(바꿀 내용, 이유, 영향받는 파일)만 먼저 제안해줘.
```

## 15. 작업 흐름과 완료 기준

- 하루 흐름 (개인 작업일) :
    1. 이 문서의 해당 장을 AI에게 주고, 함수 하나씩 구현한다.
    2. 함수 하나가 끝날 때마다 테스트를 돌린다.
    3. 코어타임 전에 다른 사람의 코드를 한 번 훑어본다.
- 코어타임 흐름 :
    1. 각자 5분 안에 자기 코드의 구조를 설명한다.
    2. 서로 가독성, 세분화, 수정 편의 기준으로 의견을 말한다.
    3. 테스트와 화면 확인 후 통합 결과를 기록한다.
- 완료 기준 (Definition of Done) : 아래를 모두 만족해야 "끝"이다.
    - 협약의 시그니처와 일치한다.
    - 3장 네이밍과 코드 형식을 지켰다.
    - 해당 테스트가 통과한다.
    - 4장 트리와 실제 파일이 일치한다.
    - 새로 만든 이름이나 변경은 문서에 반영했다.
- 막혔을 때 : 30분 이상 같은 문제에서 막히면 AI에게 "원인 후보 3가지와 확인 방법"을 먼저 물어보고, 그래도 안 풀리면 팀에 공유한다. 협약이 모호해서 막힌 것이면 코드를 우회하지 말고 이 문서를 고친다.
- 문서 관리 : 이 문서가 바뀌면 변경 이력에 남기고, 바뀐 날은 팀 전원이 다시 읽는다.

## 16. 용어 정리

- VNode : 실제 DOM을 흉내 낸 가벼운 자바스크립트 객체. 화면 구조를 데이터로 들고 있다.
- Diff : 이전 VNode 트리와 새 VNode 트리를 비교해서 달라진 곳을 찾는 일
- Patch : Diff 결과로 나온 "여기를 이렇게 바꿔라"라는 명령 한 개
- path : 루트에서 어떤 노드까지 가는 자식 번호 목록 (예: `[0, 2]`는 첫째 자식의 셋째 자식)
- 루트 컴포넌트 : State와 Hook을 가질 수 있는 유일한 컴포넌트
- 자식 컴포넌트 : props만 받고 VNode를 돌려주는 순수 함수. 상태가 없다.
- Lifting State Up : 자식들이 쓰는 상태를 부모(루트)로 끌어올려 관리하는 방식
- Hook : `useState`, `useEffect`, `useMemo`처럼 함수형 컴포넌트에서 상태나 부수 효과를 다루는 함수
- deps : Hook이 "값이 바뀌었는지"를 판단할 때 쓰는 의존성 배열
- Batching : 여러 상태 변경을 모아서 렌더링을 한 번만 하는 최적화
- 코어타임 : 팀이 저녁에 같이 모여 통합과 코드 리뷰를 하는 시간
- 시그니처 : 함수의 이름, 매개변수, 반환값 모양

## 17. 미정 사항

| 항목 | 후보 | 결정 날짜 | 결정자 |
|---|---|---|---|
| Phase 2 데모 주제 | 할 일 목록 (AI 제안), 변경 가능 | 11월 8일 (제안) | 팀 전원 |
| 차별화 추가 구현 요소 | 1) Batching (여러 상태 변경을 모아 한 번만 갱신) 2) `key` 기반 자식 비교 (순서 바뀜 대응) 3) Patch 로그 시각화 패널 4) `useRef`, `useReducer` 추가 5) Diff 성능 비교 화면 (전체 다시 그리기 vs 부분 갱신) | 11월 11일 | 팀 전원 |

## 18. 변경 이력

| 날짜 | 변경 내용 | 작성자 |
|---|---|---|
| 2026-11-07 | 팀원 3명의 문서를 취합해 Phase 1 문서(공통 규칙 포함) 초안 작성 | |
