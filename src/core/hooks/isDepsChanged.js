/**
 * 의존성 배열이 바뀌었는지 판단한다. (Phase 2 문서 8장)
 * 이전 의존성이 없거나(첫 실행), 다음 의존성이 없거나(생략), 길이가 다르거나,
 * 항목 하나라도 Object.is가 false면 바뀐 것으로 본다.
 * @param {any[] | undefined} previousDependencies 이전 의존성 배열
 * @param {any[] | undefined} nextDependencies 이번 의존성 배열
 * @returns {boolean} 바뀌었으면 true
 */
export function isDepsChanged(previousDependencies, nextDependencies) {
  if (previousDependencies === undefined || nextDependencies === undefined) {
    return true;
  }

  if (previousDependencies.length !== nextDependencies.length) {
    return true;
  }

  return nextDependencies.some(
    (dependency, index) => !Object.is(dependency, previousDependencies[index]),
  );
}
