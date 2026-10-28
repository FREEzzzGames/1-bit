/* FREEzzz ARCADE — button-only selector for five games on the fixed 208x320 display. */
(()=>{'use strict';
const W=208,H=320,C='#FFF8E8',B='#000000',G='#77736b';
const canvas=document.getElementById('c'),ctx=canvas.getContext('2d',{alpha:false});
canvas.width=W;canvas.height=H;canvas.style.width='100%';canvas.style.height='100%';ctx.imageSmoothingEnabled=false;
const glyph={'A':'010101111101101','B':'110101110101110','C':'011100100100011','D':'110101101101110','E':'111100110100111','F':'111100110100100','G':'011100101101011','H':'101101111101101','I':'111010010010111','J':'001001001101010','K':'101101110101101','L':'100100100100111','M':'101111111101101','N':'101111101101101','O':'010101101101010','P':'110101110100100','Q':'010101101111011','R':'110101110101101','S':'011100010001110','T':'111010010010010','U':'101101101101111','V':'101101101101010','W':'101101111111101','X':'101101010101101','Y':'101101010010010','Z':'111001010100111','0':'111101101101111','1':'010110010010111','2':'111001111100111','3':'111001111001111','4':'101101111001001','5':'111100111001111','6':'111100111101111','7':'111001001001001','8':'111101111101111','9':'111101111001111','-':'000000111000000','/':'001001010100100',' ':'000000000000000'};
function px(x,y,w=1,h=1,col=C){ctx.fillStyle=col;ctx.fillRect(Math.round(x),Math.round(y),Math.round(w),Math.round(h))}
function text(str,x,y,scale=1,col=C,align='center'){str=String(str).toUpperCase();const width=str.length*4*scale-scale;let ox=align==='center'?Math.round(x-width/2):align==='right'?Math.round(x-width):Math.round(x);for(const ch of str){const bits=glyph[ch];if(bits)for(let r=0;r<5;r++)for(let c=0;c<3;c++)if(bits.charCodeAt(r*3+c)===49)px(ox+c*scale,y+r*scale,scale,scale,col);ox+=4*scale}}
const games=[
{id:'shadow',name:'SHADOW SPIRE',sub:'PLATFORM ADVENTURE',href:'./shadow-spire.html'},
{id:'runner',name:'NIGHT RUNNER',sub:'ARCADE RACING',href:'./night-runner.html'},
{id:'void',name:'VOID HUNTER',sub:'SPACE SHOOTER',href:'./void-hunter.html'},
{id:'iron',name:'IRON DEPTHS',sub:'TACTICAL DUNGEON',href:'./iron-depths.html'},
{id:'neon',name:'NEON DRIFT',sub:'DRIFT RACING',href:'./neon-drift.html'}];
let selected=0;
function card(y,g,active,index){px(12,y,184,66,active?C:G);px(14,y+2,180,62,B);px(14,y+2,180,1,active?C:G);px(14,y+63,180,1,active?C:G);text(String(index+1).padStart(2,'0'),21,y+8,1,G,'left');
if(g.id==='shadow'){px(28,y+21,20,25,C);px(32,y+14,12,7,C);px(35,y+8,6,6,C);px(34,y+28,3,3,B);px(40,y+28,3,3,B)}
if(g.id==='runner'){px(31,y+10,2,43,G);px(45,y+10,2,43,G);for(let z=0;z<4;z++)px(38,y+12+z*10,2,5,C);px(33,y+31,12,19,C);px(36,y+34,6,5,B);px(36,y+43,6,3,B)}
if(g.id==='void'){px(27,y+24,23,3,C);px(35,y+15,7,18,C);px(38,y+9,2,7,C);px(35,y+20,7,7,B);for(let z=0;z<3;z++)px(23+z*12,y+8+z*7,2,2,G)}
if(g.id==='iron'){for(let yy=0;yy<3;yy++)for(let xx=0;xx<3;xx++)px(27+xx*8,y+13+yy*8,6,6,(xx+yy)%2?G:C);px(35,y+21,6,6,B);px(35,y+21,3,3,C)}
if(g.id==='neon'){px(28,y+7,23,49,G);px(31,y+7,17,49,B);for(let yy=0;yy<4;yy++)px(39,y+10+yy*12,2,6,C);px(34,y+36,11,16,C);px(37,y+39,5,4,B);px(37,y+47,5,2,B)}
text(g.name,58,y+13,2,C,'left');text(g.sub,58,y+32,1,G,'left');text(active?'ACTION TO PLAY':'SELECT WITH L/R',58,y+47,1,active?C:G,'left');
}
function draw(){px(0,0,W,H,B);for(let i=0;i<36;i++){const x=(i*47+13)%W,y=(i*31+7)%H;if(y<95||y>260)px(x,y,i%7===0?2:1,1,i%5===0?G:C)}
px(7,7,W-14,1,G);px(7,7,1,78,G);px(W-8,7,1,78,G);px(7,84,W-14,1,G);text('FREEzzz',W/2,17,3,C);text('ARCADE',W/2,32,3,C);text('FIVE GAMES / ONE CONSOLE',W/2,52,1,G);
for(let i=0;i<5;i++)px(81+i*10,66,6,2,i===selected?C:G);
card(94,games[selected],true,selected);card(166,games[(selected+1)%games.length],false,(selected+1)%games.length);
text('GAME '+String(selected+1).padStart(2,'0')+' / 05',W/2,246,1,C);text('LEFT / RIGHT: BROWSE',W/2,261,1,G);text('ACTION: START GAME',W/2,273,1,C);px(6,306,W-12,1,G);text('FREEzzz GAMES // 1-BIT',W/2,310,1,C)}
function select(d){selected=(selected+d+games.length)%games.length;draw()}
function bind(id,fn){const el=document.querySelector('[data-control="'+id+'"]');if(!el)return;el.addEventListener('pointerdown',e=>{e.preventDefault();e.stopPropagation();fn()},{passive:false})}
bind('left',()=>select(-1));bind('right',()=>select(1));bind('jump',()=>location.href=games[selected].href);bind('lantern',()=>select(-1));bind('pause',()=>select(1));
window.addEventListener('keydown',e=>{if(['ArrowLeft','ArrowUp'].includes(e.key)){e.preventDefault();select(-1)}else if(['ArrowRight','ArrowDown'].includes(e.key)){e.preventDefault();select(1)}else if(e.key==='Enter'||e.key===' '){e.preventDefault();location.href=games[selected].href}});
draw();
})();