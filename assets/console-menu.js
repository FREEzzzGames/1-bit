/* FREEzzz ARCADE — 1-bit game launcher for the fixed 208x320 console display. */
(()=>{'use strict';
const W=208,H=320,C='#FFF8E8',B='#000000',G='#77736b';
const canvas=document.getElementById('c'),ctx=canvas.getContext('2d',{alpha:false});
canvas.width=W;canvas.height=H;canvas.style.width='100%';canvas.style.height='100%';ctx.imageSmoothingEnabled=false;
const glyph={
'A':'010101111101101','B':'110101110101110','C':'011100100100011','D':'110101101101110','E':'111100110100111','F':'111100110100100','G':'011100101101011','H':'101101111101101','I':'111010010010111','J':'001001001101010','K':'101101110101101','L':'100100100100111','M':'101111111101101','N':'101111101101101','O':'010101101101010','P':'110101110100100','Q':'010101101111011','R':'110101110101101','S':'011100010001110','T':'111010010010010','U':'101101101101111','V':'101101101101010','W':'101101111111101','X':'101101010101101','Y':'101101010010010','Z':'111001010100111','0':'111101101101111','1':'010110010010111','2':'111001111100111','3':'111001111001111','4':'101101111001001','5':'111100111001111','6':'111100111101111','7':'111001001001001','8':'111101111101111','9':'111101111001111','-':'000000111000000',':':'000010000010000',' ':'000000000000000','/':'001001010100100','+':'000010111010000'};
function px(x,y,w=1,h=1,col=C){ctx.fillStyle=col;ctx.fillRect(Math.round(x),Math.round(y),Math.round(w),Math.round(h))}
function text(str,x,y,scale=1,col=C,align='center'){str=String(str).toUpperCase();const width=str.length*4*scale-scale;let ox=align==='center'?Math.round(x-width/2):align==='right'?Math.round(x-width):Math.round(x);for(const ch of str){const bits=glyph[ch];if(bits)for(let r=0;r<5;r++)for(let c=0;c<3;c++)if(bits.charCodeAt(r*3+c)===49)px(ox+c*scale,y+r*scale,scale,scale,col);ox+=4*scale}}
function car(x,y,s=1){px(x-2*s,y+3*s,16*s,20*s,C);px(x,y,12*s,24*s,B);px(x+2*s,y+3*s,8*s,5*s,C);px(x+2*s,y+11*s,8*s,4*s,C);px(x+1*s,y+20*s,3*s,2*s,C);px(x+8*s,y+20*s,3*s,2*s,C);px(x-2*s,y+5*s,2*s,5*s,C);px(x+12*s,y+5*s,2*s,5*s,C)}
function spire(x,y){px(x+10,y,2,2,C);px(x+8,y+2,6,4,C);px(x+6,y+6,10,5,C);px(x+4,y+11,14,6,C);px(x+2,y+17,18,10,C);px(x,y+27,22,23,C);for(let i=0;i<4;i++)px(x+4+i*4,y+32,2,2,B);px(x+8,y+10,2,3,B);px(x+12,y+10,2,3,B)}
const cards=[{x:12,y:112,w:184,h:66,id:'shadow',href:'./shadow-spire.html'},{x:12,y:188,w:184,h:66,id:'racer',href:'./night-runner.html'}];
function cardFrame(r,active){px(r.x,r.y,r.w,r.h,active?C:G);px(r.x+2,r.y+2,r.w-4,r.h-4,B);px(r.x+2,r.y+2,r.w-4,1,active?C:G);px(r.x+2,r.y+r.h-3,r.w-4,1,active?C:G);if(active){px(r.x+4,r.y+4,4,2,C);px(r.x+r.w-8,r.y+r.h-6,4,2,C)}}
function draw(){
 px(0,0,W,H,B);
 for(let i=0;i<34;i++){const x=(i*47+13)%W,y=(i*31+7)%H;if(y<98||y>260)px(x,y,i%7===0?2:1,1,i%5===0?G:C)}
 // Fine double frame and header ornaments.
 px(7,7,W-14,1,G);px(7,7,1,78,G);px(W-8,7,1,78,G);px(7,84,W-14,1,G);
 text('FREEzzz',W/2,17,3,C);text('ARCADE',W/2,32,3,C);
 text('ONE CONSOLE. TWO WORLDS.',W/2,51,1,G);
 for(let i=0;i<5;i++){px(78+i*11,66,7,2,i<3?C:G);px(80+i*11,70,3,1,B)}
 cards.forEach((r,i)=>cardFrame(r,i===selected));
 // SHADOW SPIRE thumbnail: distant tower, crystal and stars.
 spire(24,121);text('01',52,122,1,G,'left');text('SHADOW',52,134,2,C,'left');text('SPIRE',52,148,2,C,'left');text('PLATFORM ADVENTURE',52,163,1,G,'left');
 px(169,132,1,20,G);px(165,151,9,1,G);px(167,146,5,5,C);px(169,142,1,4,C);
 // NIGHT RUNNER thumbnail: dashed road, player car and road-side markers.
 px(25,198,2,44,G);px(43,198,2,44,G);for(let y=199;y<240;y+=9)px(34,y,2,4,C);car(29,211,.8);
 text('02',54,198,1,G,'left');text('NIGHT',54,209,2,C,'left');text('RUNNER',54,223,2,C,'left');text('ARCADE RACING',54,238,1,G,'left');
 text('TAP A GAME TO START',W/2,273,1,C);
 text('SELECT  /  PLAY',W/2,286,1,G);
 px(6,306,W-12,1,G);text('FREEzzz GAMES  //  1-BIT',W/2,310,1,C);
}
let selected=0;
function hit(x,y){const r=cards.find(v=>x>=v.x&&x<=v.x+v.w&&y>=v.y&&y<=v.y+v.h);if(r)location.href=r.href}
function coords(e){const r=canvas.getBoundingClientRect();return{x:(e.clientX-r.left)*W/r.width,y:(e.clientY-r.top)*H/r.height}}
canvas.addEventListener('pointerdown',e=>{e.preventDefault();const p=coords(e);const i=cards.findIndex(r=>p.x>=r.x&&p.x<=r.x+r.w&&p.y>=r.y&&p.y<=r.y+r.h);if(i>=0){selected=i;draw();setTimeout(()=>location.href=cards[i].href,70)}},{passive:false});
window.addEventListener('keydown',e=>{if(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key)){e.preventDefault();selected=selected===0?1:0;draw()}else if(e.key==='Enter'||e.key===' '){e.preventDefault();location.href=cards[selected].href}});
draw();
})();