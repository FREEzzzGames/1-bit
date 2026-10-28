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
assert.ok(html.includes('./assets/console.css?v=25'), 'Console stylesheet must be linked');
assert.ok(html.includes('./assets/console-menu.js?v=2'), 'Console launcher runtime must be linked');
assert.ok(html.includes('<title>FREEzzz ARCADE — Game Select</title>'), 'Startup page must be the game selector');
assert.ok(html.includes('./assets/skins.js?v=20'), 'Skin module must be linked');

const layers = ['class="shell-art"', 'class="shell-skin-art"', 'class="deck-tint"', 'class="screen-bezel"', 'class="controls"'];
let lastIndex = -1;
for (const layer of layers) {
  const index = html.indexOf(layer);
  assert.ok(index > lastIndex, `Layer order invalid at ${layer}`);
  lastIndex = index;
}
assert.equal((css.match(/\.controls\{position:absolute/g) || []).length, 1, 'Only one authoritative controls rule is allowed');
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
const layoutEditor = read('../assets/control-layout.js');
const shadowPage = read('../shadow-spire.html');
assert.ok(shadowPage.includes('./assets/control-layout.js?v=3'), 'SHADOW SPIRE control layout editor must load');
assert.ok(html.includes('./assets/console-menu.js?v=2'), 'Home page must load its launcher');
assert.ok(html.includes('class="menu-home"'), 'Startup page must use launcher layout');
const menu = read('../assets/console-menu.js');
assert.ok(menu.includes("href:'./shadow-spire.html'"), 'Launcher must offer SHADOW SPIRE');
assert.ok(menu.includes("href:'./night-runner.html'"), 'Launcher must offer NIGHT RUNNER');
assert.ok(css.includes('.menu-home .controls{display:block}'), 'Launcher must show physical console controls');
assert.ok(menu.includes("bindControl('left'"), 'Left console control must select a game');
assert.ok(menu.includes("bindControl('right'"), 'Right console control must select a game');
assert.ok(menu.includes("bindControl('jump'"), 'Action console control must launch selected game');
const racerPage = read('../night-runner.html');
const racer = read('../assets/night-runner.js');
assert.ok(racerPage.includes('<title>NIGHT RUNNER</title>'), 'Racing game page must be identifiable');
assert.ok(racerPage.includes('./assets/night-runner.js?v=6'), 'Racing game runtime must load');
assert.ok(racerPage.includes('href="./index.html"'), 'Racing game must link back to the main menu');
assert.ok(shadowPage.includes('href="./index.html"'), 'SHADOW SPIRE must link back to the main menu');
assert.ok(racer.includes('canvas.width=W;canvas.height=H'), 'Racing canvas must use the shared logical resolution');
assert.ok(racer.includes("canvas.style.objectFit='contain'"), 'Racing canvas must preserve pixel aspect ratio');
assert.ok(racer.includes("canvas.style.imageRendering='pixelated'"), 'Racing canvas must render crisp enlarged pixels');
assert.ok(racer.includes('ctx.globalAlpha=Math.min(.2,flash)'), 'Crash flash must not wash out the entire screen');
assert.ok(racer.includes('Wheel silhouettes, chassis, windshield, roof and headlights'), 'Racing cars must use detailed pixel sprites');
assert.ok(racer.includes('nightRunnerBest_v1'), 'Racing game must persist high score');
assert.ok(racer.includes("bind('left');bind('right');bind('jump')"), 'Racing game must use console controls');
assert.ok(racer.includes('function spawnCar()'), 'Racing game must spawn traffic');
assert.ok(racer.includes('function spawnPickup()'), 'Racing game must spawn collectible pickups');
assert.ok(racer.includes('function spawnHazard()'), 'Racing game must spawn road hazards');
assert.ok(racer.includes('function nearMiss(o)'), 'Racing game must reward near misses');
assert.ok(racer.includes("type==='nitro'"), 'Nitro pickups must refill boost');
assert.ok(racer.includes("skid=1.15"), 'Oil slicks must cause temporary steering loss');
assert.ok(racer.includes('level=1+Math.floor(elapsed/18)'), 'Traffic difficulty must scale over time');
assert.ok(racer.includes("mode='crash'"), 'Racing game must detect collisions');
assert.ok(racer.includes("text('RUN OVER'"), 'Crash screen must use the revised run-over layout');
assert.ok(racer.includes("ctx.lineTo(87,190)"), 'Title road must not overlap the instruction block');
assert.ok(layoutEditor.includes("const buttons=[...controls.querySelectorAll('[data-control]')]"), 'Editor must support each control independently');
assert.ok(layoutEditor.includes("trigger.addEventListener('click'"), 'Visible settings button must activate layout editing');
assert.ok(!layoutEditor.includes("document.addEventListener('dblclick'"), 'Legacy double-tap activation must be removed');
assert.ok(layoutEditor.includes("'resize':'move'"), 'Controls must support moving and resizing');
assert.ok(layoutEditor.includes('shadowSpireControlLayout_v1'), 'Control layout must persist');

assert.ok(skins.includes('.style-option[data-shell-skin]') && skins.includes('.style-option[data-pad-skin]'), 'Skin handlers must bind only to options');
for (const fn of ['rs','pl','stl','up','rd','loop','onDown','onUp','onMove','onCancel','onKD','onKU','dMenu','dHUD']) {
  assert.ok(new RegExp(`function\\s+${fn}\\s*\\(`).test(game), `Missing runtime function: ${fn}`);
}
assert.ok(screenContract.includes('width: 208, height: 320'), 'Shared game screen must stay 208x320');
assert.ok(shadowPage.includes('./assets/game-screen.js?v=1'), 'Shared game screen contract must load before SHADOW SPIRE');
assert.ok(racerPage.includes('./assets/game-screen.js?v=1'), 'Shared game screen contract must load before NIGHT RUNNER');
assert.ok(game.includes('window.CONSOLE_GAME_SCREEN'), 'Game must use the shared screen contract');
assert.ok(game.includes('function makeTowerRows'), 'Levels must be generated for the fixed portrait game screen');
const generatorMatch = game.match(/function makeTowerRows\(seed,style\)\{[\s\S]*?\n\}/);
assert.ok(generatorMatch, 'Portrait level generator must be inspectable for reachability checks');
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
const makeTowerRows = new Function('cl', `${generatorMatch[0]}; return makeTowerRows;`)(clamp);
assert.ok((game.match(/\{n:'/g) || []).length >= 20, 'Expected 20 designed levels');
for (let seed = 0; seed < 20; seed++) {
  const rows = makeTowerRows(seed, seed % 3);
  assert.equal(rows.length, 28, `Level ${seed + 1} must have 28 rows`);
  assert.ok(rows.every(row => row.length === 13), `Level ${seed + 1} rows must align to the 13-tile grid`);
  assert.ok(rows[26].includes('S'), `Level ${seed + 1} must have a spawn`);
  assert.ok(rows[2].includes('G'), `Level ${seed + 1} must have an exit`);
  assert.ok(rows[17].includes('P'), `Level ${seed + 1} must have a checkpoint`);
  assert.ok(rows[27] === '#'.repeat(13), `Level ${seed + 1} must have a continuous starting floor`);
  const platforms = rows.map((row, y) => {
    const xs = [...row].map((ch, x) => '#:=P'.includes(ch) ? x : -1).filter(x => x >= 0);
    return xs.length ? { y, left: Math.min(...xs), right: Math.max(...xs) } : null;
  }).filter(Boolean).filter(p => p.y < 27);
  assert.ok(platforms.length >= 12, `Level ${seed + 1} needs enough climbing platforms`);
  for (let i = 1; i < platforms.length; i++) {
    const a = platforms[i - 1], b = platforms[i];
    assert.ok((b.y - a.y) * 16 <= (250 * 250) / (2 * 700) + 1, `Level ${seed + 1} has a vertical jump beyond normal jump height`);
    const horizontalGap = Math.max(0, b.left - a.right - 1, a.left - b.right - 1) * 16;
    assert.ok(horizontalGap <= 112 * (2 * 250 / 700) + 9, `Level ${seed + 1} has an excessive horizontal platform gap`);
  }
}
for (const [feature,pattern] of [
  ['coyote-time jump forgiveness', 'p.coyote>0'],
  ['jump input buffering', 'p.jumpBuf=.14'],
  ['variable jump height', 'p.vy-=GR*.22*dt'],
  ['double jump', 'p.jumpsUsed<2'],
  ['wall jump', 'wallContact!==0'],
  ['wall slide', 'p.wallDir!==0&&!p.onGround&&p.vy>95'],
  ['double-tap dash', 'now-dirTap.right<280'],
  ['dash damage grace', 'if(p.dashT>0)return'],
  ['high-contrast level label', 'cx.fillRect(2,2,Math.min(VW-4,tw(title,1)+10),11)'],
  ['on-screen control tutorial', "dtc('DOUBLE TAP L/R: DASH'"]
]) assert.ok(game.includes(pattern), `Missing gameplay feature: ${feature}`);

assert.ok(game.includes('requestAnimationFrame(loop)'), 'Game loop must be active');
assert.ok(game.includes('localStorage'), 'Game progress persistence must remain present');

assert.ok(game.includes("n:'HEART OF THE SPIRE'"), 'Final level must be present');
assert.ok(game.includes("if(seed===19)"), 'Final level must require collecting its three memory crystals');
console.log('SHADOW SPIRE architecture checks passed.');
