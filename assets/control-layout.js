/* Touch-first editor for all movable console UI buttons. */
(()=>{'use strict';
const root=document.querySelector('.console'),controls=document.querySelector('.controls'),tools=document.querySelector('.style-tools');
if(!root||!controls)return;
const KEY='shadowSpireControlLayout_v2',clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const rootRect=()=>root.getBoundingClientRect();
const targets=[];
const defaults={};
function keyOf(el){return el.dataset.layoutId||el.dataset.control||el.id||'control'}
function measure(el){const r=rootRect(),q=el.getBoundingClientRect();return{x:(q.left-r.left)/r.width,y:(q.top-r.top)/r.height,w:q.width/r.width,h:q.height/r.height}}
function applyOne(el,v){el.style.position='absolute';el.style.left=(v.x*100)+'%';el.style.top=(v.y*100)+'%';el.style.right='auto';el.style.bottom='auto';el.style.width=(v.w*100)+'%';el.style.height=(v.h*100)+'%';el.style.maxWidth='none';el.style.minWidth='0';el.style.boxSizing='border-box';el.style.aspectRatio='auto';}
function apply(){for(const el of targets){const v=layout[keyOf(el)]||defaults[keyOf(el)];if(v)applyOne(el,v)}}
function save(){try{localStorage.setItem(KEY,JSON.stringify(layout))}catch(_){}}
let layout={};try{const v=JSON.parse(localStorage.getItem(KEY)||'{}');if(v&&typeof v==='object')layout=v}catch(_){}
if(tools){
 const header=[...tools.querySelectorAll('.style-trigger')];
 for(const el of header){const id=el.id==='shellSkinOpen'?'shellSkin':el.id==='padSkinOpen'?'padSkin':'gameMenu';el.dataset.layoutId=id;defaults[id]=measure(el);controls.appendChild(el);targets.push(el)}
}
const hardware=[...controls.querySelectorAll('[data-control]')];
for(const el of hardware){el.dataset.layoutId=el.dataset.control;defaults[el.dataset.layoutId]=measure(el);targets.push(el)}
const trigger=document.createElement('button');trigger.type='button';trigger.className='control-layout-trigger';trigger.dataset.layoutId='controlSettings';trigger.setAttribute('aria-label','Настроить размер и положение элементов');trigger.textContent='⚙ CONTROLS';controls.appendChild(trigger);defaults.controlSettings=measure(trigger);targets.push(trigger);
let editing=false,drag=null;
const bar=document.createElement('div');bar.className='control-layout-toolbar';bar.hidden=true;bar.innerHTML='<span>MOVE: DRAG · RESIZE: CORNER</span><button type="button" data-layout-done>DONE</button><button type="button" data-layout-reset>RESET</button>';controls.appendChild(bar);
for(const el of targets){const handle=document.createElement('span');handle.className='control-resize-handle';handle.setAttribute('aria-hidden','true');handle.hidden=true;el.appendChild(handle)}
function setEditing(on){editing=on;controls.classList.toggle('editing-controls',on);root.classList.toggle('layout-editing',on);bar.hidden=!on;trigger.textContent=on?'⚙ EDIT MODE':'⚙ CONTROLS';for(const el of targets){el.classList.toggle('layout-editable',on);el.querySelector('.control-resize-handle').hidden=!on}if(!on){drag=null;save()}}
function start(e,mode,el){e.preventDefault();e.stopImmediatePropagation();const q=measure(el);drag={id:e.pointerId,mode,el,startX:e.clientX,startY:e.clientY,base:q};try{e.target.setPointerCapture(e.pointerId)}catch(_){}}
trigger.addEventListener('click',e=>{if(editing){e.preventDefault();e.stopImmediatePropagation();return}e.preventDefault();e.stopPropagation();setEditing(true)});
document.addEventListener('pointerdown',e=>{if(!editing)return;const done=e.target.closest('[data-layout-done]');if(done){e.preventDefault();e.stopImmediatePropagation();setEditing(false);return}const reset=e.target.closest('[data-layout-reset]');if(reset){e.preventDefault();e.stopImmediatePropagation();layout={};apply();save();return}const el=e.target.closest('.controls .layout-editable');if(!el)return;e.preventDefault();e.stopImmediatePropagation();start(e,e.target.closest('.control-resize-handle')?'resize':'move',el)},true);
document.addEventListener('pointermove',e=>{if(!editing||!drag||drag.id!==e.pointerId)return;e.preventDefault();const d=drag,r=rootRect(),dx=(e.clientX-d.startX)/r.width,dy=(e.clientY-d.startY)/r.height;let{x,y,w,h}=d.base;if(d.mode==='move'){x=clamp(x+dx,0,1-w);y=clamp(y+dy,0,1-h)}else{w=clamp(w+dx,.055,Math.min(.7,1-x));h=clamp(h+dy,.025,Math.min(.35,1-y))}layout[keyOf(d.el)]={x,y,w,h};applyOne(d.el,layout[keyOf(d.el)])},true);
function finish(e){if(drag&&drag.id===e.pointerId){e.preventDefault();drag=null;save()}}
document.addEventListener('pointerup',finish,true);document.addEventListener('pointercancel',finish,true);window.addEventListener('resize',apply);apply();
})();