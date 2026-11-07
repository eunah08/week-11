# 미니 리액트 (week-11)

외부 프레임워크 없이 JavaScript로 직접 만드는 미니 리액트입니다.

- Phase 1 : Virtual DOM 변환, Diff, Patch, 상태 이력
- Phase 2 : 함수형 컴포넌트, State, Hooks (`useState`, `useEffect`, `useMemo`)

## 협약 문서

코드보다 협약 문서가 우선합니다. 작업 전에 꼭 읽어 주세요.

- [`docs/미니리액트_Phase1.md`](docs/미니리액트_Phase1.md) : 공통 규칙 + Phase 1
- [`docs/미니리액트_Phase2.md`](docs/미니리액트_Phase2.md) : Phase 2
- [`docs/implementation_notes.md`](docs/implementation_notes.md) : 구현 안내

## AI 진입 파일

AI별 진입 파일은 `ai/` 폴더에 있습니다. 루트에 없어서 자동으로 읽히지 않으므로,
작업을 시작할 때 AI에게 먼저 읽게 합니다.

- Claude : `ai/CLAUDE.md 를 읽고 시작해`
- Gemini : `ai/GEMINI.md 를 읽고 시작해`
- GPT(Codex 등) : `ai/AGENTS.md 를 읽고 시작해`

## 실행

ES Modules를 쓰므로 `file://`로 열면 동작하지 않습니다. 저장소 루트에서 로컬 정적 서버를 띄웁니다.

```bash
npx serve .
# 또는
python3 -m http.server 8000
```

## 파일 구조

Phase 1 문서 4장의 트리를 따릅니다.
