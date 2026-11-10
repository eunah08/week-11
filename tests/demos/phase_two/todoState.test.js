import {
  addTodo,
  toggleTodo,
  removeTodo,
  countRemainingTodos,
} from '../../../src/demos/phase_two/todoState.js';
import { test, assertEqual, assertTrue } from '../../testRunner.js';

function createTodos() {
  return [
    { id: 1, text: '가', isDone: true },
    { id: 3, text: '나', isDone: false },
  ];
}

test('todoState : addTodo는 가장 큰 id + 1로 맨 뒤에 추가하고 원래 배열은 그대로 둔다', () => {
  const todos = createTodos();
  const nextTodos = addTodo(todos, '다');

  assertEqual(nextTodos[2], { id: 4, text: '다', isDone: false });
  assertEqual(todos.length, 2, '원래 배열이 바뀜');
});

test('todoState : 빈 목록에 addTodo하면 id는 1이다', () => {
  assertEqual(addTodo([], '처음'), [{ id: 1, text: '처음', isDone: false }]);
});

test('todoState : toggleTodo는 그 항목만 완료 여부를 뒤집고 나머지는 같은 객체로 둔다', () => {
  const todos = createTodos();
  const nextTodos = toggleTodo(todos, 3);

  assertEqual(nextTodos[1].isDone, true);
  assertTrue(nextTodos[0] === todos[0], '안 바뀐 항목은 같은 객체여야 함');
  assertEqual(todos[1].isDone, false, '원래 항목이 바뀜');
});

test('todoState : removeTodo는 그 항목만 빼고, 없는 id면 내용이 같다', () => {
  const todos = createTodos();

  assertEqual(removeTodo(todos, 1).map((todo) => todo.id), [3]);
  assertEqual(removeTodo(todos, 99), todos);
});

test('todoState : countRemainingTodos는 완료되지 않은 개수를 센다', () => {
  assertEqual(countRemainingTodos(createTodos()), 1);
  assertEqual(countRemainingTodos([]), 0);
});
