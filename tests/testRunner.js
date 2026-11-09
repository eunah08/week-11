// 설치 없이 브라우저에서 돌리는 최소 테스트 도구 (Phase 1 문서 12장, 도구는 자율)

const registeredTests = [];

/**
 * 테스트 하나를 등록한다.
 * @param {string} name 테스트 이름
 * @param {Function} run 실패하면 예외를 던지는 테스트 본문
 * @returns {void}
 */
export function test(name, run) {
  registeredTests.push({ name, run });
}

/**
 * 두 값의 구조가 같은지 확인한다. 함수는 참조로 비교한다.
 * @param {*} actual 실제 값
 * @param {*} expected 기대 값
 * @param {string} [message] 실패 시 덧붙일 설명
 * @returns {void}
 * @throws {Error} 구조가 다를 때
 */
export function assertEqual(actual, expected, message = '') {
  if (!isDeepEqual(actual, expected)) {
    const detail = `기대 ${JSON.stringify(expected)}, 실제 ${JSON.stringify(actual)}`;
    throw new Error(`${message} ${detail}`.trim());
  }
}

/**
 * 조건이 참인지 확인한다.
 * @param {boolean} condition 확인할 조건
 * @param {string} message 실패 시 설명
 * @returns {void}
 * @throws {Error} 조건이 거짓일 때
 */
export function assertTrue(condition, message) {
  if (condition !== true) {
    throw new Error(message);
  }
}

/**
 * 함수가 지정한 종류와 메시지의 에러를 던지는지 확인한다.
 * @param {Function} run 실행할 함수
 * @param {Function} errorType 기대하는 에러 클래스
 * @param {string} messagePart 에러 메시지에 들어 있어야 할 문자열
 * @returns {void}
 * @throws {Error} 에러가 없거나 종류·메시지가 다를 때
 */
export function assertThrows(run, errorType, messagePart) {
  try {
    run();
  } catch (error) {
    assertTrue(error instanceof errorType, `${errorType.name}가 아님: ${error}`);
    assertTrue(error.message.includes(messagePart), `메시지가 다름: ${error.message}`);
    return;
  }

  throw new Error(`${errorType.name}가 발생하지 않음`);
}

/**
 * 등록한 테스트를 모두 실행한다.
 * @returns {{ name: string, isPassed: boolean, errorMessage: string }[]} 테스트별 결과
 */
export function runTests() {
  return registeredTests.map(({ name, run }) => {
    try {
      run();
      return { name, isPassed: true, errorMessage: '' };
    } catch (error) {
      return { name, isPassed: false, errorMessage: error.message };
    }
  });
}

function isDeepEqual(left, right) {
  if (Object.is(left, right)) {
    return true;
  }

  const isBothObjects = left !== null && right !== null
    && typeof left === 'object' && typeof right === 'object';

  if (!isBothObjects || Array.isArray(left) !== Array.isArray(right)) {
    return false;
  }

  const leftKeys = Object.keys(left);
  const rightKeys = Object.keys(right);

  if (leftKeys.length !== rightKeys.length) {
    return false;
  }

  return leftKeys.every((key) => key in right && isDeepEqual(left[key], right[key]));
}
