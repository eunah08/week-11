// 실제 영역에 처음 넣을 샘플 HTML (Phase 1 문서 9.3)
// 12.3의 특수 속성(class, checked, disabled)을 화면에서 확인할 수 있게 넣었다.
export const SAMPLE_HTML = `
<section class="sample-card">
  <h3 class="sample-title">오늘의 할 일</h3>
  <p class="sample-text">테스트 영역에서 내용을 고친 뒤 <b>Patch</b>를 눌러 보세요.</p>
  <ul class="sample-list">
    <li>Virtual DOM 만들기</li>
    <li>Diff 계산하기</li>
    <li>Patch 적용하기</li>
  </ul>
  <label class="sample-label"><input type="checkbox" checked> 완료 표시</label>
  <button type="button" class="sample-button" disabled>비활성 버튼</button>
</section>
`;
