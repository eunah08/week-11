// 극한 규모 테스트. 단위 테스트와 달리 느리므로 tests/stress/index.html에서 따로 실행한다.
// 규모를 단계적으로 키우며 성공 여부와 시간을 잰다. 한 단계가 시간 예산을 넘으면 더 큰 규모는 건너뛴다.
// 측정은 document에 붙이지 않은 DOM에서 한다. (레이아웃·그리기 비용은 포함하지 않는다)

import { NODE_TYPES, render, diff, applyPatches, domToVNode } from '../../src/core/index.js';
import { StateHistory } from '../../src/demos/phase_one/StateHistory.js';

// 이 시간(ms)을 넘으면 같은 시나리오의 더 큰 규모는 실행하지 않는다.
const TIME_BUDGET_MS = 5000;

const text = (value) => ({ type: NODE_TYPES.TEXT, text: value });
const element = (tag, props, children) => ({ type: NODE_TYPES.ELEMENT, tag, props, children });

// ---------- 측정 도구 ----------

function measure(run) {
  const startedAt = performance.now();

  try {
    const value = run();

    return { ok: true, ms: performance.now() - startedAt, value };
  } catch (error) {
    return { ok: false, ms: performance.now() - startedAt, error: `${error.name}: ${error.message}` };
  }
}

// 깊은 트리에서도 스택을 쓰지 않도록 반복문으로 비교한다.
function isSameVNode(left, right) {
  const pairs = [[left, right]];

  while (pairs.length > 0) {
    const [a, b] = pairs.pop();

    if (a.type !== b.type) return false;
    if (a.type === NODE_TYPES.TEXT) {
      if (a.text !== b.text) return false;
      continue;
    }
    if (a.tag !== b.tag || a.children.length !== b.children.length) return false;

    const aKeys = Object.keys(a.props);

    if (aKeys.length !== Object.keys(b.props).length) return false;
    if (aKeys.some((key) => a.props[key] !== b.props[key])) return false;

    a.children.forEach((child, i) => pairs.push([child, b.children[i]]));
  }

  return true;
}

// 부모가 있어야 applyPatches가 루트를 교체할 수 있다.
function mount(domNode) {
  document.createElement('div').append(domNode);
  return domNode;
}

// render를 거치지 않고 VNode로 DOM을 만든다. (render가 실패하는 깊이에서도 다음 단계를 재기 위해)
function buildDomWithoutRecursion(vnode) {
  const makeNode = (v) => (v.type === NODE_TYPES.TEXT
    ? document.createTextNode(v.text)
    : document.createElement(v.tag));
  const rootNode = makeNode(vnode);
  const stack = [[vnode, rootNode]];

  while (stack.length > 0) {
    const [v, domNode] = stack.pop();

    if (v.type === NODE_TYPES.TEXT) continue;
    for (const [key, value] of Object.entries(v.props)) domNode.setAttribute(key, value);
    for (const child of v.children) {
      const childNode = makeNode(child);

      domNode.append(childNode);
      stack.push([child, childNode]);
    }
  }

  return rootNode;
}

// diff → applyPatches → domToVNode로 다시 읽어 next와 같은지 확인하는 공통 흐름.
// 각 단계를 따로 재서 어느 단계가 느리거나 깨지는지 보이게 한다.
function runPipeline(previousVNode, nextVNode, { useRender = true } = {}) {
  const stages = {};
  const renderStage = measure(() => mount(useRender
    ? render(previousVNode)
    : buildDomWithoutRecursion(previousVNode)));

  stages.render = renderStage;

  // render가 실패해도 이후 단계를 재 볼 수 있도록 반복문으로 만든 DOM을 쓴다.
  const rootNode = renderStage.ok ? renderStage.value : mount(buildDomWithoutRecursion(previousVNode));

  stages.diff = measure(() => diff(previousVNode, nextVNode));
  if (!stages.diff.ok) return { stages, patchCount: null, isCorrect: false };

  const patches = stages.diff.value;

  stages.apply = measure(() => applyPatches(rootNode, patches));
  if (!stages.apply.ok) return { stages, patchCount: patches.length, isCorrect: false };

  stages.read = measure(() => domToVNode(stages.apply.value));
  if (!stages.read.ok) return { stages, patchCount: patches.length, isCorrect: null };

  return { stages, patchCount: patches.length, isCorrect: isSameVNode(stages.read.value, nextVNode) };
}

// ---------- 시나리오 ----------

// 1. 깊이 : div를 N겹 중첩하고 맨 안쪽 글자 하나만 바꾼다.
function deepChain(depth, leafText) {
  let node = text(leafText);

  for (let i = 0; i < depth; i++) node = element('div', {}, [node]);
  return node;
}

// 2~4. 너비 : 부모 하나에 li N개
function wideList(count, label = (i) => `항목 ${i}`) {
  return element('ul', {}, Array.from({ length: count }, (_, i) => element('li', {}, [text(label(i))])));
}

// 5. 노드 수가 많은 균형 트리 (가지 수 branching, 깊이 depth)
function balancedTree(branching, depth) {
  if (depth === 0) return text('잎');
  return element('div', { class: `d${depth}` },
    Array.from({ length: branching }, () => balancedTree(branching, depth - 1)));
}

export const SCENARIOS = [
  {
    name: '깊이 : N겹 중첩된 맨 안쪽 글자 변경',
    scales: [100, 1000, 3000, 5000, 10000, 30000],
    run: (n) => runPipeline(deepChain(n, '이전'), deepChain(n, '다음')),
  },
  {
    name: '너비 : 자식 N개의 글자 전부 변경 (patch N개)',
    scales: [1000, 5000, 10000, 20000, 50000],
    run: (n) => runPipeline(wideList(n), wideList(n, (i) => `바뀐 항목 ${i}`)),
  },
  {
    name: '너비 : 자식 N개 앞에 하나 삽입 (인덱스 비교라 patch N개)',
    scales: [1000, 5000, 10000, 20000, 50000],
    run: (n) => {
      const previousVNode = wideList(n);
      const nextVNode = element('ul', {}, [element('li', {}, [text('새 항목')]), ...previousVNode.children]);

      return runPipeline(previousVNode, nextVNode);
    },
  },
  {
    name: '너비 : 자식 N개 전부 삭제 (REMOVE N개)',
    scales: [1000, 5000, 10000, 20000, 50000],
    run: (n) => runPipeline(wideList(n), element('ul', {}, [])),
  },
  {
    name: '노드 수 : 변경 없는 큰 트리 비교 (가지 4개, 깊이 d → 약 4^d개)',
    scales: [5, 7, 8, 9],
    run: (d) => runPipeline(balancedTree(4, d), balancedTree(4, d)),
  },
  {
    name: '내용 : 글자 길이 N자 텍스트 변경',
    scales: [1e5, 1e6, 1e7, 5e7],
    run: (n) => runPipeline(element('p', {}, [text('가'.repeat(n))]), element('p', {}, [text('나'.repeat(n))])),
  },
  {
    name: '내용 : 한 요소의 속성 N개 전부 변경',
    scales: [1000, 10000, 50000],
    run: (n) => {
      const props = (prefix) => Object.fromEntries(Array.from({ length: n }, (_, i) => [`data-a${i}`, `${prefix}${i}`]));

      return runPipeline(element('div', props('이전'), []), element('div', props('다음'), []));
    },
  },
  {
    name: '기록 : StateHistory에 N개 push 후 처음까지 뒤로, 끝까지 앞으로',
    scales: [1000, 10000, 50000, 100000],
    run: (n) => {
      const vnodes = Array.from({ length: n }, (_, i) => element('p', {}, [text(`상태 ${i}`)]));
      const stages = {};
      let history;

      stages.push = measure(() => {
        history = new StateHistory(vnodes[0]);
        for (let i = 1; i < n; i++) history.push(vnodes[i]);
      });
      if (!stages.push.ok) return { stages, isCorrect: false };
      stages.back = measure(() => { while (history.back() !== null); });
      const isAtStart = history.getCurrent() === vnodes[0];

      stages.forward = measure(() => { while (history.forward() !== null); });
      const isAtEnd = history.getCurrent() === vnodes[n - 1] && history.getCount() === n;

      return { stages, isCorrect: isAtStart && isAtEnd };
    },
  },
  {
    name: '기록 : 노드 약 N개짜리 트리를 1000번 기록하며 diff·patch (실제 데모와 같은 흐름)',
    scales: [100, 1000, 5000],
    run: (n) => {
      const STEPS = 1000;
      const stages = {};
      const rootHolder = {};
      const versionOf = (step) => wideList(n, (i) => (i === step % n ? `변경 ${step}` : `항목 ${i}`));
      let history;
      let isCorrect = true;

      stages.loop = measure(() => {
        const first = versionOf(-1);

        history = new StateHistory(first);
        rootHolder.node = mount(render(first));
        for (let step = 0; step < STEPS; step++) {
          const next = versionOf(step);

          rootHolder.node = applyPatches(rootHolder.node, diff(history.getCurrent(), next));
          history.push(next);
        }
      });
      if (!stages.loop.ok) return { stages, isCorrect: false };

      // 처음까지 뒤로 갔다가 끝까지 다시 앞으로 오면서 매번 DOM을 맞춘다.
      stages.travel = measure(() => {
        let current = history.getCurrent();
        let previous;

        while ((previous = history.back()) !== null) {
          rootHolder.node = applyPatches(rootHolder.node, diff(current, previous));
          current = previous;
        }
        isCorrect = isSameVNode(domToVNode(rootHolder.node), versionOf(-1));
        while ((previous = history.forward()) !== null) {
          rootHolder.node = applyPatches(rootHolder.node, diff(current, previous));
          current = previous;
        }
      });
      isCorrect = isCorrect && isSameVNode(domToVNode(rootHolder.node), versionOf(STEPS - 1));

      return { stages, isCorrect };
    },
  },
];

/**
 * 시나리오를 하나씩, 규모를 키워 가며 실행한다. 단계마다 화면이 갱신되도록 쉬어 간다.
 * @param {(row: object) => void} onResult 결과 한 줄을 받을 함수
 * @returns {Promise<object[]>} 전체 결과
 */
export async function runStressTests(onResult) {
  const rows = [];

  for (const scenario of SCENARIOS) {
    let skipReason = '';

    for (const scale of scenario.scales) {
      await new Promise((resolve) => setTimeout(resolve, 0));

      let row;

      if (skipReason) {
        row = { scenario: scenario.name, scale, skipped: skipReason };
      } else {
        const totalStartedAt = performance.now();
        const result = measure(() => scenario.run(scale));
        const totalMs = performance.now() - totalStartedAt;

        row = result.ok
          ? { scenario: scenario.name, scale, totalMs, ...result.value }
          : { scenario: scenario.name, scale, totalMs, crashed: result.error };

        const isSlow = totalMs > TIME_BUDGET_MS;
        const hasFailedStage = row.crashed || Object.values(row.stages || {}).some((stage) => !stage.ok);

        if (isSlow) skipReason = `앞 규모가 ${TIME_BUDGET_MS}ms를 넘어 건너뜀`;
        else if (hasFailedStage) skipReason = '앞 규모에서 실패해 건너뜀';
      }

      rows.push(row);
      onResult(row);
    }
  }

  return rows;
}
