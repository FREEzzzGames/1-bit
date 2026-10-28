
(function(){
'use strict';
const CL='#FFF8E8',CD='#000000',T=16;
const SCREEN=window.CONSOLE_GAME_SCREEN;const SCREEN_WIDTH=SCREEN.width,SCREEN_HEIGHT=SCREEN.height;let VW=SCREEN_WIDTH,VH=SCREEN_HEIGHT,S=1;
const GR=700,MF=430,MV=112,AC=1000,FG=1400,FA=260,JU=250;
const cv=document.getElementById('c'),cx=cv.getContext('2d',{alpha:false});
cx.imageSmoothingEnabled=false;
const cl=(v,a,b)=>v<a?a:(v>b?b:v),lp=(a,b,t)=>a+(b-a)*t,sg=v=>v<0?-1:(v>0?1:0);
let ac=null;
const ia=()=>{if(!ac){try{ac=new(window.AudioContext||window.webkitAudioContext)()}catch(e){ac=null}}if(ac&&ac.state==='suspended')ac.resume()};
const bp=(f,d,t,v,s)=>{if(!ac)return;t=t||'square';v=v||.05;const t0=ac.currentTime,o=ac.createOscillator(),g=ac.createGain();o.type=t;o.frequency.setValueAtTime(f,t0);if(s)o.frequency.exponentialRampToValueAtTime(Math.max(20,s),t0+d);g.gain.setValueAtTime(v,t0);g.gain.exponentialRampToValueAtTime(.0001,t0+d);o.connect(g);g.connect(ac.destination);o.start(t0);o.stop(t0+d+.02)};
const SFX={jump:()=>bp(300,.13,'square',.045,640),step:()=>bp(110,.045,'square',.022),lampOn:()=>{bp(440,.07,'square',.05);setTimeout(()=>bp(680,.1,'square',.05),55)},lampOff:()=>bp(340,.09,'square',.04,170),key:()=>{bp(880,.07,'square',.06);setTimeout(()=>bp(1320,.14,'square',.06),65)},door:()=>bp(200,.28,'sawtooth',.05,80),hurt:()=>bp(200,.28,'sawtooth',.075,55),win:()=>[523,659,784,1046].forEach((f,i)=>setTimeout(()=>bp(f,.16,'square',.06),i*95)),crystal:()=>bp(1150,.09,'square',.05,1750),ui:()=>bp(700,.045,'square',.035)};
const F={A:'010101111101101',B:'110101110101110',C:'011100100100011',D:'110101101101110',E:'111100110100111',F:'111100110100100',G:'011100101101011',H:'101101111101101',I:'111010010010111',J:'001001001101010',K:'101101110101101',L:'100100100100111',M:'101111111101101',N:'101111101101101',O:'010101101101010',P:'110101110100100',Q:'010101101111011',R:'110101110101101',S:'011100010001110',T:'111010010010010',U:'101101101101111',V:'101101101101010',W:'101101111111101',X:'101101010101101',Y:'101101010010010',Z:'111001010100111','0':'111101101101111','1':'010110010010111','2':'110001010100111','3':'110001010001110','4':'101101111001001','5':'111100110001110','6':'011100111101111','7':'111001010010010','8':'111101111101111','9':'111101111001110',' ':'000000000000000','.':'000000000000010',':':'000010000010000','-':'000000111000000','!':'010010010000010','?':'110001010000010',"'":'010010000000000','/':'001001010100100',',':'000000000010100','+':'000010111010000','*':'101010101000000','>':'100010001010100','<':'001010100010001','(':'001010010010001',')':'100010010010100'};
const dt=(s,x,y,sc,col)=>{sc=sc||1;col=col||CL;cx.fillStyle=col;let px=Math.round(x),py=Math.round(y);s=String(s).toUpperCase();for(let i=0;i<s.length;i++){const g=F[s[i]];if(!g){px+=4*sc;continue}for(let r=0;r<5;r++)for(let c=0;c<3;c++)if(g.charCodeAt(r*3+c)===49)cx.fillRect(px+c*sc,py+r*sc,sc,sc);px+=4*sc}};
const tw=(s,sc)=>{sc=sc||1;return Math.max(0,String(s).length*4*sc-sc)};
const dtc=(s,x,y,sc,col)=>dt(s,Math.round(x-tw(s,sc)/2),y,sc,col);
const PT={};
const mp=(w,h,p,col)=>{const c=document.createElement('canvas');c.width=w;c.height=h;const g=c.getContext('2d');g.fillStyle=col;for(const[x,y]of p)g.fillRect(x,y,1,1);return cx.createPattern(c,'repeat')};
function bpt(){PT.B50=mp(2,2,[[0,0],[1,1]],CD);PT.B25=mp(4,4,[[0,0],[2,2]],CD);PT.B75=mp(4,4,[[0,0],[1,1],[2,0],[3,1],[0,2],[2,2]],CD);PT.W50=mp(2,2,[[0,0],[1,1]],CL);PT.W25=mp(4,4,[[0,0],[2,2]],CL);PT.W75=mp(4,4,[[0,0],[1,1],[2,0],[3,1],[0,2],[2,2]],CL)}
function makeTowerRows(seed,style){
  const W=13,H=28,grid=Array.from({length:H},()=>Array(W).fill('.'));
  const put=(x,y,ch)=>{if(y>=0&&y<H&&x>=0&&x<W)grid[y][x]=ch};
  const fill=(x,y,w,ch='#')=>{for(let i=0;i<w;i++)put(x+i,y,ch)};
  fill(0,H-1,W,'#');
  const centers=[6];let center=6;
  for(let i=0,row=25;row>=3;row-=2,i++){
    const step=1+((seed+i)%2);
    center=cl(center+(((i+seed)%2===0)?step:-step),3,9);
    centers.push(center);
    const width=5+((seed+i)%3);
    const left=cl(center-Math.floor(width/2),0,W-width);
    const kind=(style===1&&i%3===1)?':':(style===2&&i%3===2?'=':'#');
    fill(left,row,width,kind);
    if(seed%3===1&&i%4===2&&width>=5)put(left+2,row,'^');
    if(seed%4===2&&i%5===3&&width>=6)put(left+width-2,row,'^');
  }
  const startX=cl(centers[0],2,10),goalX=cl(centers[centers.length-1],2,10);
  put(startX,H-2,'S');put(goalX,2,'G');
  put(cl(centers[4],1,11),17,'P');
  if(seed%3===0){put(cl(centers[3],1,11),19,'K');}
  if(seed%2===0){put(cl(centers[4],1,11),15,'C');}
  if(seed%4===1){put(cl(centers[6],1,11),9,'C');}
  if(seed===9){put(cl(centers[2]-2,1,11),21,'C');put(cl(centers[4]+1,1,11),15,'C');put(cl(centers[7]-1,1,11),9,'C');}
  if(seed===19){put(cl(centers[2]-1,1,11),21,'C');put(cl(centers[4]+1,1,11),15,'C');put(cl(centers[7]-1,1,11),9,'C');}
  return grid.map(row=>row.join(''));
}
const LVS=[
{n:'OUTSKIRTS OF SHADOW',d:0,bg:'forest',w:0,o:[],r:makeTowerRows(0,0)},
{n:'WHISPERING FOREST',d:0,bg:'forest',w:0,o:[{type:'saw',ax:55,ay:320,bx:150,by:320,r:7,speed:.55}],r:makeTowerRows(1,1)},
{n:'FORGOTTEN GATES',d:0,bg:'ruins',w:0,o:[{type:'saw',ax:150,ay:250,bx:55,by:250,r:7,speed:.8}],r:makeTowerRows(2,2)},
{n:'BRIDGE OF SOLITUDE',d:0,bg:'cliff',w:12,o:[{type:'saw',ax:55,ay:190,bx:150,by:190,r:7,speed:.6}],r:makeTowerRows(3,0)},
{n:'CAVES OF ECHO',d:1,bg:'cave',w:0,o:[{type:'saw',ax:150,ay:300,bx:55,by:300,r:7,speed:.6},{type:'saw',ax:55,ay:150,bx:150,by:150,r:7,speed:.65}],r:makeTowerRows(4,1)},
{n:'STEEP ASCENT',d:.35,bg:'cliff',w:0,o:[{type:'saw',ax:55,ay:240,bx:150,by:240,r:7,speed:.55}],r:makeTowerRows(5,2)},
{n:'OUTER SPIRE WALL',d:.15,bg:'castle',w:0,o:[{type:'gear',gx:104,gy:220,r:11,spin:2.2}],r:makeTowerRows(6,0)},
{n:'HALL OF MIRRORS',d:.25,bg:'mirror',w:0,inv:1,o:[{type:'saw',ax:55,ay:170,bx:150,by:170,r:7,speed:.8}],r:makeTowerRows(7,1)},
{n:'CLOCK TOWER',d:.2,bg:'castle',w:0,o:[{type:'gear',gx:104,gy:280,r:10,spin:2.6},{type:'saw',ax:55,ay:120,bx:150,by:120,r:7,speed:.35}],r:makeTowerRows(8,2)},
{n:'STAR SPIRE',d:.55,bg:'spire',w:8,o:[{type:'saw',ax:55,ay:280,bx:150,by:280,r:7,speed:.85},{type:'gear',gx:104,gy:190,r:11,spin:3}],r:makeTowerRows(9,0)},
{n:'SUNKEN ARCHIVE',d:.35,bg:'cave',w:0,o:[{type:'saw',ax:150,ay:300,bx:55,by:300,r:7,speed:.7}],r:makeTowerRows(10,1)},
{n:'GLASS CATACOMBS',d:.4,bg:'mirror',w:0,inv:1,o:[{type:'saw',ax:55,ay:240,bx:150,by:240,r:7,speed:.75},{type:'saw',ax:150,ay:120,bx:55,by:120,r:7,speed:.55}],r:makeTowerRows(11,2)},
{n:'BROKEN AQUEDUCT',d:.25,bg:'ruins',w:14,o:[{type:'saw',ax:55,ay:200,bx:150,by:200,r:7,speed:.7}],r:makeTowerRows(12,0)},
{n:'HOLLOW ENGINE',d:.45,bg:'castle',w:0,o:[{type:'gear',gx:104,gy:250,r:11,spin:2.4},{type:'saw',ax:150,ay:150,bx:55,by:150,r:7,speed:.7}],r:makeTowerRows(13,1)},
{n:'VEIL GARDEN',d:.65,bg:'forest',w:0,o:[{type:'saw',ax:55,ay:280,bx:150,by:280,r:7,speed:.8},{type:'saw',ax:150,ay:180,bx:55,by:180,r:7,speed:.7}],r:makeTowerRows(14,2)},
{n:'BLACK FURNACE',d:.7,bg:'cliff',w:12,o:[{type:'gear',gx:104,gy:220,r:12,spin:2.8}],r:makeTowerRows(15,0)},
{n:'SHATTERED ORRERY',d:.45,bg:'mirror',w:0,inv:1,o:[{type:'gear',gx:104,gy:260,r:10,spin:3},{type:'gear',gx:104,gy:150,r:10,spin:-2.6}],r:makeTowerRows(16,1)},
{n:'LAST WATCH',d:.55,bg:'castle',w:8,o:[{type:'saw',ax:55,ay:280,bx:150,by:280,r:7,speed:.9},{type:'gear',gx:104,gy:190,r:12,spin:3.2},{type:'saw',ax:150,ay:100,bx:55,by:100,r:7,speed:.7}],r:makeTowerRows(17,2)},
{n:'ECHO VAULT',d:.9,bg:'cave',w:0,inv:1,o:[{type:'saw',ax:150,ay:240,bx:55,by:240,r:7,speed:.8},{type:'gear',gx:104,gy:130,r:11,spin:3}],r:makeTowerRows(18,0)},
{n:'HEART OF THE SPIRE',d:.85,bg:'spire',w:10,inv:1,o:[{type:'saw',ax:55,ay:280,bx:150,by:280,r:7,speed:.95},{type:'gear',gx:104,gy:190,r:12,spin:3.4},{type:'saw',ax:150,ay:100,bx:55,by:100,r:7,speed:.8}],r:makeTowerRows(19,1)}
];
const SK='shadowspire_save_v1';let save={unlocked:1,cleared:{}};
const ls=()=>{try{const r=localStorage.getItem(SK);if(r){const s=JSON.parse(r);if(s&&typeof s.unlocked==='number'){save.unlocked=cl(s.unlocked|0,1,LVS.length);save.cleared=s.cleared||{}}}}catch(e){}};
const ws=()=>{try{localStorage.setItem(SK,JSON.stringify(save))}catch(e){}};
const ST={MENU:0,LEVELS:1,PLAY:2,DEAD:3,WIN:4,ENDING:5,PAUSE:6};let state=ST.MENU,stateT=0,levelIndex=0,level=null,cam={x:0,y:0},crSt=new Map(),stars=[];
const P={x:0,y:0,w:9,h:13,vx:0,vy:0,onGround:false,coyote:0,jumpBuf:0,jumpsUsed:0,wallDir:0,dashT:0,dashCd:0,dashDir:1,facing:1,lanternOn:true,alive:true,animT:0,stepT:0,prevY:0,riding:null,hasKey:false,crystals:0};
const dirTap={left:-Infinity,right:-Infinity};
const I={left:false,right:false,jump:false,jumpPressed:false,lanternPressed:false};
function rs(){/* Canonical portrait game surface shared by every title on this console. */VW=SCREEN_WIDTH;VH=SCREEN_HEIGHT;cv.width=SCREEN_WIDTH;cv.height=SCREEN_HEIGHT;cv.style.width='100%';cv.style.height='100%';cx.imageSmoothingEnabled=false;bpt()}
function pl(d){const rows=d.r,H=rows.length;let W=0;for(let i=0;i<H;i++)W=Math.max(W,rows[i].length);const ti=new Uint8Array(W*H),lv={w:W,h:H,tiles:ti,spawn:{x:32,y:32},keys:[],crystals:[],checkpoints:[],doors:[],goal:null,objects:[],def:d};for(let y=0;y<H;y++){const row=rows[y];for(let x=0;x<W;x++){const ch=x<row.length?row.charAt(x):'.',idx=y*W+x;if(ch==='#')ti[idx]=1;else if(ch===':')ti[idx]=2;else if(ch==='^')ti[idx]=3;else if(ch==='=')ti[idx]=4;else if(ch==='K')lv.keys.push({x:x*T+8,y:y*T+8,got:false,t:0});else if(ch==='C')lv.crystals.push({x:x*T+8,y:y*T+8,got:false,t:0});else if(ch==='P'){ti[idx]=6;lv.checkpoints.push({x:x*T,y:y*T,active:false})}else if(ch==='G')lv.goal={x:x*T+8,y:y*T+8,t:0};else if(ch==='S')lv.spawn={x:x*T+3,y:y*T+3};else if(ch==='D'){const dr={cx:x,cy:y,open:false,anim:0};lv.doors.push(dr);for(let k=0;k<3;k++)if(y-k>=0)ti[(y-k)*W+x]=5}}}
const objs=d.o||[];for(const s of objs){const o=Object.assign({},s);if(o.type==='saw'){o.x=o.ax;o.y=o.ay;o.t=o.phase||0;o.dir=1;o.r=o.r||7;o.speed=o.speed||.5}else if(o.type==='gear'){o.x=o.gx;o.y=o.gy;o.r=o.r||10;o.rot=0;o.spin=o.spin||1.5}lv.objects.push(o)}return lv}
function stl(i,fromCheckpoint){const resume=fromCheckpoint&&level&&levelIndex===i?P.checkpoint:null;levelIndex=cl(i,0,LVS.length-1);level=pl(LVS[levelIndex]);crSt=new Map();Object.assign(P,{x:resume?resume.x:level.spawn.x,y:resume?resume.y:level.spawn.y,vx:0,vy:0,onGround:false,coyote:0,jumpBuf:0,jumpsUsed:0,wallDir:0,dashT:0,dashCd:0,dashDir:1,facing:1,lanternOn:true,alive:true,animT:0,stepT:0,prevY:resume?resume.y:level.spawn.y,riding:null,hasKey:false,crystals:0,checkpoint:resume||{x:level.spawn.x,y:level.spawn.y}});if(resume)for(const cp of level.checkpoints)if(resume.x>=cp.x&&resume.x<cp.x+T&&Math.abs(resume.y+P.h-cp.y)<1)cp.active=true;cam.x=cl(P.x-VW/2,0,Math.max(0,level.w*T-VW));cam.y=cl(P.y-VH/2,0,Math.max(0,level.h*T-VH));I.left=I.right=I.jump=false;I.jumpPressed=false;I.lanternPressed=false;state=ST.PLAY;stateT=0}
const tr=(c,r)=>(c<0||c>=level.w||r<0||r>=level.h)?0:level.tiles[r*level.w+c];
function isol(c,r){if(c<0||c>=level.w)return true;if(r<0||r>=level.h)return false;const t=level.tiles[r*level.w+c];if(t===1)return true;if(t===2)return P.lanternOn;if(t===4){const s=crSt.get(r*level.w+c);return!(s&&s.gone)}if(t===5||t===6)return true;return false}
function gcr(i){let s=crSt.get(i);if(!s){s={t:0,gone:false,regen:0};crSt.set(i,s)}return s}
function mvX(p,dt){p.x+=p.vx*dt;const r0=Math.floor(p.y/T),r1=Math.floor((p.y+p.h-.01)/T);if(p.vx>0){const cR=Math.floor((p.x+p.w-.01)/T);for(let r=r0;r<=r1;r++)if(isol(cR,r)){p.x=cR*T-p.w;p.vx=0;p.wallDir=1;break}}else if(p.vx<0){const cL=Math.floor(p.x/T);for(let r=r0;r<=r1;r++)if(isol(cL,r)){p.x=(cL+1)*T;p.vx=0;p.wallDir=-1;break}}}
function mvY(p,dt){p.y+=p.vy*dt;const c0=Math.floor(p.x/T),c1=Math.floor((p.x+p.w-.01)/T);if(p.vy>0){const rB=Math.floor((p.y+p.h-.01)/T);for(let c=c0;c<=c1;c++)if(isol(c,rB)){p.y=rB*T-p.h;p.vy=0;p.onGround=true;break}}else if(p.vy<0){const rT=Math.floor(p.y/T);for(let c=c0;c<=c1;c++)if(isol(c,rT)){p.y=(rT+1)*T;p.vy=0;break}}}
function uObj(dt){for(const o of level.objects){if(o.type==='saw'){o.t+=o.speed*dt*o.dir;if(o.t>=1){o.t=1;o.dir=-1}else if(o.t<=0){o.t=0;o.dir=1}o.x=o.ax+(o.bx-o.ax)*o.t;o.y=o.ay+(o.by-o.ay)*o.t}else if(o.type==='gear')o.rot+=o.spin*dt}}
function uCr(dt){if(P.onGround){const fy=Math.floor((P.y+P.h+2)/T),fx0=Math.floor(P.x/T),fx1=Math.floor((P.x+P.w-1)/T);for(let fx=fx0;fx<=fx1;fx++){if(fx<0||fx>=level.w||fy<0||fy>=level.h)continue;const idx=fy*level.w+fx;if(level.tiles[idx]===4){const st=gcr(idx);if(!st.gone&&st.t<=0)st.t=.34}}}crSt.forEach(st=>{if(st.t>0){st.t-=dt;if(st.t<=0){st.t=0;st.gone=true;st.regen=2.6}}else if(st.gone){st.regen-=dt;if(st.regen<=0){st.gone=false;st.regen=0}}})}
function uPl(dt){const p=P,dir=(I.right?1:0)-(I.left?1:0),wallContact=p.wallDir;p.dashT=Math.max(0,p.dashT-dt);p.dashCd=Math.max(0,p.dashCd-dt);p.wallDir=0;if(dir!==0){p.vx+=dir*AC*dt;p.facing=dir}else{const f=(p.onGround?FG:FA)*dt;if(Math.abs(p.vx)<=f)p.vx=0;else p.vx-=sg(p.vx)*f}p.vx=cl(p.vx,-MV,MV);
if(I.jumpPressed){p.jumpBuf=.14;I.jumpPressed=false}p.jumpBuf-=dt;p.coyote-=dt;
if(p.jumpBuf>0){if(p.onGround||p.coyote>0){p.vy=-JU;p.jumpsUsed=1;p.onGround=false;p.coyote=0;p.jumpBuf=0;SFX.jump()}else if(wallContact!==0){p.vy=-JU*.9;p.vx=-wallContact*MV*1.12;p.facing=-wallContact;p.jumpsUsed=1;p.jumpBuf=0;SFX.jump()}else if(p.jumpsUsed<2){p.vy=-JU*.88;p.jumpsUsed=2;p.jumpBuf=0;SFX.jump()}}
if(!I.jump&&p.vy<0)p.vy+=GR*1.8*dt;else if(I.jump&&p.vy<0)p.vy-=GR*.22*dt;p.vy+=GR*dt;if(p.vy>MF)p.vy=MF;if(p.dashT>0)p.vx=p.dashDir*MV*2.35;if(level.def.w)p.vx+=level.def.w*dt;
if(p.riding){p.x+=p.riding.dx||0;p.y+=p.riding.dy||0}p.riding=null;p.prevY=p.y;p.onGround=false;mvX(p,dt);if(p.wallDir!==0&&!p.onGround&&p.vy>95)p.vy=95;mvY(p,dt);if(p.onGround){p.jumpsUsed=0;p.dashT=0}
if(p.vy>=0)for(const o of level.objects){if(o.type!=='plat')continue;const pb=p.prevY+p.h,nb=p.y+p.h;if(pb<=o.y+3&&nb>=o.y&&p.x+p.w>o.x+1&&p.x<o.x+o.w-1){p.y=o.y-p.h;p.vy=0;p.onGround=true;p.riding=o}}
if(p.onGround){p.coyote=.1;p.animT+=Math.abs(p.vx)*dt*.09;p.stepT-=dt;if(Math.abs(p.vx)>20&&p.stepT<=0){SFX.step();p.stepT=.22}}else p.animT+=dt*2;
for(const cp of level.checkpoints){if(!cp.active&&p.onGround&&p.x+p.w>cp.x&&p.x<cp.x+T&&Math.abs(p.y+p.h-cp.y)<1){cp.active=true;p.checkpoint={x:cl(cp.x+(T-p.w)/2,0,level.w*T-p.w),y:cp.y-p.h};SFX.key()}}
for(const k of level.keys){if(k.got)continue;const dx=(p.x+p.w/2)-k.x,dy=(p.y+p.h/2)-k.y;if(dx*dx+dy*dy<200){k.got=true;p.hasKey=true;SFX.key()}}
for(const cr of level.crystals){if(cr.got)continue;const dx=(p.x+p.w/2)-cr.x,dy=(p.y+p.h/2)-cr.y;if(dx*dx+dy*dy<220){cr.got=true;p.crystals++;SFX.crystal()}}
for(const dr of level.doors){if(dr.open)continue;const dx=(p.x+p.w/2)-(dr.cx*T+8),dy=(p.y+p.h/2)-(dr.cy*T+4);if(dx*dx+dy*dy<420&&p.hasKey){dr.open=true;for(let k=0;k<3;k++){const ry=dr.cy-k;if(ry>=0)level.tiles[ry*level.w+dr.cx]=0}SFX.door()}}
if(level.goal){const dx=(p.x+p.w/2)-level.goal.x,dy=(p.y+p.h/2)-level.goal.y,need=(levelIndex===LVS.length-1),ok=(!need)||(p.crystals>=level.crystals.length);if(ok&&dx*dx+dy*dy<300)winLv()}}
function chkH(){const p=P;if(p.dashT>0)return;const c0=Math.floor(p.x/T),c1=Math.floor((p.x+p.w-1)/T),r0=Math.floor(p.y/T),r1=Math.floor((p.y+p.h-1)/T);for(let r=r0;r<=r1;r++)for(let c=c0;c<=c1;c++)if(tr(c,r)===3){const sx=c*T+2,sy=r*T+7,sw=T-4,sh=T-7;if(p.x<sx+sw&&p.x+p.w>sx&&p.y<sy+sh&&p.y+p.h>sy){die();return}}const pcx=p.x+p.w/2,pcy=p.y+p.h/2;for(const o of level.objects)if(o.type==='saw'||o.type==='gear'){const dx=pcx-o.x,dy=pcy-o.y,rr=o.r+3;if(dx*dx+dy*dy<rr*rr){die();return}}if(p.y>level.h*T+40)die()}
function die(){if(state!==ST.PLAY)return;P.alive=false;state=ST.DEAD;stateT=0;SFX.hurt()}
function winLv(){if(state!==ST.PLAY)return;state=ST.WIN;stateT=0;SFX.win();save.cleared[levelIndex]=true;if(levelIndex+1<LVS.length)save.unlocked=Math.max(save.unlocked,levelIndex+2);else save.unlocked=LVS.length;ws()}
function uCam(dt){let tx=P.x+P.w/2-VW/2,ty=P.y+P.h/2-VH/2;const mx=Math.max(0,level.w*T-VW),my=Math.max(0,level.h*T-VH);tx=cl(tx,0,mx);ty=cl(ty,0,my);const k=1-Math.pow(.001,dt);cam.x=lp(cam.x,tx,k);cam.y=lp(cam.y,ty,k);if(level.w*T<VW)cam.x=(level.w*T-VW)/2;if(level.h*T<VH)cam.y=(level.h*T-VH)/2}
function dBg(){const d=level.def;cx.fillStyle=CD;cx.fillRect(0,0,VW,VH);if(d.bg==='cave'||d.d>0.8){/* cave grain and distant mineral flecks */cx.fillStyle=PT.W25;for(let i=0;i<Math.ceil(VW/32);i++){const x=(i*31+7)%VW,y=(i*23+11)%VH;if((i%3)!==0)cx.fillRect(x,y,2,1)}return}/* distant broken skyline */const ox=cam.x*.08,oy=cam.y*.025;cx.fillStyle=PT.W25;for(let i=-1;i<Math.ceil(VW/92)+2;i++){const x=i*92-(ox%92),h=18+Math.abs((i*23+41)%31);cx.beginPath();cx.moveTo(x-10,VH-25-oy);cx.lineTo(x+17,VH-h-oy);cx.lineTo(x+38,VH-h+8-oy);cx.lineTo(x+67,VH-21-oy);cx.closePath();cx.fill();if(d.bg==='ruins'||d.bg==='castle'){cx.fillRect(Math.round(x+24),Math.round(VH-h-oy-5),2,7);cx.fillRect(Math.round(x+19),Math.round(VH-h-oy-5),12,1)}}/* crescent moon with pixel halo */const mx=Math.round(VW*.79-cam.x*.018),my=24-Math.round(cam.y*.018);cx.fillStyle=PT.W25;cx.beginPath();cx.arc(mx,my,15,0,Math.PI*2);cx.fill();cx.fillStyle=CD;cx.beginPath();cx.arc(mx-4,my-3,13,0,Math.PI*2);cx.fill();cx.fillStyle=CL;cx.fillRect(mx+8,my-2,2,2);cx.fillRect(mx+4,my+7,1,1);cx.fillRect(mx+12,my+5,1,1);/* stars: slow, layered parallax */cx.fillStyle=CL;for(let i=0;i<24;i++){const x=((i*53+17-Math.floor(cam.x*(i%3===0?.035:.012)))%(VW+12)+VW+12)%(VW+12)-6,y=(i*29+13)%Math.max(30,VH-44);if(i%5===0){cx.fillRect(x-1,y,3,1);cx.fillRect(x,y-1,1,3)}else cx.fillRect(x,y,1,1)}/* foreground silhouettes */const ox2=cam.x*.27,sp=46;cx.fillStyle=PT.W50;for(let j=Math.floor(ox2/sp)-1;j<Math.floor(ox2/sp)+Math.ceil(VW/sp)+3;j++){const x=j*sp-ox2+23,h=29+Math.abs((j*37)%49),base=VH+13-cam.y*.022;cx.beginPath();cx.moveTo(x-16,base);cx.lineTo(x,base-h);cx.lineTo(x+16,base);cx.closePath();cx.fill();cx.fillRect(Math.round(x-2),Math.round(base-4),4,8);if(d.bg==='forest'&&j%3===0){cx.fillStyle=PT.W25;cx.fillRect(Math.round(x-6),Math.round(base-h+12),2,2);cx.fillRect(Math.round(x+4),Math.round(base-h+20),1,2);cx.fillStyle=PT.W50}}/* subtle fog bands */cx.fillStyle=PT.W25;for(let i=0;i<4;i++){const x=((i*79+Math.floor(stateT*2))%(VW+50))-25,y=VH-35-(i%3)*12;if(x<VW)cx.fillRect(x,y,16+(i%3)*5,1)}}
function dSt(){if(level.def.d>.5)return;cx.fillStyle=CL;for(const s of stars){const sx=Math.round(s.x-cam.x*.05),sy=Math.round(s.y-cam.y*.05);if(sx<-2||sx>VW+2||sy<-2||sy>VH+2)continue;cx.fillRect(sx,sy,1,1)}}
function dSol(px,py,tx,ty){const top=tr(tx,ty-1)!==1;cx.fillStyle=CL;cx.fillRect(px,py,T,T);/* carved stone face */cx.fillStyle=PT.B25;cx.fillRect(px,py+2,T,2);cx.fillStyle=PT.B50;cx.fillRect(px,py+9,T,7);cx.fillStyle=CD;cx.fillRect(px,py+15,T,1);if(top){cx.fillStyle=CL;cx.fillRect(px,py,T,1);cx.fillStyle=PT.B25;cx.fillRect(px+2,py+1,T-4,1);/* tiny grassless stone chips */if((tx*7+ty*3)%4===0){cx.fillStyle=CD;cx.fillRect(px+3,py+4,1,1);cx.fillRect(px+5,py+5,2,1)}if((tx+ty)%5===0){cx.fillStyle=PT.B50;cx.fillRect(px+10,py+5,3,1)}}if((tx+ty)%3===0){cx.fillStyle=PT.B25;cx.fillRect(px+3,py+7,4,1);cx.fillRect(px+11,py+12,2,1)}if((tx*3+ty)%7===0){cx.fillStyle=CD;cx.fillRect(px+8,py+3,1,3);cx.fillRect(px+7,py+5,2,1)}}
function dHid(px,py){if(P.lanternOn){cx.fillStyle=CL;cx.fillRect(px,py,T,T);cx.fillStyle=PT.B50;cx.fillRect(px,py+10,T,6);cx.fillStyle=CD;cx.fillRect(px,py,T,1);cx.fillRect(px,py+T-1,T,1)}else{cx.fillStyle=PT.W25;cx.fillRect(px+1,py+1,T-2,T-2);cx.fillStyle=PT.W50;cx.fillRect(px,py,T,2);cx.fillRect(px,py+T-2,T,2)}}
function dSpk(px,py){cx.fillStyle=PT.W25;cx.fillRect(px+2,py+T-2,T-4,2);cx.fillStyle=CL;cx.beginPath();cx.moveTo(px,py+T);cx.lineTo(px+3,py+T-5);cx.lineTo(px+8,py+3);cx.lineTo(px+12,py+T-5);cx.lineTo(px+T,py+T);cx.closePath();cx.fill();cx.fillStyle=PT.B25;cx.fillRect(px+4,py+T-4,2,2);cx.fillRect(px+10,py+T-4,2,2);cx.fillStyle=CD;cx.fillRect(px+7,py+8,2,3);cx.fillRect(px+6,py+11,4,1)}
function dCrm(px,py,idx){const st=crSt.get(idx);if(st&&st.gone){cx.fillStyle=PT.W25;cx.fillRect(px,py+T-3,T,3);return}cx.fillStyle=CL;cx.fillRect(px,py,T,8);cx.fillStyle=PT.B50;cx.fillRect(px,py+4,T,4);cx.fillStyle=CD;cx.fillRect(px,py+7,T,1);if(st&&st.t>0){cx.fillStyle=(Math.floor(st.t*24)%2===0)?CD:CL;cx.fillRect(px+4,py+2,2,2);cx.fillRect(px+10,py+2,2,2)}}
function dDor(px,py){cx.fillStyle=PT.W50;cx.fillRect(px,py,T,T);cx.fillStyle=CL;cx.fillRect(px+2,py+2,T-4,T-4);cx.fillStyle=CD;cx.fillRect(px+6,py+6,4,4)}
function dCkp(px,py,active){cx.fillStyle=active?CL:PT.W50;cx.fillRect(px+2,py+2,2,T-3);cx.fillStyle=active?CL:PT.W25;cx.beginPath();cx.moveTo(px+4,py+2);cx.lineTo(px+13,py+5);cx.lineTo(px+4,py+9);cx.closePath();cx.fill();cx.fillStyle=active?CL:CD;cx.fillRect(px+1,py+T-2,T-2,1)}
function dKey(k){const px=Math.round(k.x-cam.x),py=Math.round(k.y-cam.y+Math.sin(k.t*3)*2);cx.fillStyle=PT.W25;cx.fillRect(px-4,py-5,8,11);cx.fillStyle=CL;cx.fillRect(px-2,py-4,4,4);cx.fillRect(px-1,py,2,5);cx.fillRect(px-1,py+3,4,1);cx.fillStyle=CD;cx.fillRect(px-1,py-3,2,2);cx.fillRect(px+2,py+3,1,1)}
function dCry(c){const px=Math.round(c.x-cam.x),py=Math.round(c.y-cam.y+Math.sin(c.t*2.4)*2);cx.fillStyle=PT.W25;cx.fillRect(px-6,py-7,12,14);cx.fillStyle=CL;cx.beginPath();cx.moveTo(px,py-7);cx.lineTo(px+5,py);cx.lineTo(px,py+7);cx.lineTo(px-5,py);cx.closePath();cx.fill();cx.fillStyle=PT.B50;cx.beginPath();cx.moveTo(px,py-4);cx.lineTo(px+2,py);cx.lineTo(px,py+4);cx.lineTo(px-2,py);cx.closePath();cx.fill();cx.fillStyle=CD;cx.fillRect(px-1,py-1,2,2);cx.fillStyle=CL;cx.fillRect(px-1,py-5,1,2)}
function dGol(){const g=level.goal,px=Math.round(g.x-cam.x),py=Math.round(g.y-cam.y+Math.sin(g.t*2)*2);cx.fillStyle=PT.W50;cx.beginPath();cx.arc(px,py,12,0,Math.PI*2);cx.fill();cx.fillStyle=CL;cx.beginPath();cx.moveTo(px,py-8);cx.lineTo(px+5,py);cx.lineTo(px,py+8);cx.lineTo(px-5,py);cx.closePath();cx.fill();cx.fillStyle=CD;cx.fillRect(px-1,py-1,2,2)}
function dObj(){const now=performance.now();for(const o of level.objects){const px=Math.round(o.x-cam.x),py=Math.round(o.y-cam.y);if(o.type==='saw'){/* dotted motion echo */cx.fillStyle=PT.W25;for(let k=3;k>=1;k--){const a=now*.004-k*.48;cx.fillRect(Math.round(px+Math.cos(a)*(o.r+5)),Math.round(py+Math.sin(a)*(o.r+5)),1,1)}cx.fillStyle=CL;for(let a=0;a<8;a++){const ang=a*Math.PI/4+now*.004;cx.fillRect(Math.round(px+Math.cos(ang)*(o.r+2))-1,Math.round(py+Math.sin(ang)*(o.r+2))-1,3,3)}cx.beginPath();cx.arc(px,py,o.r,0,Math.PI*2);cx.fill();cx.fillStyle=CD;cx.beginPath();cx.arc(px,py,Math.max(2,o.r-4),0,Math.PI*2);cx.fill();cx.fillStyle=CL;cx.fillRect(px-1,py-1,2,2)}else if(o.type==='gear'){cx.save();cx.translate(px,py);cx.rotate(o.rot);cx.fillStyle=CL;for(let a=0;a<8;a++){cx.save();cx.rotate(a*Math.PI/4);cx.fillRect(-2,-o.r-3,4,5);cx.restore()}cx.beginPath();cx.arc(0,0,o.r,0,Math.PI*2);cx.fill();cx.fillStyle=CD;cx.beginPath();cx.arc(0,0,o.r-4,0,Math.PI*2);cx.fill();cx.fillStyle=CL;cx.fillRect(-2,-2,4,4);cx.fillStyle=CD;cx.fillRect(-1,-1,2,2);cx.restore()}else if(o.type==='plat'){cx.fillStyle=CL;cx.fillRect(px,py,o.w,o.h);cx.fillStyle=PT.B50;cx.fillRect(px,py+o.h-4,o.w,4);cx.fillStyle=PT.B25;cx.fillRect(px+2,py+2,Math.max(0,o.w-4),1)}}}
function dPl(){const p=P,px=Math.round(p.x-cam.x),py=Math.round(p.y-cam.y);if(!p.alive){cx.fillStyle=PT.W50;cx.fillRect(px,py+8,9,5);cx.fillStyle=CL;cx.fillRect(px+1,py+10,2,1);return}const walk=p.onGround&&Math.abs(p.vx)>15,fr=walk?(Math.floor(p.animT)%2):0;cx.fillStyle=PT.W25;cx.fillRect(px-2,py+13,13,1);cx.fillStyle=CL;cx.fillRect(px+1,py,7,5);cx.fillStyle=CD;cx.fillRect(px+2,py+1,5,2);cx.fillStyle=CL;cx.fillRect(px+(p.facing>0?5:2),py+2,2,1);cx.fillRect(px+2,py+5,5,5);cx.fillStyle=PT.B50;cx.fillRect(px+2,py+6,5,3);cx.fillStyle=CL;cx.fillRect(px+(p.facing>0?7:0),py+5,2,4);cx.fillStyle=CL;if(walk){if(fr===0){cx.fillRect(px+1,py+10,3,3);cx.fillRect(px+6,py+10,2,2)}else{cx.fillRect(px+2,py+10,2,2);cx.fillRect(px+5,py+10,3,3)}}else{cx.fillRect(px+2,py+10,2,3);cx.fillRect(px+5,py+10,2,3)}if(p.lanternOn){cx.fillStyle=CL;cx.fillRect(px+(p.facing>0?8:0),py+7,1,1)}}
function dDrk(){const d=level.def;if(!d.d)return;const fO=(P.facing>0)?7:-7;const lx=Math.round(P.x+P.w/2+fO-cam.x),ly=Math.round(P.y+P.h/2-cam.y);const R=P.lanternOn?92-d.d*30:30;cx.fillStyle=CD;cx.beginPath();cx.rect(0,0,VW,VH);cx.moveTo(lx+R,ly);cx.arc(lx,ly,R,0,Math.PI*2,true);cx.fill();cx.save();cx.beginPath();cx.arc(lx,ly,R+9,0,Math.PI*2);cx.arc(lx,ly,R,0,Math.PI*2,true);cx.fillStyle=PT.B50;cx.fill();cx.beginPath();cx.arc(lx,ly,R+18,0,Math.PI*2);cx.arc(lx,ly,R+9,0,Math.PI*2,true);cx.fillStyle=PT.B25;cx.fill();cx.restore()}
function dWrld(){const cx0=Math.max(0,Math.floor(cam.x/T)),cx1=Math.min(level.w-1,Math.floor((cam.x+VW)/T)),cy0=Math.max(0,Math.floor(cam.y/T)),cy1=Math.min(level.h-1,Math.floor((cam.y+VH)/T)),dk=level.def.d;let lX=0,lY=0,lR=0;if(dk>0){const fO=(P.facing>0)?7:-7;lX=P.x+P.w/2+fO;lY=P.y+P.h/2;lR=P.lanternOn?(92-dk*30)+22:50}
for(let y=cy0;y<=cy1;y++)for(let x=cx0;x<=cx1;x++){const t=level.tiles[y*level.w+x];if(t===0)continue;if(dk>.5){const tcx=x*T+8,tcy=y*T+8,ddx=tcx-lX,ddy=tcy-lY;if(ddx*ddx+ddy*ddy>lR*lR)continue}const px=Math.round(x*T-cam.x),py=Math.round(y*T-cam.y);if(t===1)dSol(px,py,x,y);else if(t===2)dHid(px,py);else if(t===3)dSpk(px,py);else if(t===4)dCrm(px,py,y*level.w+x);else if(t===5)dDor(px,py);else if(t===6){const cp=level.checkpoints.find(c=>c.x===x*T&&c.y===y*T);dCkp(px,py,!!(cp&&cp.active))}}
for(const k of level.keys)if(!k.got)dKey(k);for(const c of level.crystals)if(!c.got)dCry(c);dObj();if(level.goal)dGol();dPl();dDrk()}
function gBtn(){ return []; }
function dBtn(b,a){const x=b.x,y=b.y,w=b.w,h=b.h;if(b.id==='pause'){cx.fillStyle=a?CL:PT.W50;cx.fillRect(x,y,w,h);cx.fillStyle=a?CD:CL;cx.fillRect(x+5,y+4,3,h-8);cx.fillRect(x+12,y+4,3,h-8);return}cx.fillStyle=a?CL:PT.W50;cx.fillRect(x,y,w,h);cx.fillStyle=a?CD:CL;if(b.id==='left'){for(let i=0;i<7;i++){cx.fillRect(x+12+i,y+10+i,2,2);cx.fillRect(x+12+i,y+h-12-i,2,2)}cx.fillRect(x+12,y+16,2,h-32)}else if(b.id==='right'){for(let j=0;j<7;j++){cx.fillRect(x+w-14-j,y+10+j,2,2);cx.fillRect(x+w-14-j,y+h-12-j,2,2)}cx.fillRect(x+w-14,y+16,2,h-32)}else if(b.id==='jump'){dtc('JUMP',x+w/2,y+h/2-5,1,a?CD:CL);dtc('^',x+w/2-4,y+6,1,a?CD:CL)}else if(b.id==='lantern'){const lx=x+w/2,ly=y+h/2;cx.beginPath();cx.moveTo(lx,ly-9);cx.lineTo(lx+6,ly);cx.lineTo(lx,ly+9);cx.lineTo(lx-6,ly);cx.closePath();cx.fill();if(a){cx.fillStyle=CL;cx.fillRect(lx-2,ly-2,4,4)}}}
function dHUD(){const B=gBtn();for(const b of B){let a=false;if(b.id==='left')a=I.left;else if(b.id==='right')a=I.right;else if(b.id==='jump')a=I.jump;else if(b.id==='lantern')a=P.lanternOn;dBtn(b,a)}const title=LVS[levelIndex].n;cx.fillStyle=CD;cx.fillRect(2,2,Math.min(VW-4,tw(title,1)+10),11);cx.fillStyle=PT.W50;cx.fillRect(2,2,Math.min(VW-4,tw(title,1)+10),1);cx.fillRect(2,12,Math.min(VW-4,tw(title,1)+10),1);dt(title,6,5,1,CL);if(level.keys.length>0){const kx=6,ky=16;cx.fillStyle=P.hasKey?CL:PT.W50;cx.fillRect(kx,ky,3,3);cx.fillRect(kx+1,ky+3,1,4);cx.fillRect(kx+1,ky+5,3,1)}if(levelIndex===LVS.length-1){const tt=level.crystals.length,gt=P.crystals,bx=6,by=26;for(let c=0;c<tt;c++){cx.fillStyle=(c<gt)?CL:PT.W50;cx.beginPath();cx.moveTo(bx+c*8+3,by);cx.lineTo(bx+c*8+6,by+4);cx.lineTo(bx+c*8+3,by+8);cx.lineTo(bx+c*8,by+4);cx.closePath();cx.fill()}}}
function dMenu(){
  cx.fillStyle=CD;cx.fillRect(0,0,VW,VH);
  const sx=Math.round(VW/2),ground=VH;
  // Deep-space stars: deterministic twinkle keeps the pixel-art scene stable.
  for(let i=0;i<stars.length;i++){
    const st=stars[i],pulse=Math.sin(stateT*2.1+i*1.7);
    const x=Math.round((st.x+(stateT*(st.s*1.3+.12))%VW)%VW),y=Math.round(st.y);
    if(y>122&&y<VH-35)continue;
    cx.fillStyle=(pulse>.58&&st.s>.48)?CL:PT.W50;
    const z=st.s>.88?2:1;cx.fillRect(x,y,z,z);
    if(st.s>.96&&pulse>.35){cx.fillRect(x-2,y,5,1);cx.fillRect(x,y-2,1,5)}
  }
  // Distant skyline with stepped rooftops and tiny lit windows.
  const skyline=[
    {x:8,w:18,h:48},{x:30,w:15,h:78},{x:48,w:20,h:58},
    {x:VW-68,w:18,h:64},{x:VW-48,w:17,h:88},{x:VW-28,w:20,h:52}
  ];
  for(let i=0;i<skyline.length;i++){
    const b=skyline[i],top=ground-b.h;
    cx.fillStyle=PT.W25;cx.fillRect(b.x,top,b.w,b.h);
    cx.fillStyle=CD;cx.fillRect(b.x+2,top+2,b.w-4,b.h);
    cx.fillStyle=PT.W50;
    for(let wy=top+7;wy<ground-7;wy+=8)for(let wx=b.x+4;wx<b.x+b.w-3;wx+=6)
      if(((wx+wy+i*3)%3)!==0)cx.fillRect(wx,wy,2,2);
    cx.fillStyle=CL;cx.fillRect(b.x+Math.floor(b.w/2)-1,top-3,2,3);
  }
  // The Spire: a stepped silhouette, inner stonework and a bright summit.
  cx.fillStyle=PT.W25;
  cx.beginPath();cx.moveTo(sx-43,ground);cx.lineTo(sx-30,ground-47);
  cx.lineTo(sx-23,ground-47);cx.lineTo(sx-18,ground-94);
  cx.lineTo(sx-12,ground-94);cx.lineTo(sx-7,ground-137);
  cx.lineTo(sx,ground-157);cx.lineTo(sx+7,ground-137);
  cx.lineTo(sx+12,ground-94);cx.lineTo(sx+18,ground-94);
  cx.lineTo(sx+23,ground-47);cx.lineTo(sx+30,ground-47);
  cx.lineTo(sx+43,ground);cx.closePath();cx.fill();
  cx.fillStyle=PT.W50;
  cx.beginPath();cx.moveTo(sx-37,ground);cx.lineTo(sx-25,ground-47);
  cx.lineTo(sx-15,ground-94);cx.lineTo(sx-5,ground-137);
  cx.lineTo(sx,ground-153);cx.lineTo(sx+5,ground-137);
  cx.lineTo(sx+15,ground-94);cx.lineTo(sx+25,ground-47);
  cx.lineTo(sx+37,ground);cx.closePath();cx.fill();
  // Vertical ribs and scattered masonry pixels.
  cx.fillStyle=CD;
  cx.fillRect(sx-2,ground-126,1,ground-(ground-12));
  cx.fillRect(sx+2,ground-126,1,ground-(ground-12));
  for(let y=ground-118,row=0;y<ground-8;y+=11,row++){
    const half=Math.max(2,Math.floor((y-(ground-150))*.28));
    cx.fillStyle=(row%2===0)?PT.B25:PT.B50;
    cx.fillRect(sx-half,y,half*2,2);
    if(row%2===0){cx.fillStyle=CL;cx.fillRect(sx-half-3,y+4,2,2);cx.fillRect(sx+half+1,y+4,2,2)}
  }
  // Beacon and a narrow checkerboard beam reaching into the sky.
  const beaconY=ground-166;
  cx.fillStyle=CL;cx.beginPath();cx.moveTo(sx,beaconY-9);cx.lineTo(sx+5,beaconY+1);
  cx.lineTo(sx+3,beaconY+10);cx.lineTo(sx,beaconY+15);
  cx.lineTo(sx-3,beaconY+10);cx.lineTo(sx-5,beaconY+1);cx.closePath();cx.fill();
  cx.fillStyle=PT.W50;
  cx.beginPath();cx.moveTo(sx-2,beaconY+15);cx.lineTo(sx+2,beaconY+15);
  cx.lineTo(sx+22,ground-28);cx.lineTo(sx-22,ground-28);cx.closePath();cx.fill();
  cx.fillStyle=CD;cx.fillRect(sx-1,beaconY-3,2,7);
  // Thin horizon and isolated foreground stones give the scene depth.
  cx.fillStyle=PT.W50;cx.fillRect(0,ground-1,VW,1);
  for(let i=0;i<18;i++){const x=(i*37+13)%VW,y=ground-((i*19)%13);cx.fillStyle=(i%3===0)?CL:PT.W50;cx.fillRect(x,y,1+(i%2),1)}
  const bob=Math.sin(stateT*2)*1.5;
  // Small ornamental rules frame the title without competing with it.
  cx.fillStyle=PT.W50;cx.fillRect(Math.round(VW*.19),19,Math.round(VW*.62),1);
  cx.fillRect(Math.round(VW*.27),69,Math.round(VW*.46),1);
  dtc('SHADOW',VW/2,24+bob,4,CL);dtc('SPIRE',VW/2,47+bob,4,CL);
  dtc('A 1-BIT JOURNEY TO THE STARS',VW/2,76,1,CL);
  dtc('ACTION: JUMP / AIR JUMP',VW/2,91,1,CL);
  dtc('DOUBLE TAP L/R: DASH',VW/2,102,1,CL);
  dtc('WALL + ACTION: WALL JUMP',VW/2,113,1,CL);
  if(Math.floor(stateT*1.6)%2===0){
    const label='TAP TO START',sc=2,tw0=tw(label,sc),bx=Math.round((VW-tw0-12)/2),by=VH-49;
    cx.fillStyle=CD;cx.fillRect(bx,by,tw0+12,16);cx.fillStyle=PT.W50;
    cx.fillRect(bx,by,tw0+12,1);cx.fillRect(bx,by+15,tw0+12,1);
    cx.fillRect(bx,by,1,16);cx.fillRect(bx+tw0+11,by,1,16);dtc(label,VW/2,by+3,sc,CL)
  }
  if(save.unlocked>1){
    const label='PROGRESS: '+save.unlocked+'/'+LVS.length,sc=1,tw0=tw(label,sc),bx=Math.round((VW-tw0-8)/2),by=VH-25;
    cx.fillStyle=CD;cx.fillRect(bx,by,tw0+8,8);cx.fillStyle=PT.W50;
    cx.fillRect(bx,by,tw0+8,1);cx.fillRect(bx,by+7,tw0+8,1);dtc(label,VW/2,by+1,sc,CL)
  }
}function dLS(){cx.fillStyle=CD;cx.fillRect(0,0,VW,VH);dtc('SELECT LEVEL',VW/2,14,2,CL);const cols=2,tw2=34,th=18,gap=4,tW=cols*tw2+(cols-1)*gap,sX=Math.round((VW-tW)/2),sY=34;for(let i=0;i<LVS.length;i++){const col=i%cols,row=Math.floor(i/cols),x=sX+col*(tw2+gap),y=sY+row*(th+gap),un=(i<save.unlocked),clr=!!save.cleared[i];if(un){cx.fillStyle=clr?CL:PT.W50;cx.fillRect(x,y,tw2,th);cx.fillStyle=CD;dtc(String(i+1),x+tw2/2,y+10,2,CD);if(clr){cx.fillRect(x+4,y+th-6,4,2);cx.fillRect(x+9,y+th-8,4,4)}}else{cx.fillStyle=PT.W25;cx.fillRect(x,y,tw2,th);dtc('-',x+tw2/2,y+10,2,CL)}}dtc('TAP LEVEL TO PLAY',VW/2,VH-24,1,CL);cx.fillStyle=PT.W50;cx.fillRect(6,6,26,16);dtc('<',18,10,1,CL)}
function dOv(title,sub){cx.fillStyle=CD;cx.fillRect(0,0,VW,VH);const bW=Math.min(VW-24,220),bH=74,bx=Math.round((VW-bW)/2),by=Math.round((VH-bH)/2);cx.fillStyle=CL;cx.fillRect(bx,by,bW,bH);cx.fillStyle=CD;cx.fillRect(bx+2,by+2,bW-4,bH-4);cx.fillStyle=CL;cx.fillRect(bx+4,by+4,bW-8,bH-8);dtc(title,VW/2,by+14,2,CD);if(sub&&Math.floor(stateT*2)%2===0)dtc(sub,VW/2,by+44,1,CD)}
function dEnd(){cx.fillStyle=CD;cx.fillRect(0,0,VW,VH);cx.fillStyle=CL;for(let i=0;i<stars.length;i++){const s=stars[i],tw3=(Math.sin(stateT*3+i)+1)*.5;if(tw3>.4)cx.fillRect(Math.round(s.x),Math.round(s.y),1,1)}cx.fillStyle=PT.W50;const sx=VW/2;cx.beginPath();cx.moveTo(sx-30,VH);cx.lineTo(sx,VH-120);cx.lineTo(sx+30,VH);cx.closePath();cx.fill();cx.fillStyle=CL;cx.beginPath();cx.moveTo(sx,VH-140);cx.lineTo(sx+6,VH-124);cx.lineTo(sx,VH-108);cx.lineTo(sx-6,VH-124);cx.closePath();cx.fill();dtc('THE SPIRE AWAKENS',VW/2,26,2,CL);dtc('LIGHT RETURNS TO THE',VW/2,50,1,CL);dtc('PIXEL WORLD',VW/2,62,1,CL);if(Math.floor(stateT*1.6)%2===0)dtc('TAP TO RETURN',VW/2,VH-26,1,CL)}
function rd(){cx.fillStyle=CD;cx.fillRect(0,0,VW,VH);if(state===ST.MENU)return dMenu();if(state===ST.LEVELS)return dLS();if(state===ST.ENDING)return dEnd();dBg();dSt();dWrld();if(level.def.inv&&!P.lanternOn){cx.globalCompositeOperation='difference';cx.fillStyle=CL;cx.fillRect(0,0,VW,VH);cx.globalCompositeOperation='source-over'}dHUD();if(state===ST.PAUSE)dOv('PAUSED','TAP TO RESUME');else if(state===ST.DEAD)dOv('YOU FELL','TAP TO RETRY');else if(state===ST.WIN){const isLast=(levelIndex===LVS.length-1);dOv(isLast?'SPIRE IGNITED':'LEVEL CLEARED',isLast?'TAP TO CONTINUE':'TAP FOR NEXT LEVEL')}}
let lastT=0;
function up(dt){stateT+=dt;if(state===ST.PLAY){if(I.lanternPressed){P.lanternOn=!P.lanternOn;I.lanternPressed=false;if(P.lanternOn)SFX.lampOn();else SFX.lampOff()}uObj(dt);uPl(dt);uCr(dt);chkH();uCam(dt);for(const k of level.keys)k.t+=dt;for(const c of level.crystals)c.t+=dt;if(level.goal)level.goal.t+=dt}else if(state===ST.DEAD||state===ST.WIN)for(const o of level.objects)if(o.type==='gear')o.rot+=o.spin*dt}
function loop(ts){if(!lastT)lastT=ts;let dt=(ts-lastT)/1000;lastT=ts;if(dt>.05)dt=.05;if(dt<0)dt=0;up(dt);rd();requestAnimationFrame(loop)}
const pts=new Map();
function tV(clientX,clientY){const r=cv.getBoundingClientRect();return{x:(clientX-r.left)/r.width*VW,y:(clientY-r.top)/r.height*VH}}
function hB(vx,vy){for(const b of gBtn())if(vx>=b.x&&vx<=b.x+b.w&&vy>=b.y&&vy<=b.y+b.h)return b.id;return null}
function prB(id){switch(id){case 'left':{I.left=true;const now=performance.now();if(now-dirTap.left<280&&P.dashCd<=0){P.dashT=.13;P.dashCd=.85;P.dashDir=-1;P.facing=-1}dirTap.left=now;break}case 'right':{I.right=true;const now=performance.now();if(now-dirTap.right<280&&P.dashCd<=0){P.dashT=.13;P.dashCd=.85;P.dashDir=1;P.facing=1}dirTap.right=now;break}case 'jump':if(!I.jump)I.jumpPressed=true;I.jump=true;break;case 'lantern':I.lanternPressed=true;break;case 'pause':SFX.ui();state=ST.PAUSE;stateT=0;I.left=I.right=I.jump=false;I.jumpPressed=I.lanternPressed=false;pts.clear();break}}
function rlB(id){switch(id){case 'left':I.left=false;break;case 'right':I.right=false;break;case 'jump':I.jump=false;break}}
function hMenu(vx,vy){if(state===ST.PAUSE){SFX.ui();state=ST.PLAY;stateT=0;return}if(state===ST.MENU){SFX.ui();if(save.unlocked>1&&vx>VW/2-60&&vx<VW/2+60&&vy>VH-46&&vy<VH-26){state=ST.LEVELS;stateT=0}else if(save.unlocked>1){state=ST.LEVELS;stateT=0}else stl(0);return}if(state===ST.LEVELS){if(vx<34&&vy<24){SFX.ui();state=ST.MENU;stateT=0;return}const cols=2,tw2=34,th=18,gap=4,tW=cols*tw2+(cols-1)*gap,sX=Math.round((VW-tW)/2),sY=34;for(let i=0;i<LVS.length;i++){const col=i%cols,row=Math.floor(i/cols),x=sX+col*(tw2+gap),y=sY+row*(th+gap);if(vx>=x&&vx<=x+tw2&&vy>=y&&vy<=y+th){if(i<save.unlocked){SFX.ui();stl(i)}return}}return}if(state===ST.DEAD){SFX.ui();stl(levelIndex,true);return}if(state===ST.WIN){SFX.ui();if(levelIndex+1<LVS.length)stl(levelIndex+1);else{state=ST.ENDING;stateT=0}return}if(state===ST.ENDING){SFX.ui();state=ST.MENU;stateT=0}}
function onDown(e){e.preventDefault();ia();const v=tV(e.clientX,e.clientY);if(state===ST.MENU||state===ST.LEVELS||state===ST.DEAD||state===ST.WIN||state===ST.ENDING||state===ST.PAUSE){hMenu(v.x,v.y);return}if(state===ST.PLAY){const id=hB(v.x,v.y);if(id){pts.set(e.pointerId,id);if(cv.setPointerCapture){try{cv.setPointerCapture(e.pointerId)}catch(_){}}prB(id)}}}
function onUp(e){e.preventDefault();const id=pts.get(e.pointerId);if(id){rlB(id);pts.delete(e.pointerId)}}
function onMove(e){e.preventDefault();if(state!==ST.PLAY)return;const id=pts.get(e.pointerId);if(!id||id==='jump'||id==='lantern'||id==='pause')return;const v=tV(e.clientX,e.clientY),nid=hB(v.x,v.y);if(nid!==id&&(nid==='left'||nid==='right')){rlB(id);pts.set(e.pointerId,nid);prB(nid)}}
function onCancel(e){const id=pts.get(e.pointerId);if(id){rlB(id);pts.delete(e.pointerId)}}
function onKD(e){ia();const k=e.key.toLowerCase();if(state===ST.PAUSE){if(k==='escape'||k===' '||k==='enter'){e.preventDefault();state=ST.PLAY;stateT=0}return}if(state===ST.MENU||state===ST.LEVELS||state===ST.DEAD||state===ST.WIN||state===ST.ENDING){if(k===' '||k==='enter'){e.preventDefault();if(state===ST.LEVELS)stl(0);else hMenu(VW/2,VH/2)}return}if(k==='arrowleft'||k==='a'){if(!I.left)prB('left');e.preventDefault()}else if(k==='arrowright'||k==='d'){if(!I.right)prB('right');e.preventDefault()}else if(k==='arrowup'||k==='w'||k===' '){if(!I.jump)I.jumpPressed=true;I.jump=true;e.preventDefault()}else if(k==='f'){I.lanternPressed=true;e.preventDefault()}else if(k==='escape'){state=ST.PAUSE;stateT=0;I.left=I.right=I.jump=false;I.jumpPressed=I.lanternPressed=false;pts.clear()}}
function onKU(e){const k=e.key.toLowerCase();if(k==='arrowleft'||k==='a')I.left=false;else if(k==='arrowright'||k==='d')I.right=false;else if(k==='arrowup'||k==='w'||k===' ')I.jump=false}
function iStars(){stars=[];for(let i=0;i<70;i++)stars.push({x:Math.random()*VW,y:Math.random()*VH*.75,s:Math.random()})}
ls();rs();bpt();iStars();
cv.addEventListener('pointerdown',onDown,{passive:false});
cv.addEventListener('pointerup',onUp,{passive:false});
cv.addEventListener('pointermove',onMove,{passive:false});
cv.addEventListener('pointercancel',onCancel,{passive:false});
cv.addEventListener('pointerleave',onCancel,{passive:false});
document.querySelectorAll('[data-control]').forEach(el=>{const id=el.dataset.control;el.addEventListener('pointerdown',e=>{e.preventDefault();e.stopPropagation();ia();el.classList.add('pressed');try{el.setPointerCapture(e.pointerId)}catch(_){}pts.set(e.pointerId,id);if(state!==ST.PLAY){if(id==='jump'){if(state===ST.LEVELS){if(save.unlocked>0){SFX.ui();stl(0)}}else hMenu(VW/2,VH/2)}else if(id==='pause'&&state===ST.PAUSE){state=ST.PLAY;stateT=0;SFX.ui()}else if(id==='pause'&&state===ST.LEVELS){state=ST.MENU;stateT=0;SFX.ui()}}else if(id==='pause'&&state===ST.PAUSE){state=ST.PLAY;stateT=0;SFX.ui()}else prB(id)},{passive:false});const release=e=>{e.preventDefault();el.classList.remove('pressed');rlB(id);pts.delete(e.pointerId)};el.addEventListener('pointerup',release,{passive:false});el.addEventListener('pointercancel',release,{passive:false});el.addEventListener('lostpointercapture',()=>{el.classList.remove('pressed');rlB(id)})});
cv.addEventListener('contextmenu',e=>e.preventDefault());
window.addEventListener('keydown',onKD,{passive:false});
window.addEventListener('keyup',onKU,{passive:false});
window.addEventListener('resize',()=>{rs();iStars()});
window.addEventListener('blur',()=>{I.left=I.right=I.jump=false;I.jumpPressed=I.lanternPressed=false;pts.clear();document.querySelectorAll('.hw-btn.pressed').forEach(el=>el.classList.remove('pressed'))});
document.addEventListener('visibilitychange',()=>{if(document.hidden){I.left=I.right=I.jump=false;I.jumpPressed=I.lanternPressed=false;pts.clear();document.querySelectorAll('.hw-btn.pressed').forEach(el=>el.classList.remove('pressed'))}});
window.addEventListener('orientationchange',()=>setTimeout(()=>{rs();iStars()},200));
document.addEventListener('touchstart',e=>{if(e.target===cv)e.preventDefault()},{passive:false});
requestAnimationFrame(loop);
})();
