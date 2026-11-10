import { createElement, FunctionComponent } from '../../../src/core/index.js';
import { getCurrentComponent } from '../../../src/core/component/index.js';
import { test, assertEqual, assertThrows, assertTrue } from '../../testRunner.js';

function createContainer() {
  const container = document.createElement('div');

  document.body.append(container);
  return container;
}

// 바깥 변수 label을 보여 주는 가장 단순한 루트 컴포넌트를 만든다.
function createLabelRoot(state) {
  return () => createElement(
    'section',
    { class: state.rootClass },
    createElement('p', null, state.label),
  );
}

function createLabelInstance(container) {
  return new FunctionComponent(createLabelRoot({ label: '글', rootClass: 'a' }), {}, container);
}

test('FunctionComponent : mount하면 container에 루트 DOM 하나만 들어간다', () => {
  const container = createContainer();

  container.append(document.createElement('span'));
  new FunctionComponent(createLabelRoot({ label: '안녕', rootClass: 'a' }), {}, container).mount();

  assertEqual(container.childNodes.length, 1);
  assertEqual(container.innerHTML, '<section class="a"><p>안녕</p></section>');
});

test('FunctionComponent : 루트 컴포넌트에 props를 넘긴다', () => {
  const container = createContainer();
  const root = (props) => createElement('p', null, props.message);

  new FunctionComponent(root, { message: '받은 값' }, container).mount();
  assertEqual(container.textContent, '받은 값');
});

test('FunctionComponent : update하면 바뀐 부분만 반영하고 나머지 DOM 노드는 그대로다', () => {
  const container = createContainer();
  const state = { label: '전', rootClass: 'a' };
  const instance = new FunctionComponent(createLabelRoot(state), {}, container);

  instance.mount();

  const rootNodeBefore = container.firstChild;
  const paragraphBefore = rootNodeBefore.firstChild;

  state.label = '후';
  instance.update();

  assertEqual(container.textContent, '후');
  assertTrue(container.firstChild === rootNodeBefore, '루트 DOM이 교체됨');
  assertTrue(rootNodeBefore.firstChild === paragraphBefore, '바뀌지 않은 p가 교체됨');
});

test('FunctionComponent : 루트 태그가 바뀌어도 새 루트로 이어서 update한다', () => {
  const container = createContainer();
  let tag = 'section';
  const instance = new FunctionComponent(() => createElement(tag, null, '글'), {}, container);

  instance.mount();
  tag = 'article';
  instance.update();
  assertEqual(container.innerHTML, '<article>글</article>');
  assertTrue(instance.rootNode === container.firstChild, 'rootNode가 새 루트가 아님');

  tag = 'main';
  instance.update();
  assertEqual(container.innerHTML, '<main>글</main>');
});

test('FunctionComponent : 렌더링 중에는 현재 컴포넌트가 이 인스턴스이고 끝나면 null이다', () => {
  const container = createContainer();
  let seenComponent = null;
  const root = () => {
    seenComponent = getCurrentComponent();
    return createElement('p', null);
  };
  const instance = new FunctionComponent(root, {}, container);

  instance.mount();
  assertTrue(seenComponent === instance, '렌더링 중 현재 컴포넌트가 다름');
  assertTrue(getCurrentComponent() === null, '렌더링이 끝났는데 현재 컴포넌트가 남아 있음');
});

test('FunctionComponent : 렌더링 시작마다 hookIndex를 0으로 되돌린다', () => {
  const container = createContainer();
  const seenIndexes = [];
  const root = () => {
    seenIndexes.push(getCurrentComponent().hookIndex);
    return createElement('p', null);
  };
  const instance = new FunctionComponent(root, {}, container);

  instance.mount();
  instance.hookIndex = 5;
  instance.update();
  assertEqual(seenIndexes, [0, 0]);
});

test('FunctionComponent : unmount하면 DOM이 사라지고 다시 mount할 수 있다', () => {
  const container = createContainer();
  const instance = createLabelInstance(container);

  instance.mount();
  instance.unmount();
  assertEqual(container.childNodes.length, 0);
  assertTrue(instance.isMounted === false, 'isMounted가 true로 남음');

  instance.mount();
  assertEqual(container.childNodes.length, 1);
});

test('FunctionComponent : mount하지 않은 상태의 unmount는 아무 일도 하지 않는다', () => {
  const container = createContainer();
  const instance = createLabelInstance(container);

  instance.unmount();
  assertEqual(container.childNodes.length, 0);
});

test('FunctionComponent : 이미 mount했으면 Error', () => {
  const container = createContainer();
  const instance = createLabelInstance(container);

  instance.mount();
  assertThrows(() => instance.mount(), Error, '[FunctionComponent] 이미 mount되었습니다');
});

test('FunctionComponent : mount 전에 update하면 Error', () => {
  const instance = createLabelInstance(createContainer());

  assertThrows(() => instance.update(), Error, '[FunctionComponent] mount 전에 update할 수 없습니다');
});

test('FunctionComponent : 루트 컴포넌트가 함수가 아니면 TypeError', () => {
  assertThrows(
    () => new FunctionComponent('root', {}, createContainer()),
    TypeError,
    '루트 컴포넌트는 함수여야 합니다',
  );
});

test('FunctionComponent : container가 DOM 요소가 아니면 TypeError', () => {
  assertThrows(
    () => new FunctionComponent(() => createElement('p', null), {}, null),
    TypeError,
    'container는 DOM 요소여야 합니다',
  );
});

test('FunctionComponent : 루트가 element VNode가 아니면 TypeError이고 렌더링 문맥을 정리한다', () => {
  const container = createContainer();

  for (const badResult of ['글자', null, { type: 'text', text: '글' }]) {
    const instance = new FunctionComponent(() => badResult, {}, container);

    assertThrows(() => instance.mount(), TypeError, '루트는 element VNode여야 합니다');
    assertTrue(getCurrentComponent() === null, '에러 뒤에 현재 컴포넌트가 남아 있음');
    assertTrue(instance.isRendering === false, '에러 뒤에 isRendering이 남아 있음');
  }
});

test('FunctionComponent : 루트 컴포넌트가 에러를 던져도 렌더링 문맥을 정리한다', () => {
  const instance = new FunctionComponent(
    () => {
      throw new Error('실패');
    },
    {},
    createContainer(),
  );

  assertThrows(() => instance.mount(), Error, '실패');
  assertTrue(getCurrentComponent() === null, '에러 뒤에 현재 컴포넌트가 남아 있음');
});

test('FunctionComponent : update 중 루트가 잘못된 값을 반환하면 DOM은 그대로다', () => {
  const container = createContainer();
  let shouldBreak = false;
  const root = () => (shouldBreak ? '글자' : createElement('p', null, '정상'));
  const instance = new FunctionComponent(root, {}, container);

  instance.mount();
  shouldBreak = true;
  assertThrows(() => instance.update(), TypeError, '루트는 element VNode여야 합니다');
  assertEqual(container.innerHTML, '<p>정상</p>');
});

test('FunctionComponent : 렌더링 중에 모아 둔 Effect는 DOM 반영 뒤에 등록 순서대로 실행한다', () => {
  const container = createContainer();
  const logs = [];
  const root = () => {
    const component = getCurrentComponent();

    component.pendingEffects.push(() => logs.push(`A:${container.textContent}`));
    component.pendingEffects.push(() => logs.push(`B:${container.textContent}`));
    return createElement('p', null, '화면');
  };
  const instance = new FunctionComponent(root, {}, container);

  instance.mount();
  assertEqual(logs, ['A:화면', 'B:화면']);
  instance.update();
  assertEqual(logs, ['A:화면', 'B:화면', 'A:화면', 'B:화면']);
});
