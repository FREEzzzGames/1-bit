import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
const read = (path) => readFileSync(new URL(path, import.meta.url), 'utf8');
const home=read('../index.html'), css=read('../assets/console.css'), layout=read('../assets/control-layout.js');
const menu=read('../assets/console-menu.js'), extras=read('../assets/arcade-extras.js');
const screen=read('../assets/game-screen.js'), shadow=read('../shadow-spire.html'), night=read('../night-runner.html');
assert.ok(home.includes('<title>FREEzzz ARCADE — Game Select</title>'),'Home must start at launcher');
assert.ok(home.includes('./assets/console-menu.js?v=4'),'Launcher runtime cache must be current');
assert.ok(home.includes('./assets/console.css?v=26'),'Shared console CSS must be current');
assert.ok(shadow.includes('./assets/game.js?v=33'),'Shadow Spire gameplay cache must be current');
assert.ok(shadow.includes('./assets/pixel-art.js?v=5'),'Shadow Spire pixel-art cache must be current');
const game=read('../assets/game.js');
assert.ok(game.includes('const routes=['),'Shadow Spire must define distinct level route archetypes');
assert.ok(game.includes('function dSpring(')&&game.includes("else if(ch==='B')ti[idx]=7"),'Spring mechanics must be parsed and rendered');
assert.ok(game.includes("o.type==='plat'&&o.ax!==undefined"),'Moving platform simulation must exist');
assert.ok((game.match(/\{n:'/g)||[]).length>=20,'Shadow Spire must retain 20 levels');
assert.ok(screen.includes('width: 208, height: 320'),'Logical screen must stay 208x320');
assert.ok(css.includes('.screen-bezel{position:absolute'),'Display aperture must remain fixed');
assert.ok(layout.includes("const KEY='shadowSpireControlLayout_v2'"),'Control layout must persist in current schema');
assert.ok(layout.includes("const hardware=[...controls.querySelectorAll('[data-control]')]"),'All hardware buttons must be editable');
assert.ok(!menu.includes("canvas.addEventListener('pointerdown'"),'Launcher canvas must not accept touch');
for(const href of ['./shadow-spire.html','./night-runner.html','./void-hunter.html','./iron-depths.html','./neon-drift.html']) assert.ok(menu.includes(href),'Missing launcher entry '+href);
for(const [page,title,slug] of [
 ['../void-hunter.html','VOID HUNTER','void'],
 ['../iron-depths.html','IRON DEPTHS','iron'],
 ['../neon-drift.html','NEON DRIFT','neon']
]){
 const html=read(page);
 assert.ok(html.includes('<title>'+title+'</title>'),title+' title missing');
 assert.ok(html.includes('data-game="'+slug+'"'),title+' mode missing');
 assert.ok(html.includes('./assets/game-screen.js?v=1'),title+' must use fixed screen contract');
 assert.ok(html.includes('./assets/arcade-extras.js?v=4'),title+' runtime cache must be current');
 assert.ok(html.includes('href="./index.html"'),title+' must link back to launcher');
 assert.ok(html.includes('data-control="left"')&&html.includes('data-control="right"')&&html.includes('data-control="jump"'),title+' must expose physical buttons');
}
assert.ok(extras.includes("mode==='void'")&&extras.includes("mode==='iron'")&&extras.includes("mode==='neon'"),'All three new game modes must exist');
assert.ok(!extras.includes("canvas.addEventListener('pointerdown'"),'New game canvas must be display-only');
assert.ok(menu.includes("bind('left'")&&menu.includes("bind('right'")&&menu.includes("bind('jump'"),'Launcher must use console buttons');
assert.ok(!read('../assets/game.js').includes("cv.addEventListener('pointerdown'"),'SHADOW SPIRE canvas must be display-only');
assert.ok(!read('../assets/night-runner.js').includes("canvas.addEventListener('pointerdown'"),'NIGHT RUNNER canvas must be display-only');
assert.ok(shadow.includes('./assets/game-screen.js?v=1')&&night.includes('./assets/game-screen.js?v=1'),'Existing games must use shared display contract');
console.log('Console architecture checks passed.');
