/* Appearance controls are isolated from game runtime state. */
(()=>{'use strict';
const root=document.querySelector('.console');
if(!root)return;
const shellOpen=document.getElementById('shellSkinOpen'),padOpen=document.getElementById('padSkinOpen');
const shellPicker=document.getElementById('shellSkinPicker'),padPicker=document.getElementById('padSkinPicker');
const shellOptions=[...document.querySelectorAll('.style-option[data-shell-skin]')],padOptions=[...document.querySelectorAll('.style-option[data-pad-skin]')];
function setShellSkin(name){if(!shellOptions.some(b=>b.dataset.shellSkin===name))name='dark';root.dataset.shellSkin=name;shellOptions.forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.shellSkin===name)));try{localStorage.setItem('shadowSpireShellSkin',name)}catch(_){}}
function setPadSkin(name){if(!padOptions.some(b=>b.dataset.padSkin===name))name='graphite';root.dataset.padSkin=name;padOptions.forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.padSkin===name)));try{localStorage.setItem('shadowSpirePadSkin',name)}catch(_){}}
function closePickers(){shellPicker.classList.remove('open');padPicker.classList.remove('open');shellOpen.setAttribute('aria-expanded','false');padOpen.setAttribute('aria-expanded','false')}
shellOpen.addEventListener('click',()=>{const open=!shellPicker.classList.contains('open');closePickers();if(open){shellPicker.classList.add('open');shellOpen.setAttribute('aria-expanded','true')}});
padOpen.addEventListener('click',()=>{const open=!padPicker.classList.contains('open');closePickers();if(open){padPicker.classList.add('open');padOpen.setAttribute('aria-expanded','true')}});
shellOptions.forEach(b=>b.addEventListener('click',()=>{setShellSkin(b.dataset.shellSkin);closePickers()}));
padOptions.forEach(b=>b.addEventListener('click',()=>{setPadSkin(b.dataset.padSkin);closePickers()}));
let savedShell='dark',savedPad='graphite';try{savedShell=localStorage.getItem('shadowSpireShellSkin')||'dark';savedPad=localStorage.getItem('shadowSpirePadSkin')||'graphite'}catch(_){}
setShellSkin(savedShell);setPadSkin(savedPad);
document.addEventListener('pointerdown',e=>{if(!e.target.closest('.style-tools')&&!e.target.closest('.style-picker'))closePickers()});
document.addEventListener('keydown',e=>{if(e.key==='Escape')closePickers()});
})();
