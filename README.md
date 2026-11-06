# 미니 리액트 (week-11)

Vanilla JS로 만든 미니 리액트입니다. 외부 프레임워크를 사용하지 않습니다.

## 구성
- Phase 1: Virtual DOM 변환, Diff, Patch, State History
- Phase 2: FunctionComponent, useState, useEffect, useMemo

## 실행
index.html을 브라우저에서 열면 됩니다.

# week-11 파일 구조

| 경로                        | 역할 |
--------------------------------------------------------------------------------------------------------
| `index.html`               | 화면 뼈대. 실제 영역, 테스트 영역, 뒤로가기·앞으로가기·Patch 버튼이 있다. 
| `css/style.css`            | 화면 레이아웃과 버튼, 영역 스타일을 정의한다. 
| `src/main.js`              | 진입점. 실제 영역과 테스트 영역을 연결한다. (지금은 placeholder) 
| `src/vdom/.gitkeep`        | Phase 1 자리. Virtual DOM 변환, Diff, Patch 코드를 둔다. 
| `src/component/.gitkeep`   | Phase 2 자리. FunctionComponent, useState, useEffect, useMemo 코드를 둔다. 
| `tests/.gitkeep`           | 단위 테스트 자리. 
| `README.md`                | 프로젝트 소개, 구성, 실행 방법을 적는다. 
