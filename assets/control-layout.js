/* Touch-first layout editor for the physical console controls. */
(()=>{'use strict';
const root=document.querySelector('.console'),controls=document.querySelector('.controls');
if(!root||!controls)return;
const buttons=[...controls.querySelectorAll('[data-control]')],KEY='shadowSpireControlLayout_v1';
let editing=false,drag=null,layout={};
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const rect=()=>root.getBoundingClientRect();
function defaults(){layout={};buttons.forEach(b=>{for(const p of ['left','top','right','bottom','width','height','aspectRatio','borderRadius'])b.style[p]='';});}
function read(){try{const v=JSON.parse(localStorage.getItem(KEY)||'{}');if(v&&typeof v==='object')layout=v}catch(_){layout={}}}
function apply(){buttons.forEach(b=>{const v=layout[b.dataset.control];if(!v||![v.x,v.y,v.w,v.h].every(Number.isFinite))return;b.style.left=(v.x*100)+'%';b.style.top=(v.y*100)+'%';b.style.right='auto';b.style.bottom='auto';b.style.width=(v.w*100)+'%';b.style.height=(v.h*100)+'%';b.style.aspectRatio='auto';if(b.dataset.control==='jump')b.style.borderRadius='50%';});}
function snapshot(b){const r=rect(),q=b.getBoundingClientRect();return{x:(q.left-r.left)/r.width,y:(q.top-r.top)/r.height,w:q.width/r.width,h:q.height/r.height};}
function save(){try{localStorage.setItem(KEY,JSON.stringify(layout))}catch(_){}}
const trigger=document.createElement('button');
trigger.type='button';trigger.className='control-layout-trigger';
trigger.setAttribute('aria-label','Настроить размер и положение кнопок');
trigger.textContent='⚙ CONTROLS';
controls.appendChild(trigger);
const bar=document.createElement('div');bar.className='control-layout-toolbar';bar.hidden=true;
bar.innerHTML='<span>MOVE: DRAG · RESIZE: CORNER</span><button type="button" data-layout-done>DONE</button><button type="button" data-layout-reset>RESET</button>';controls.appendChild(bar);
buttons.forEach(b=>{const handle=document.createElement('span');handle.className='control-resize-handle';handle.setAttribute('aria-label','Resize '+(b.getAttribute('aria-label')||b.dataset.control));handle.setAttribute('role','presentation');handle.hidden=true;b.appendChild(handle);});
function setEditing(on){editing=on;controls.classList.toggle('editing-controls',editing);bar.hidden=!editing;trigger.hidden=editing;buttons.forEach(b=>{b.classList.toggle('layout-editable',editing);b.querySelector('.control-resize-handle').hidden=!editing;});if(!editing){drag=null;save();}}
function start(e,mode,b){e.preventDefault();e.stopPropagation();const q=snapshot(b);drag={id:e.pointerId,mode,b,startX:e.clientX,startY:e.clientY,base:q};try{e.target.setPointerCapture(e.pointerId)}catch(_){}}
trigger.addEventListener('click',e=>{e.preventDefault();e.stopPropagation();setEditing(!editing);});
document.addEventListener('dblclick',e=>{const b=e.target.closest('.controls [data-control]');if(!b||editing)return;e.preventDefault();e.stopImmediatePropagation();setEditing(true);},true);
document.addEventListener('pointerdown',e=>{if(!editing)return;const done=e.target.closest('[data-layout-done]');if(done){e.preventDefault();e.stopImmediatePropagation();setEditing(false);return;}const reset=e.target.closest('[data-layout-reset]');if(reset){e.preventDefault();e.stopImmediatePropagation();defaults();save();setEditing(true);return;}const b=e.target.closest('.controls [data-control]');if(!b)return;e.preventDefault();e.stopImmediatePropagation();start(e,e.target.closest('.control-resize-handle')?'resize':'move',b);},true);
document.addEventListener('pointermove',e=>{if(!editing||!drag||drag.id!==e.pointerId)return;e.preventDefault();const d=drag,r=rect(),dx=(e.clientX-d.startX)/r.width,dy=(e.clientY-d.startY)/r.height;let{x,y,w,h}=d.base;if(d.mode==='move'){x=clamp(x+dx,0,1-w);y=clamp(y+dy,0,1-h);}else{w=clamp(w+dx,.055,Math.min(.65,1-x));h=clamp(h+dy,.035,Math.min(.35,1-y));}layout[d.b.dataset.control]={x,y,w,h};apply();},true);
function finish(e){if(drag&&drag.id===e.pointerId){e.preventDefault();drag=null;save();}}
document.addEventListener('pointerup',finish,true);document.addEventListener('pointercancel',finish,true);window.addEventListener('resize',apply);read();apply();
})();