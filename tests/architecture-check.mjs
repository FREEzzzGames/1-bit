import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = (path) => readFileSync(new URL(path, import.meta.url), 'utf8');
const html = read('../index.html');
const css = read('../assets/console.css');
const game = read('../assets/game.js');
const skins = read('../assets/skins.js');
const svg = read('../assets/console-shell.svg');
const screenContract = read('../assets/game-screen.js');

assert.equal((html.match(/<canvas\b/g) || []).length, 1, 'Expected one game canvas');
assert.equal((html.match(/<main\b/g) || []).length, 1, 'Expected one console root');
assert.equal((html.match(/<style\b/gi) || []).length, 0, 'Styles must live in console.css');
assert.equal((html.match(/<script(?![^>]*\bsrc=)[^>]*>/gi) || []).length, 0, 'Scripts must be external modules');
assert.ok(html.includes('./assets/console.css?v=20'), 'Console stylesheet must be linked');
assert.ok(html.includes('./assets/game.js?v=21'), 'Game runtime must be linked');
assert.ok(html.includes('./assets/skins.js?v=20'), 'Skin module must be linked');

const layers = ['class="shell-art"', 'class="shell-skin-art"', 'class="deck-tint"', 'class="screen-bezel"', 'class="controls"'];
let lastIndex = -1;
for (const layer of layers) {
  const index = html.indexOf(layer);
  assert.ok(index > lastIndex, `Layer order invalid at ${layer}`);
  lastIndex = index;
}
assert.equal((css.match(/\.controls\s*\{/g) || []).length, 1, 'Only one authoritative controls rule is allowed');
assert.equal((css.match(/\.screen-bezel\s*\{/g) || []).length, 1, 'Only one authoritative display rule is allowed');
assert.ok(css.includes('.controls{position:absolute;z-index:4;inset:0;display:block;'), 'Controls input layer must cover the console');
assert.ok(css.includes('background:transparent!important;box-shadow:none!important;pointer-events:none'), 'Input layer must not paint over shell art');
assert.ok(css.includes('.console[data-shell-skin="violet"]>.shell-skin-art'), 'Shell skins must target only the shell tint layer');
assert.ok(css.includes('--deck-tint:rgba(0,190,225,.85)'), 'Control deck must have its own tint layer');
assert.ok(svg.includes('id="cutouts"') || svg.includes('id="mask"'), 'Shell SVG should retain its aperture mask');

for (const id of ['dark','arctic','ember','forest','violet']) {
  assert.ok(html.includes(`data-shell-skin="${id}"`), `Missing shell skin: ${id}`);
}
for (const id of ['graphite','cyan','amber','violet','ivory']) {
  assert.ok(html.includes(`data-pad-skin="${id}"`), `Missing control-panel skin: ${id}`);
}
assert.ok(skins.includes('shadowSpireShellSkin') && skins.includes('shadowSpirePadSkin'), 'Skin preferences must persist independently');
assert.ok(skins.includes('.style-option[data-shell-skin]') && skins.includes('.style-option[data-pad-skin]'), 'Skin handlers must bind only to options');
for (const fn of ['rs','pl','stl','up','rd','loop','onDown','onUp','onMove','onCancel','onKD','onKU','dMenu','dHUD']) {
  assert.ok(new RegExp(`function\\s+${fn}\\s*\\(`).test(game), `Missing runtime function: ${fn}`);
}
assert.ok(screenContract.includes('width: 208, height: 320'), 'Shared game screen must stay 208x320');
assert.ok(html.includes('./assets/game-screen.js?v=1'), 'Shared game screen contract must load before games');
assert.ok(game.includes('window.CONSOLE_GAME_SCREEN'), 'Game must use the shared screen contract');
assert.ok(game.includes('function makeTowerRows'), 'Levels must be generated for the fixed portrait game screen');
for (const [feature,pattern] of [
  ['coyote-time jump forgiveness', 'p.coyote>0'],
  ['jump input buffering', 'p.jumpBuf=.14'],
  ['variable jump height', 'p.vy-=GR*.22*dt'],
  ['double jump', 'p.jumpsUsed<2'],
  ['wall jump', 'wallContact!==0'],
  ['wall slide', 'p.wallDir!==0&&!p.onGround&&p.vy>95'],
  ['double-tap dash', 'now-dirTap.right<280'],
  ['dash damage grace', 'if(p.dashT>0)return'],
  ['high-contrast level label', 'cx.fillRect(2,2,Math.min(VW-4,tw(title,1)+10),11)']
]) assert.ok(game.includes(pattern), `Missing gameplay feature: ${feature}`);

assert.ok(game.includes('requestAnimationFrame(loop)'), 'Game loop must be active');
assert.ok(game.includes('localStorage'), 'Game progress persistence must remain present');

console.log('SHADOW SPIRE architecture checks passed.');
