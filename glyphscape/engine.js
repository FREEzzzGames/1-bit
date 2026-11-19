(() => {
'use strict';
const $ = id => document.getElementById(id);
const canvas = $('view'), ctx = canvas.getContext('2d', {alpha:false});
const sampleCanvas = document.createElement('canvas'), sampleCtx = sampleCanvas.getContext('2d', {willReadFrequently:true});
const GLYPHS = ' .,:;!|/\\()[]{}itfx%#@';
const state = { image:null, mode:'glyph', density:6, contrast:1.15, cameraX:.5, cameraY:.5, busy:false, token:sessionStorage.getItem('glyphscape_token')||'', appKey:localStorage.getItem('glyphscape_app_key')||'', drag:null, seed:0 };
const redirectUri = location.origin + location.pathname;
$('redirectText').textContent = redirectUri;
$('appKey').value = state.appKey;
function status(message, error=false){ $('status').textContent=message; $('status').style.color=error?'#ff9d91':'#ffc879'; }
function setConnection(){ $('connection').textContent=state.token?'AI CONNECTED · SESSION':'NOT CONNECTED'; $('generate').disabled=!state.token||state.busy; }
function base64url(bytes){return btoa(String.fromCharCode(...new Uint8Array(bytes))).replace(/\+/g,'-').replace(/\//g,'_').replace(/=+$/,'');}
function randomString(n=48){const a=new Uint8Array(n);crypto.getRandomValues(a);return base64url(a);}
async function sha256(s){return base64url(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(s)));}
async function connect(){
 const key=$('appKey').value.trim();
 if(!/^pk_[A-Za-z0-9_-]+$/.test(key)){status('Нужен App Key с префиксом pk_. Не используй sk_-ключ.',true);return;}
 state.appKey=key;localStorage.setItem('glyphscape_app_key',key);
 const verifier=randomString(), challenge=await sha256(verifier), csrf=randomString(24);
 sessionStorage.setItem('glyphscape_verifier',verifier);sessionStorage.setItem('glyphscape_state',csrf);sessionStorage.setItem('glyphscape_client',key);
 const url=new URL('https://enter.pollinations.ai/authorize');
 Object.entries({response_type:'code',client_id:key,redirect_uri:redirectUri,scope:'profile',state:csrf,code_challenge:challenge,code_challenge_method:'S256',models:'image',budget:'2',expiry:'7'}).forEach(([k,v])=>url.searchParams.set(k,v));
 location.assign(url.toString());
}
async function handleCallback(){
 const params=new URLSearchParams(location.search), code=params.get('code');
 if(!code)return;
 const returnedState=params.get('state'), expected=sessionStorage.getItem('glyphscape_state');
 if(!returnedState||returnedState!==expected){status('OAuth state mismatch. Повтори подключение.',true);history.replaceState({},'',redirectUri);return;}
 const verifier=sessionStorage.getItem('glyphscape_verifier'), client=sessionStorage.getItem('glyphscape_client');
 try{
  const res=await fetch('https://enter.pollinations.ai/api/oauth/token',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({grant_type:'authorization_code',code,client_id:client,redirect_uri:redirectUri,code_verifier:verifier})});
  const data=await res.json();if(!res.ok||!data.access_token)throw new Error(data.error_description||data.error||('OAuth HTTP '+res.status));
  state.token=data.access_token;sessionStorage.setItem('glyphscape_token',state.token);
  status('Pollinations подключён. Можно генерировать мир.');
 }catch(e){status('Ошибка авторизации: '+e.message,true);}
 finally{sessionStorage.removeItem('glyphscape_verifier');sessionStorage.removeItem('glyphscape_state');sessionStorage.removeItem('glyphscape_client');history.replaceState({},'',redirectUri);setConnection();}
}
async function generate(){
 if(!state.token){status('Сначала подключи Pollinations App Key.',true);return;}
 if(state.busy)return;
 state.busy=true;setConnection();status('AI генерирует фотографическую территорию…');
 const prompt=$('prompt').value.trim() + ', extremely realistic high-resolution landscape photograph, physically accurate natural light, intricate real-world surface detail, wide environmental composition, no ASCII, no symbols, no typography, no UI';
 const url=new URL('https://gen.pollinations.ai/image/'+encodeURIComponent(prompt));
 url.searchParams.set('model',$('model').value);url.searchParams.set('width','1024');url.searchParams.set('height','1024');url.searchParams.set('seed',String(Math.floor(Math.random()*2147483647)));
 try{
  const response=await fetch(url,{headers:{Authorization:'Bearer '+state.token}});
  if(!response.ok){let detail='';try{const d=await response.json();detail=d.detail||d.error||d.message||'';}catch{}throw new Error('HTTP '+response.status+(detail?' — '+detail:''));}
  const blob=await response.blob();if(!blob.type.startsWith('image/'))throw new Error('API вернул не изображение ('+blob.type+')');
  const bitmap=await createImageBitmap(blob);state.image=bitmap;state.cameraX=.5;state.cameraY=.5;state.seed++;render();
  $('worldInfo').textContent='AI WORLD '+String(state.seed).padStart(3,'0')+' · '+bitmap.width+'×'+bitmap.height;
  status('Готово: AI-изображение получено и преобразовано в символьный мир.');
 }catch(e){status('Генерация не удалась: '+e.message,true);}
 finally{state.busy=false;setConnection();}
}
function resize(){
 const rect=canvas.getBoundingClientRect(), dpr=Math.min(devicePixelRatio||1,1.5);
 const w=Math.max(1,Math.round(rect.width*dpr)),h=Math.max(1,Math.round(rect.height*dpr));
 if(canvas.width!==w||canvas.height!==h){canvas.width=w;canvas.height=h;render();}
}
function render(){
 const w=canvas.width,h=canvas.height;if(!w||!h)return;
 ctx.fillStyle='#05090b';ctx.fillRect(0,0,w,h);
 if(!state.image){ctx.fillStyle='#8fffe0';ctx.font='14px ui-monospace,monospace';ctx.textAlign='center';ctx.fillText('CONNECT AI  /  GENERATE WORLD',w/2,h/2);return;}
 const img=state.image, aspect=w/h;
 let sw,sh;if(img.width/img.height>aspect){sh=img.height;sw=sh*aspect;}else{sw=img.width;sh=sw/aspect;}
 const sx=(img.width-sw)*state.cameraX,sy=(img.height-sh)*state.cameraY;
 if(state.mode==='photo'){ctx.drawImage(img,sx,sy,sw,sh,0,0,w,h);return;}
 const cell=Math.max(3,Number(state.density));const cols=Math.ceil(w/cell),rows=Math.ceil(h/cell);
 sampleCanvas.width=cols;sampleCanvas.height=rows;
 sampleCtx.drawImage(img,sx,sy,sw,sh,0,0,cols,rows);
 const pixels=sampleCtx.getImageData(0,0,cols,rows).data;
 const fontSize=cell*1.12;ctx.font=fontSize+'px monospace';ctx.textBaseline='top';ctx.textAlign='left';
 for(let y=0;y<rows;y++)for(let x=0;x<cols;x++){
  const i=(y*cols+x)*4,r=pixels[i],g=pixels[i+1],b=pixels[i+2];
  const lum=(.2126*r+.7152*g+.0722*b)/255;
  const prev=x?((.2126*pixels[i-4]+.7152*pixels[i-3]+.0722*pixels[i-2])/255):lum;
  const next=x<cols-1?((.2126*pixels[i+4]+.7152*pixels[i+5]+.0722*pixels[i+6])/255):lum;
  const edge=Math.min(1,Math.abs(next-prev)*2.8);
  const tone=Math.max(0,Math.min(1,lum*state.contrast));
  const idx=Math.min(GLYPHS.length-1,Math.floor((tone*.82+edge*.18)*(GLYPHS.length-1)));
  const alpha=.48+tone*.52;ctx.fillStyle='rgba('+r+','+g+','+b+','+alpha+')';
  ctx.fillText(GLYPHS[idx],x*cell,y*cell);
 }
}
function move(dx,dy){if(!state.image)return;state.cameraX=Math.max(0,Math.min(1,state.cameraX+dx*.055));state.cameraY=Math.max(0,Math.min(1,state.cameraY+dy*.055));render();}
$('generate').addEventListener('click',generate);$('connect').addEventListener('click',connect);
$('disconnect').addEventListener('click',()=>{state.token='';sessionStorage.removeItem('glyphscape_token');setConnection();status('AI отключён.');});
$('density').addEventListener('input',e=>{$('densityOut').textContent=e.target.value+' px';state.density=Number(e.target.value);render();});
$('contrast').addEventListener('input',e=>{$('contrastOut').textContent=Number(e.target.value).toFixed(2);state.contrast=Number(e.target.value);render();});
$('glyph').addEventListener('click',()=>{state.mode='glyph';$('glyph').classList.add('primary');$('photo').classList.remove('primary');render();});
$('photo').addEventListener('click',()=>{state.mode='photo';$('photo').classList.add('primary');$('glyph').classList.remove('primary');render();});
$('reset').addEventListener('click',()=>{state.cameraX=.5;state.cameraY=.5;render();});
document.querySelectorAll('[data-move]').forEach(b=>b.addEventListener('click',()=>{const m=b.dataset.move;move(m==='left'?-1:m==='right'?1:0,m==='up'?-1:m==='down'?1:0);}));
window.addEventListener('keydown',e=>{const k=e.key.toLowerCase();const map={arrowleft:[-1,0],a:[-1,0],arrowright:[1,0],d:[1,0],arrowup:[0,-1],w:[0,-1],arrowdown:[0,1],s:[0,1]};if(map[k]){e.preventDefault();move(...map[k]);}});
canvas.addEventListener('pointerdown',e=>{state.drag={x:e.clientX,y:e.clientY,cx:state.cameraX,cy:state.cameraY};canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener('pointermove',e=>{if(!state.drag||!state.image)return;const r=canvas.getBoundingClientRect(),img=state.image,aspect=r.width/r.height;let sw,sh;if(img.width/img.height>aspect){sh=img.height;sw=sh*aspect;}else{sw=img.width;sh=sw/aspect;}state.cameraX=Math.max(0,Math.min(1,state.drag.cx-(e.clientX-state.drag.x)/r.width*sw/(img.width-sw||1)));state.cameraY=Math.max(0,Math.min(1,state.drag.cy-(e.clientY-state.drag.y)/r.height*sh/(img.height-sh||1)));render();});
canvas.addEventListener('pointerup',()=>state.drag=null);canvas.addEventListener('pointercancel',()=>state.drag=null);
new ResizeObserver(resize).observe(canvas);resize();handleCallback();setConnection();
})();