// 할 일 목록의 상태를 바꾸는 순수 함수 모음. 모두 새 배열을 돌려주고 받은 배열은 고치지 않는다.
// 화면(DOM)이나 Hook을 쓰지 않으므로 컴포넌트 없이 바로 테스트할 수 있다.

/**
 * 할 일을 맨 뒤에 추가한다. id는 지금 있는 가장 큰 id + 1이다.
 * @param {object[]} todos 지금 할 일 목록
 * @param {string} text 추가할 글자 (앞뒤 공백은 지운 값)
 * @returns {object[]} 새 목록
 */
export function addTodo(todos, text) {
  const lastId = todos.reduce((maxId, todo) => Math.max(maxId, todo.id), 0);

  return [...todos, { id: lastId + 1, text, isDone: false }];
}

/**
 * id가 같은 할 일의 완료 여부를 뒤집는다.
 * @param {object[]} todos 지금 할 일 목록
 * @param {number} todoId 바꿀 할 일의 id
 * @returns {object[]} 새 목록
 */
export function toggleTodo(todos, todoId) {
  return todos.map((todo) => (todo.id === todoId ? { ...todo, isDone: !todo.isDone } : todo));
}

/**
 * id가 같은 할 일을 뺀다.
 * @param {object[]} todos 지금 할 일 목록
 * @param {number} todoId 뺄 할 일의 id
 * @returns {object[]} 새 목록
 */
export function removeTodo(todos, todoId) {
  return todos.filter((todo) => todo.id !== todoId);
}

/**
 * 아직 끝나지 않은 할 일 개수를 센다.
 * @param {object[]} todos 할 일 목록
 * @returns {number} 남은 개수
 */
export function countRemainingTodos(todos) {
  return todos.filter((todo) => !todo.isDone).length;
}
