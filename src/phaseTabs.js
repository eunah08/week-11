const phaseTabs = Array.from(document.querySelectorAll('[role="tablist"] [role="tab"]'));
const phasePanels = phaseTabs.map((tab) => document.getElementById(tab.getAttribute('aria-controls')));

// 기존 Phase DOM을 그대로 둔 채 탭의 선택 상태와 패널 표시만 전환한다.
function selectPhaseTab(selectedIndex) {
  phaseTabs.forEach((tab, index) => {
    const isSelected = index === selectedIndex;
    tab.setAttribute('aria-selected', String(isSelected));
    tab.tabIndex = isSelected ? 0 : -1;
    phasePanels[index].hidden = !isSelected;
  });
}

// 탭 목록에서 화살표와 Home·End로 이동할 위치를 구한다.
function findTabIndex(key, index) {
  const lastIndex = phaseTabs.length - 1;
  switch (key) {
    case 'ArrowLeft': return index === 0 ? lastIndex : index - 1;
    case 'ArrowRight': return index === lastIndex ? 0 : index + 1;
    case 'Home': return 0;
    case 'End': return lastIndex;
    default: return null;
  }
}

phaseTabs.forEach((tab, index) => {
  tab.addEventListener('click', () => selectPhaseTab(index));
  tab.addEventListener('keydown', (event) => {
    const nextIndex = findTabIndex(event.key, index);
    if (nextIndex === null) {
      return;
    }
    event.preventDefault();
    selectPhaseTab(nextIndex);
    phaseTabs[nextIndex].focus();
  });
});
