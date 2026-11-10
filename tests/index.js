// 브라우저(tests/index.html)와 Node(tests/runNodeTests.js)가 같은 테스트 목록을 쓴다.
// 새 테스트 파일을 만들면 여기에 한 줄만 추가한다.
import './core/vdom/domToVNode.test.js';
import './core/vdom/render.test.js';
import './core/vdom/props.test.js';
import './core/vdom/cloneVNode.test.js';
import './core/diff/diff.test.js';
import './core/diff/diffSameReference.test.js';
import './core/patch/applyPatches.test.js';
import './demos/phase_one/StateHistory.test.js';
import './demos/phase_one/app.test.js';
import './core/component/createElement.test.js';
import './core/component/FunctionComponent.test.js';
import './core/component/batching.test.js';
import './core/component/memo.test.js';
import './core/hooks/useState.test.js';
import './core/hooks/useEffect.test.js';
import './core/hooks/useMemo.test.js';
import './core/hooks/useCallback.test.js';
import './core/hooks/useDebouncedValue.test.js';
import './core/hooks/getHookSlot.test.js';
import './demos/phase_two/todoState.test.js';
import './demos/phase_two/todoApp.test.js';

export { runTests } from './testRunner.js';
