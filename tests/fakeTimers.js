// setTimeout, clearTimeout을 가짜로 바꿔 시간을 기다리지 않고 테스트하는 도구 (도구는 자율, Phase 1 문서 12장)

/**
 * setTimeout, clearTimeout을 가짜로 바꾼다. 테스트가 끝나면 uninstall()로 꼭 되돌린다.
 * @returns {{
 *   getPendingDelays: Function,
 *   runPendingTimers: Function,
 *   uninstall: Function,
 * }} 대기 중인 타이머의 지연 시간 목록, 대기 중인 타이머 실행, 원래대로 되돌리기
 */
export function installFakeTimers() {
  const originalSetTimeout = globalThis.setTimeout;
  const originalClearTimeout = globalThis.clearTimeout;
  const pendingTimers = new Map();
  let nextTimerId = 1;

  globalThis.setTimeout = (callback, delayMs) => {
    const timerId = nextTimerId;

    nextTimerId += 1;
    pendingTimers.set(timerId, { callback, delayMs });
    return timerId;
  };

  globalThis.clearTimeout = (timerId) => {
    pendingTimers.delete(timerId);
  };

  return {
    getPendingDelays() {
      return Array.from(pendingTimers.values()).map((timer) => timer.delayMs);
    },
    runPendingTimers() {
      const timers = Array.from(pendingTimers.values());

      pendingTimers.clear();

      for (const timer of timers) {
        timer.callback();
      }
    },
    uninstall() {
      globalThis.setTimeout = originalSetTimeout;
      globalThis.clearTimeout = originalClearTimeout;
    },
  };
}
