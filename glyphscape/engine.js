(() => {
'use strict';
const $=id=>document.getElementById(id);
const canvas=$('view'),ctx=canvas.getContext('2d',{alpha:false});
const sampleCanvas=document.createElement('canvas'),sampleCtx=sampleCanvas.getContext('2d',{willReadFrequently:true});
const GLYPHS=' .,:;!|/\\()[]{}itfx%#@';
const TILE=1024, STEP=960, FEATHER=72;
const state={mode:'glyph',density:6,contrast:1.15,tiles:new Map(),camera:{x:0,y:0},busy:new Set(),token:sessionStorage.getItem('glyphscape_token')||'',appKey:localStorage.getItem('glyphscape_app_key')||'',drag:null,move:{x:0,y:0},lastMove:0,worldPrompt:'',seed:Math.floor(Math.random()*1e9),autoQueued:new Set()};
const key=(x,y)=>x+','+y, getTile=(x,y)=>state.tiles.get(key(x,y));
const redirectUri=location.origin+location.pathname;
$('redirectText').textContent=redirectUri;$('appKey').value=state.appKey;
function status(s,error=false){$('status').textContent=s;$('status').style.color=error?'#ff9d91':'#ffc879';}
function setConnection(){$('connection').textContent=state.token?'AI CONNECTED · SESSION':'NOT CONNECTED';$('generate').disabled=!state.token||state.busy.size>0;}
function updateCount(){$('tileCount').textContent=state.tiles.size+' областей';}
function b64(bytes){return btoa(String.fromCharCode(...new Uint8Array(bytes))).replace(/\\+/g,'-').replace(/\\//g,'_').replace(/=+$/,'');}
function randomString(n=48){const a=new Uint8Array(n);crypto.getRandomValues(a);return b64(a);}
async function sha256(s){return b64(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(s)));}
async function connect(){
 const app=$('appKey').value.trim();if(!/^pk_[A-Za-z0-9_-]+$/.test(app)){status('Нужен публичный App Key с префиксом pk_.',true);return;}
 state.appKey=app;localStorage.setItem('glyphscape_app_key',app);
 const verifier=randomString(),csrf=randomString(24),challenge=await sha256(verifier);
 sessionStorage.setItem('glyphscape_verifier',verifier);sessionStorage.setItem('glyphscape_state',csrf);sessionStorage.setItem('glyphscape_client',app);
 const u=new URL('https://enter.pollinations.ai/authorize');
 Object.entries({response_type:'code',client_id:app,redirect_uri:redirectUri,scope:'profile',state:csrf,code_challenge:challenge,code_challenge_method:'S256',models:'image',budget:'2',expiry:'7'}).forEach(([k,v])=>u.searchParams.set(k,v));
 location.assign(u.toString());
}
async function callback(){
 const p=new URLSearchParams(location.search),code=p.get('code');if(!code)return;
 if(p.get('state')!==sessionStorage.getItem('glyphscape_state')){status('OAuth state mismatch. Повтори подключение.',true);history.replaceState({},'',redirectUri);return;}
 try{
  const res=await fetch('https://enter.pollinations.ai/api/oauth/token',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({grant_type:'authorization_code',code,client_id:sessionStorage.getItem('glyphscape_client'),redirect_uri:redirectUri,code_verifier:sessionStorage.getItem('glyphscape_verifier')})});
  const d=await res.json();if(!res.ok||!d.access_token)throw Error(d.error_description||d.error||'OAuth HTTP '+res.status);
  state.token=d.access_token;sessionStorage.setItem('glyphscape_token',state.token);status('AI подключён. Можно создавать территории.');
 }catch(e){status('Ошибка авторизации: '+e.message,true);}
 finally{['glyphscape_verifier','glyphscape_state','glyphscape_client'].forEach(k=>sessionStorage.removeItem(k));history.replaceState({},'',redirectUri);setConnection();}
}
function tilePrompt(x,y){
 const base=$('prompt').value.trim();
 let continuity='';
 const neighbors=[['north',0,-1],['east',1,0],['south',0,1],['west',-1,0]].filter(([,dx,dy])=>getTile(x+dx,y+dy));
 if(neighbors.length)continuity=' This is a neighboring region in the SAME continuous fictional landscape. Maintain matching biome, season, time of day, weather, color grading, horizon height and terrain scale. Directional continuation: '+neighbors.map(([n])=>n).join(', ')+'. Landscape features should flow naturally across the corresponding image borders; avoid a new unrelated scene.';
 else continuity=' Establish the first region of a coherent explorable landscape with a consistent horizon and natural terrain.';
 const direction=x===0&&y===0?'central starting region':(x>0?'east ':x<0?'west ':'')+(y>0?'south ':y<0?'north ':'')+'neighbor region';
 return base+'. '+direction+'. '+continuity+' Wide environmental landscape photograph, photorealistic documentary photography, physically plausible terrain, detailed natural textures, natural atmospheric perspective, consistent realistic lighting, no illustration, no painting, no symbols, no text, no borders, no collage.';
}
function featherImage(bitmap){
 const c=document.createElement('canvas');c.width=bitmap.width;c.height=bitmap.height;
 const cctx=c.getContext('2d',{willReadFrequently:true});cctx.drawImage(bitmap,0,0);
 const im=cctx.getImageData(0,0,c.width,c.height),d=im.data,w=c.width,h=c.height;
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){
  const edge=Math.min(x,y,w-1-x,h-1-y),a=Math.max(0,Math.min(1,edge/FEATHER));
  d[(y*w+x)*4+3]=Math.round(d[(y*w+x)*4+3]*a);
 }
 cctx.putImageData(im,0,0);return c;
}
async function generateTile(x,y,manual=false){
 const id=key(x,y);if(state.tiles.has(id)||state.busy.has(id)||!state.token)return;
 state.busy.add(id);setConnection();status('AI создаёт область '+id+'…');
 const url=new URL('https://gen.pollinations.ai/image/'+encodeURIComponent(tilePrompt(x,y)));
 url.searchParams.set('model',$('model').value);url.searchParams.set('width',String(TILE));url.searchParams.set('height',String(TILE));url.searchParams.set('seed',String((state.seed+x*73856093+y*19349663)>>>0));
 try{
  const res=await fetch(url,{headers:{Authorization:'Bearer '+state.token}});
  if(!res.ok){let msg='';try{const d=await res.json();msg=d.detail||d.error||d.message||'';}catch{}throw Error('HTTP '+res.status+(msg?' — '+msg:''));}
  const blob=await res.blob();if(!blob.type.startsWith('image/'))throw Error('API вернул не изображение');
  const bitmap=await createImageBitmap(blob);
  const image=featherImage(bitmap);
  state.tiles.set(id,{image,x,y,created:Date.now()});updateCount();render();
  $('worldInfo').textContent='WORLD '+state.seed+' · TILE '+id+' · '+state.tiles.size+' REGIONS';
  status('Область '+id+' создана. Исследуй её — соседние участки появятся по мере движения.');
 }catch(e){status('Не удалось создать область '+id+': '+e.message,true);}
 finally{state.busy.delete(id);setConnection();}
}
async function generateStart(){if(!state.token){status('Сначала подключи Pollinations.',true);return;}if(state.tiles.size)return;state.worldPrompt=$('prompt').value.trim();await generateTile(0,0,true);if(state.tiles.has('0,0')){state.camera.x=TILE/2;state.camera.y=TILE/2;render();}}
function tileAtWorld(wx,wy){return {x:Math.floor(wx/STEP),y:Math.floor(wy/STEP)};}
function prefetch(){
 const p=tileAtWorld(state.camera.x,state.camera.y),localX=state.camera.x-p.x*STEP,localY=state.camera.y-p.y*STEP;
 const dirs=[];if(localX<260)dirs.push([-1,0]);if(localX>TILE-260)dirs.push([1,0]);if(localY<260)dirs.push([0,-1]);if(localY>TILE-260)dirs.push([0,1]);
 for(const [dx,dy] of dirs){const x=p.x+dx,y=p.y+dy,id=key(x,y);if(!getTile(x,y)&&!state.autoQueued.has(id)&&state.busy.size<1){state.autoQueued.add(id);generateTile(x,y).finally(()=>state.autoQueued.delete(id));break;}}
}
function resize(){const r=canvas.getBoundingClientRect(),d=Math.min(devicePixelRatio||1,1.5),w=Math.max(1,Math.round(r.width*d)),h=Math.max(1,Math.round(r.height*d));if(canvas.width!==w||canvas.height!==h){canvas.width=w;canvas.height=h;render();}}
function render(){
 const w=canvas.width,h=canvas.height;if(!w||!h)return;
 ctx.fillStyle='#05090b';ctx.fillRect(0,0,w,h);
 if(!state.tiles.size){ctx.fillStyle='#8fffe0';ctx.font='14px ui-monospace,monospace';ctx.textAlign='center';ctx.fillText('CONNECT AI  /  GENERATE WORLD',w/2,h/2);return;}
 const scale=Math.min(w/650,h/820),viewW=w/scale,viewH=h/scale,left=state.camera.x-viewW/2,top=state.camera.y-viewH/2;
 const visible=[...state.tiles.values()].filter(t=>(t.x*STEP+TILE)>left&&(t.x*STEP)<left+viewW&&(t.y*STEP+TILE)>top&&(t.y*STEP)<top+viewH).sort((a,b)=>Math.hypot(b.x*STEP+512-state.camera.x,b.y*STEP+512-state.camera.y)-Math.hypot(a.x*STEP+512-state.camera.x,a.y*STEP+512-state.camera.y));
 for(const t of visible){const dx=(t.x*STEP-left)*scale,dy=(t.y*STEP-top)*scale;ctx.drawImage(t.image,dx,dy,TILE*scale,TILE*scale);}
 if(state.mode==='glyph'){
  const cell=Math.max(3,Number(state.density)),cols=Math.ceil(w/cell),rows=Math.ceil(h/cell);sampleCanvas.width=cols;sampleCanvas.height=rows;sampleCtx.drawImage(canvas,0,0,cols,rows);
  const px=sampleCtx.getImageData(0,0,cols,rows).data,font=cell*1.12;ctx.clearRect(0,0,w,h);ctx.fillStyle='#05090b';ctx.fillRect(0,0,w,h);ctx.font=font+'px monospace';ctx.textBaseline='top';ctx.textAlign='left';
  for(let y=0;y<rows;y++)for(let x=0;x<cols;x++){const i=(y*cols+x)*4,r=px[i],g=px[i+1],b=px[i+2],lum=(.2126*r+.7152*g+.0722*b)/255,prev=x?(.2126*px[i-4]+.7152*px[i-3]+.0722*px[i-2])/255:lum,next=x<cols-1?(.2126*px[i+4]+.7152*px[i+5]+.0722*px[i+6])/255:lum,edge=Math.min(1,Math.abs(next-prev)*2.8),tone=Math.max(0,Math.min(1,lum*state.contrast)),idx=Math.min(GLYPHS.length-1,Math.floor((tone*.82+edge*.18)*(GLYPHS.length-1)));ctx.fillStyle='rgba('+r+','+g+','+b+','+(.48+tone*.52)+')';ctx.fillText(GLYPHS[idx],x*cell,y*cell);}
 }
}
function move(dx,dy){if(!state.tiles.size)return;const nx=state.camera.x+dx*42,ny=state.camera.y+dy*42,p=tileAtWorld(nx,ny);if(!getTile(p.x,p.y)){generateTile(p.x,p.y);return;}state.camera.x=nx;state.camera.y=ny;render();prefetch();}
function startMove(dx,dy){stopMove();state.move={x:dx,y:dy};move(dx,dy);state.lastMove=performance.now();state.moveFrame=requestAnimationFrame(moveLoop);}
function moveLoop(t){if(!state.move.x&&!state.move.y)return;if(t-state.lastMove>115){move(state.move.x,state.move.y);state.lastMove=t;}state.moveFrame=requestAnimationFrame(moveLoop);}
function stopMove(){state.move={x:0,y:0};if(state.moveFrame)cancelAnimationFrame(state.moveFrame);}
$('generate').addEventListener('click',generateStart);$('connect').addEventListener('click',connect);
$('disconnect').addEventListener('click',()=>{state.token='';sessionStorage.removeItem('glyphscape_token');setConnection();status('AI отключён.');});
$('nextRegion').addEventListener('click',()=>{if(!state.tiles.size)return;const p=tileAtWorld(state.camera.x,state.camera.y);generateTile(p.x+1,p.y,true);});
$('density').addEventListener('input',e=>{$('densityOut').textContent=e.target.value+' px';state.density=+e.target.value;render();});
$('contrast').addEventListener('input',e=>{$('contrastOut').textContent=(+e.target.value).toFixed(2);state.contrast=+e.target.value;render();});
$('glyph').addEventListener('click',()=>{state.mode='glyph';$('glyph').classList.add('primary');$('photo').classList.remove('primary');render();});
$('photo').addEventListener('click',()=>{state.mode='photo';$('photo').classList.add('primary');$('glyph').classList.remove('primary');render();});
$('reset').addEventListener('click',()=>{state.camera.x=512;state.camera.y=512;render();});
const dirs={up:[0,-1],down:[0,1],left:[-1,0],right:[1,0]};
document.querySelectorAll('[data-move]').forEach(b=>{const [dx,dy]=dirs[b.dataset.move];b.addEventListener('pointerdown',e=>{e.preventDefault();startMove(dx,dy);});['pointerup','pointercancel','pointerleave'].forEach(ev=>b.addEventListener(ev,stopMove));});
window.addEventListener('keydown',e=>{const m={arrowleft:[-1,0],a:[-1,0],arrowright:[1,0],d:[1,0],arrowup:[0,-1],w:[0,-1],arrowdown:[0,1],s:[0,1]}[e.key.toLowerCase()];if(m&&!e.repeat){e.preventDefault();startMove(...m);}});
window.addEventListener('keyup',e=>{if(['arrowleft','arrowright','arrowup','arrowdown','a','d','w','s'].includes(e.key.toLowerCase()))stopMove();});
canvas.addEventListener('pointerdown',e=>{stopMove();state.drag={x:e.clientX,y:e.clientY,cx:state.camera.x,cy:state.camera.y};canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener('pointermove',e=>{if(!state.drag)return;const r=canvas.getBoundingClientRect(),scale=Math.min(canvas.width/650,canvas.height/820);state.camera.x=state.drag.cx-(e.clientX-state.drag.x)/scale;state.camera.y=state.drag.cy-(e.clientY-state.drag.y)/scale;render();prefetch();});
canvas.addEventListener('pointerup',()=>state.drag=null);canvas.addEventListener('pointercancel',()=>state.drag=null);
new ResizeObserver(resize).observe(canvas);resize();callback();setConnection();updateCount();
})();