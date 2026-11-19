(() => {
'use strict';
const $=id=>document.getElementById(id);
const canvas=$('view'),ctx=canvas.getContext('2d',{alpha:false});
const off=document.createElement('canvas'),offctx=off.getContext('2d',{willReadFrequently:true});
const GLYPHS=' .,:;!|/\\()[]{}itfx%#@';
const TILE=1024,STEP=960,RES=512;
const state={mode:'glyph',density:6,contrast:1.15,tiles:new Map(),camera:{x:0,y:0},busy:new Set(),drag:null,move:{x:0,y:0},seed:Math.floor(Math.random()*1e9),biome:'alpine',weather:'clear',day:0};
const key=(x,y)=>x+','+y,getTile=(x,y)=>state.tiles.get(key(x,y));
function status(s,error=false){$('status').textContent=s;$('status').style.color=error?'#ff9d91':'#ffc879';}
function updateCount(){$('tileCount').textContent=state.tiles.size+' областей';}
function hash(x,y,s=state.seed){let n=Math.imul(x|0,374761393)+Math.imul(y|0,668265263)+Math.imul(s|0,1442695041);n=Math.imul(n^(n>>>13),1274126177);return ((n^(n>>>16))>>>0)/4294967295;}
function smooth(t){return t*t*(3-2*t);}
function noise2(x,y,scale,seed){const fx=x/scale,fy=y/scale,ix=Math.floor(fx),iy=Math.floor(fy),u=smooth(fx-ix),v=smooth(fy-iy);const a=hash(ix,iy,seed),b=hash(ix+1,iy,seed),c=hash(ix,iy+1,seed),d=hash(ix+1,iy+1,seed);return (a+(b-a)*u)+((c+(d-c)*u)-(a+(b-a)*u))*v;}
function fbm(x,y,seed,scales=[180,85,38,17]){let sum=0,amp=.55,total=0;for(let i=0;i<scales.length;i++){sum+=noise2(x,y,scales[i],seed+i*101)*amp;total+=amp;amp*=.5;}return sum/total;}
function ridge(x,y,seed){const n=fbm(x,y,seed);return 1-Math.abs(n*2-1);}
function mix(a,b,t){return a+(b-a)*t;}
function color(a,b,t){return [mix(a[0],b[0],t),mix(a[1],b[1],t),mix(a[2],b[2],t)];}
function clamp(v,a=0,b=1){return Math.max(a,Math.min(b,v));}
function profile(x,seed,scale,base,amp){return base+(fbm(x,0,seed,[scale,scale*.47,scale*.2])-0.5)*amp;}
function palette(){return state.biome==='desert'?{sky1:[92,139,174],sky2:[232,184,126],mount:[111,91,77],rock:[143,111,79],ground:[128,105,68],tree:[84,88,55],water:[66,133,150]}:state.biome==='coast'?{sky1:[70,139,183],sky2:[211,222,217],mount:[105,133,139],rock:[104,120,116],ground:[92,119,92],tree:[49,91,75],water:[37,113,139]}:state.biome==='tundra'?{sky1:[77,128,166],sky2:[208,221,220],mount:[112,132,143],rock:[137,151,153],ground:[153,166,153],tree:[62,91,89],water:[63,121,139]}:{sky1:[53,105,148],sky2:[188,207,211],mount:[67,91,105],rock:[105,119,119],ground:[57,91,66],tree:[25,70,48],water:[48,122,145]};}
function makeTerrain(tx,ty){
 const c=document.createElement('canvas');c.width=RES;c.height=RES;const g=c.getContext('2d'),img=g.createImageData(RES,RES),d=img.data,p=palette();
 const ox=tx*STEP,oy=ty*STEP,worldScale=TILE/RES;
 const seed=state.seed,day=state.day,biome=state.biome;
 const sunX=.22+hash(tx,ty,seed)*.56,sunY=.15+hash(tx+71,ty-9,seed)*.17;
 const horizon=.34+(fbm(ox,oy,seed+2,[900,400,170])-.5)*.055;
 const ridgeFar=[],ridgeMid=[];
 for(let x=0;x<=RES;x++){const wx=ox+x*worldScale;ridgeFar[x]=horizon-.12-(fbm(wx,0,seed+17,[430,190,70])-.5)*.16;ridgeMid[x]=horizon+.02-(fbm(wx,0,seed+31,[270,115,44])-.5)*.13;}
 const riverX=.47+(fbm(ox,oy,seed+91,[1100,480,210])-.5)*.23;
 for(let y=0;y<RES;y++)for(let x=0;x<RES;x++){
  const wx=ox+x*worldScale,wy=oy+y*worldScale,n=fbm(wx,wy,seed+41),fine=fbm(wx,wy,seed+57,[31,13,6]),cloud=fbm(wx,wy,seed+72,[290,130,58]);
  const yn=y/RES,far=ridgeFar[x],mid=ridgeMid[x];let col;
  if(yn<far){
   const t=clamp(yn/Math.max(.01,far));col=color(p.sky1,p.sky2,t);
   const cloudMask=clamp((cloud-.57)*3.6)*clamp((far-yn)*13);
   col=color(col,[225,231,225],cloudMask*.46);
   const sunDist=Math.hypot((x/RES-sunX)*1.1,(yn-sunY)*1.5);
   col=color(col,[255,232,188],Math.pow(clamp(1-sunDist*5),3)*.2);
  }else if(yn<mid){
   const depth=clamp((yn-far)/Math.max(.01,mid-far));
   const snow=biome==='alpine'?clamp((n-.53)*5+depth*.4):0;
   col=color(p.mount,p.rock,clamp(n*.75+fine*.3));
   col=color(col,[194,204,199],snow*.72);
   const shade=clamp(.68+(.5-n)*.6+(fbm(wx+12,wy,seed+82)-.5)*.25);
   col=col.map(v=>v*shade);
   col=color(col,p.sky2,Math.pow(1-depth,2)*.35);
  }else{
   const depth=clamp((yn-mid)/(1-mid));
   const slope=Math.abs((ridgeMid[Math.min(RES,x+1)]-ridgeMid[Math.max(0,x-1)])*RES);
   const moisture=clamp(.68+(.5-n)*.55+fine*.25);
   if(biome==='desert'){col=color([157,126,82],[202,167,112],clamp(n*.9+depth*.2));}
   else if(biome==='tundra'){col=color([129,151,143],[207,214,201],clamp(n*.6+depth*.45));}
   else{col=color(p.ground,p.tree,clamp(moisture*.78+depth*.22));}
   const light=clamp(.72+(fine-.5)*.55+(slope*.05));
   col=col.map(v=>v*light);
   const riverDist=Math.abs(x/RES-(riverX+Math.sin(yn*7+fbm(wx,wy,seed+111)*2)*(.02+depth*.08)));
   if(biome!=='desert'&&riverDist<(.007+depth*.012)&&yn>.48){const bank=clamp((riverDist-(.004+depth*.008))/.012);col=color(p.water,col,bank*.8);col=color(col,[190,209,196],(1-bank)*(.15+fine*.18));}
   const rock=clamp((slope*.24+n*.3-.57)*3.5);col=color(col,p.rock,rock*.52);
   const texture=(fine-.5)*(biome==='desert'?28:20);col=col.map(v=>v+texture);
  }
  const grain=(hash(x+tx*RES,y+ty*RES,seed+303)-.5)*(yn>.42?7:3);
  const idx=(y*RES+x)*4;d[idx]=clamp((col[0]+grain)/255)*255;d[idx+1]=clamp((col[1]+grain)/255)*255;d[idx+2]=clamp((col[2]+grain)/255)*255;d[idx+3]=255;
 }
 g.putImageData(img,0,0);
 // Layered distant tree line and individually shaped conifers; positions derive from global coordinates.
 if(biome==='forest'||biome==='alpine'||biome==='coast'){
  for(let layer=0;layer<3;layer++){
   const count=layer===0?150:layer===1?100:65;
   for(let i=0;i<count;i++){
    const gx=tx*count+i,rx=hash(gx,ty+layer,seed+600+layer),ry=hash(gx+71,ty,seed+700+layer);
    const x=rx*RES,y=RES*(.58+ry*.43);const h=(8+ry*24)*(layer===0?.7:layer===1?1:1.5),w=h*(.24+hash(gx,ty,seed+800)*.16);
    if(y-h<RES*(horizon+.1))continue;
    const shade=hash(gx,ty,seed+900),base=layer===0?[23,62,49]:layer===1?[18,54,41]:[13,43,34];
    g.fillStyle='rgba(8,18,17,'+(.12+shade*.18)+')';g.beginPath();g.ellipse(x+2,y-1,w*1.2,h*.14,0,0,Math.PI*2);g.fill();
    for(let k=0;k<4;k++){const yy=y-h+k*h*.23,ww=w*(.18+k*.25);g.fillStyle='rgb('+Math.floor(base[0]+shade*15+k*2)+','+Math.floor(base[1]+shade*22+k*2)+','+Math.floor(base[2]+shade*10)+')';g.beginPath();g.moveTo(x,yy-h*.12);g.lineTo(x-ww,yy+h*.36);g.lineTo(x+ww,yy+h*.36);g.closePath();g.fill();}
    g.strokeStyle='rgba(128,111,80,.35)';g.lineWidth=Math.max(.6,w*.1);g.beginPath();g.moveTo(x,y-h*.08);g.lineTo(x,y);g.stroke();
   }
  }
 }
 // Fine atmospheric haze and subtle lens-like light falloff.
 const haze=g.createLinearGradient(0,RES*.27,0,RES*.68);haze.addColorStop(0,'rgba(210,224,222,.16)');haze.addColorStop(.45,'rgba(190,210,205,.06)');haze.addColorStop(1,'rgba(0,0,0,0)');g.fillStyle=haze;g.fillRect(0,0,RES,RES);
 const vignette=g.createRadialGradient(RES*.5,RES*.45,RES*.2,RES*.5,RES*.5,RES*.78);vignette.addColorStop(0,'rgba(0,0,0,0)');vignette.addColorStop(1,'rgba(5,12,12,.25)');g.fillStyle=vignette;g.fillRect(0,0,RES,RES);
 return c;
}
function addTile(x,y){const id=key(x,y);if(state.tiles.has(id)||state.busy.has(id))return Promise.resolve();state.busy.add(id);status('Локально строю территорию '+id+'…');return new Promise(resolve=>{requestAnimationFrame(()=>{try{const image=makeTerrain(x,y);state.tiles.set(id,{image,x,y});updateCount();render();status('Область '+id+' создана локальным движком. Интернет не используется.');}catch(e){status('Ошибка генерации: '+e.message,true);}finally{state.busy.delete(id);resolve();}});});}
async function generateStart(){if(state.tiles.size){status('Мир уже существует. Нажми «НОВЫЙ МИР» для другого seed.');return;}await addTile(0,0);state.camera.x=STEP/2;state.camera.y=STEP/2;render();}
function tileAt(wx,wy){return {x:Math.floor(wx/STEP),y:Math.floor(wy/STEP)};}
function prefetch(){const p=tileAt(state.camera.x,state.camera.y),localX=state.camera.x-p.x*STEP,localY=state.camera.y-p.y*STEP;const dirs=[];if(localX<220)dirs.push([-1,0]);if(localX>TILE-220)dirs.push([1,0]);if(localY<220)dirs.push([0,-1]);if(localY>TILE-220)dirs.push([0,1]);for(const [dx,dy] of dirs){const x=p.x+dx,y=p.y+dy;if(!getTile(x,y)&&!state.busy.has(key(x,y))){addTile(x,y);break;}}}
function resize(){const r=canvas.getBoundingClientRect(),d=Math.min(devicePixelRatio||1,1.5),w=Math.max(1,Math.round(r.width*d)),h=Math.max(1,Math.round(r.height*d));if(canvas.width!==w||canvas.height!==h){canvas.width=w;canvas.height=h;render();}}
function render(){
 const w=canvas.width,h=canvas.height;if(!w||!h)return;ctx.fillStyle='#05090b';ctx.fillRect(0,0,w,h);
 if(!state.tiles.size){ctx.fillStyle='#8fffe0';ctx.font='14px ui-monospace,monospace';ctx.textAlign='center';ctx.fillText('LOCAL WORLD ENGINE / READY',w/2,h/2);return;}
 const scale=Math.min(w/650,h/820),vw=w/scale,vh=h/scale,left=state.camera.x-vw/2,top=state.camera.y-vh/2;
 const visible=[...state.tiles.values()].filter(t=>t.x*STEP+TILE>left&&t.x*STEP<left+vw&&t.y*STEP+TILE>top&&t.y*STEP<top+vh);
 for(const t of visible)ctx.drawImage(t.image,(t.x*STEP-left)*scale,(t.y*STEP-top)*scale,TILE*scale,TILE*scale);
 if(state.mode==='glyph'){
  const cell=Math.max(3,state.density),cols=Math.ceil(w/cell),rows=Math.ceil(h/cell);off.width=cols;off.height=rows;offctx.drawImage(canvas,0,0,cols,rows);const px=offctx.getImageData(0,0,cols,rows).data;
  ctx.fillStyle='#05090b';ctx.fillRect(0,0,w,h);ctx.font=(cell*1.12)+'px monospace';ctx.textBaseline='top';ctx.textAlign='left';
  for(let y=0;y<rows;y++)for(let x=0;x<cols;x++){const i=(y*cols+x)*4,r=px[i],gg=px[i+1],b=px[i+2],lum=(.2126*r+.7152*gg+.0722*b)/255,prev=x?(.2126*px[i-4]+.7152*px[i-3]+.0722*px[i-2])/255:lum,next=x<cols-1?(.2126*px[i+4]+.7152*px[i+5]+.0722*px[i+6])/255:lum,edge=Math.min(1,Math.abs(next-prev)*2.8),tone=clamp(lum*state.contrast),idx=Math.min(GLYPHS.length-1,Math.floor((tone*.82+edge*.18)*(GLYPHS.length-1)));ctx.fillStyle='rgba('+r+','+gg+','+b+','+(.48+tone*.52)+')';ctx.fillText(GLYPHS[idx],x*cell,y*cell);}
 }
 $('worldInfo').textContent='SEED '+state.seed+' · '+state.biome.toUpperCase()+' · '+state.tiles.size+' REGIONS';
}
function move(dx,dy){if(!state.tiles.size)return;const nx=state.camera.x+dx*34,ny=state.camera.y+dy*34,p=tileAt(nx,ny);if(!getTile(p.x,p.y)){addTile(p.x,p.y);return;}state.camera.x=nx;state.camera.y=ny;render();prefetch();}
function stopMove(){state.move={x:0,y:0};if(state.moveFrame)cancelAnimationFrame(state.moveFrame);}
function startMove(dx,dy){stopMove();state.move={x:dx,y:dy};move(dx,dy);let last=performance.now();const tick=t=>{if(!state.move.x&&!state.move.y)return;if(t-last>95){move(state.move.x,state.move.y);last=t;}state.moveFrame=requestAnimationFrame(tick);};state.moveFrame=requestAnimationFrame(tick);}
$('generate').addEventListener('click',generateStart);
$('density').addEventListener('input',e=>{$('densityOut').textContent=e.target.value+' px';state.density=+e.target.value;render();});
$('contrast').addEventListener('input',e=>{$('contrastOut').textContent=(+e.target.value).toFixed(2);state.contrast=+e.target.value;render();});
$('glyph').addEventListener('click',()=>{state.mode='glyph';$('glyph').classList.add('primary');$('photo').classList.remove('primary');render();});
$('photo').addEventListener('click',()=>{state.mode='photo';$('photo').classList.add('primary');$('glyph').classList.remove('primary');render();});
$('reset').addEventListener('click',()=>{state.camera.x=STEP/2;state.camera.y=STEP/2;render();});
$('nextRegion').addEventListener('click',()=>{const p=tileAt(state.camera.x,state.camera.y);addTile(p.x+1,p.y);});
$('newWorld').addEventListener('click',()=>{state.seed=Math.floor(Math.random()*1e9);state.tiles.clear();state.camera={x:STEP/2,y:STEP/2};generateStart();});
$('biome').addEventListener('change',e=>{state.biome=e.target.value;state.tiles.clear();state.camera={x:STEP/2,y:STEP/2};generateStart();});
const dirs={up:[0,-1],down:[0,1],left:[-1,0],right:[1,0]};
document.querySelectorAll('[data-move]').forEach(b=>{const [dx,dy]=dirs[b.dataset.move];b.addEventListener('pointerdown',e=>{e.preventDefault();startMove(dx,dy);});['pointerup','pointercancel','pointerleave'].forEach(ev=>b.addEventListener(ev,stopMove));});
window.addEventListener('keydown',e=>{const m={arrowleft:[-1,0],a:[-1,0],arrowright:[1,0],d:[1,0],arrowup:[0,-1],w:[0,-1],arrowdown:[0,1],s:[0,1]}[e.key.toLowerCase()];if(m&&!e.repeat){e.preventDefault();startMove(...m);}});
window.addEventListener('keyup',e=>{if(['arrowleft','arrowright','arrowup','arrowdown','a','d','w','s'].includes(e.key.toLowerCase()))stopMove();});
canvas.addEventListener('pointerdown',e=>{stopMove();state.drag={x:e.clientX,y:e.clientY,cx:state.camera.x,cy:state.camera.y};canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener('pointermove',e=>{if(!state.drag)return;const r=canvas.getBoundingClientRect(),scale=Math.min(canvas.width/650,canvas.height/820);state.camera.x=state.drag.cx-(e.clientX-state.drag.x)/scale;state.camera.y=state.drag.cy-(e.clientY-state.drag.y)/scale;render();prefetch();});
canvas.addEventListener('pointerup',()=>state.drag=null);canvas.addEventListener('pointercancel',()=>state.drag=null);
new ResizeObserver(resize).observe(canvas);resize();updateCount();status('Локальный движок готов. Никаких сетевых запросов.');
})();