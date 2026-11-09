// 실제 영역에 처음 넣을 샘플 HTML (Phase 1 문서 9.3)
// 들여쓰기 공백과 주석은 domToVNode가 무시하므로 읽기 쉽게 줄을 나눠 둔다
export const SAMPLE_HTML = [
  '<div class="sample-card">',
  '  <h1 class="sample-title" title="미니 리액트">미니 리액트</h1>',
  '  <!-- 주석은 Virtual DOM에서 무시된다 -->',
  '  <p>',
  '    Virtual DOM 테스트 문장입니다.',
  '    <strong class="sample-highlight">바뀐 부분만</strong> 실제 영역에 반영됩니다.',
  '  </p>',
  '  <ul class="sample-list">',
  '    <li>첫 번째 항목</li>',
  '    <li>두 번째 항목</li>',
  '    <li>세 번째 항목</li>',
  '  </ul>',
  '</div>',
].join('\n');
