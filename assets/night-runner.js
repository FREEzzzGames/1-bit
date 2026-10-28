/* NIGHT RUNNER — standalone 1-bit arcade racer for the shared 208x320 console display. */
(()=>{'use strict';
const SCREEN=window.CONSOLE_GAME_SCREEN;
const W=SCREEN.width,H=SCREEN.height;
const canvas=document.getElementById('c'),ctx=canvas.getContext('2d',{alpha:false});
ctx.imageSmoothingEnabled=false;
const C='#FFF8E8',B='#000000',MID='#77736b';
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const input={left:false,right:false,action:false};
const STORE='nightRunnerBest_v1';
let best=0;try{best=Number(localStorage.getItem(STORE)||0)||0}catch(_){}
let mode='title',last=0,elapsed=0,score=0,speed=90,boost=0,spawn=0,shake=0;
let player={x:W/2,y:258,w:15,h:25,steer:0};
let traffic=[],particles=[],roadOffset=0,flash=0;
const lanes=[64,104,144];
function reset(){mode='play';elapsed=0;score=0;speed=90;boost=0;spawn=.5;shake=0;flash=0;traffic=[];particles=[];player.x=W/2;player.steer=0;roadOffset=0}
function rect(x,y,w,h,col){ctx.fillStyle=col;ctx.fillRect(Math.round(x),Math.round(y),Math.round(w),Math.round(h))}
function text(s,x,y,size=8,align='center',col=C){ctx.fillStyle=col;ctx.font='bold '+size+'px monospace';ctx.textAlign=align;ctx.textBaseline='top';ctx.fillText(s,Math.round(x),Math.round(y))}
function car(x,y,w,h,enemy=false){x=Math.round(x);y=Math.round(y);const body=enemy?B:C,trim=enemy?C:B;
 rect(x-2,y+4,w+4,h-8,trim);rect(x,y,w,h,body);
 rect(x+3,y+4,w-6,5,trim);rect(x+3,y+h-8,w-6,4,trim);
 rect(x+2,y+8,2,5,trim);rect(x+w-4,y+8,2,5,trim);
 rect(x+2,y+2,3,2,enemy?C:B);rect(x+w-5,y+2,3,2,enemy?C:B);
 if(!enemy){rect(x+5,y+11,w-10,5,trim);rect(x+4,y+h-4,3,2,trim);rect(x+w-7,y+h-4,3,2,trim)}
 else{rect(x+4,y+12,w-8,4,trim);rect(x+3,y+h-3,3,2,trim);rect(x+w-6,y+h-3,3,2,trim)}
}
function drawRoad(dt){
 rect(0,0,W,H,B);
 // Road shoulders and a broken 1-bit edge line.
 rect(25,0,3,H,C);rect(W-28,0,3,H,C);
 rect(30,0,W-60,H,MID);rect(33,0,W-66,H,B);
 for(let y=-24+(roadOffset%24);y<H;y+=24){
   rect(43,y,2,11,C);rect(W-45,y,2,11,C);
   rect(83,y+3,2,9,C);rect(123,y+3,2,9,C);
 }
 // Roadside reflectors scroll at the current speed.
 for(let y=-16+(roadOffset%32),n=0;y<H;y+=32,n++){
   rect(17,y,5,3,(n%2)?MID:C);rect(W-22,y,5,3,(n%2)?MID:C);
 }
 // Distant horizon dashes.
 rect(29,30,W-58,1,MID);
}
function spawnCar(){
 const lane=lanes[Math.floor(Math.random()*lanes.length)];
 const types=Math.random();
 traffic.push({x:lane+(Math.random()*8-4),y:-34,w:types>.72?17:14,h:types>.72?27:23,v:Math.random()*24+15,kind:types>.72?'truck':'car',passed:false});
}
function burst(x,y){for(let i=0;i<10;i++)particles.push({x,y,vx:(Math.random()-.5)*100,vy:(Math.random()-.7)*90,t:.35+Math.random()*.3})}
function update(dt){
 if(mode!=='play')return;
 elapsed+=dt;speed=clamp(90+elapsed*2.3,90,190);
 if(input.action){boost=clamp(boost+dt*.55,0,1)}else boost=clamp(boost-dt*.8,0,1);
 const actual=speed+boost*75;roadOffset=(roadOffset+actual*dt)%32;
 const steer=(input.right?1:0)-(input.left?1:0);
 player.steer+=(steer-player.steer)*Math.min(1,dt*10);
 player.x=clamp(player.x+player.steer*(115+actual*.14)*dt,39,W-39);
 spawn-=dt;if(spawn<=0){spawnCar();spawn=Math.max(.36,1.08-(actual-90)*.004)+Math.random()*.28}
 for(const o of traffic){o.y+=(actual*.72+o.v)*dt;if(!o.passed&&o.y>player.y+player.h/2){o.passed=true;score+=10}}
 for(let i=traffic.length-1;i>=0;i--){const o=traffic[i];if(o.y>H+40){traffic.splice(i,1);continue}
   if(Math.abs(o.x-player.x)<(o.w+player.w)*.72&&Math.abs((o.y+o.h/2)-(player.y+player.h/2))<(o.h+player.h)*.68){
     burst(player.x,player.y);shake=0.24;flash=.2;mode='crash';
     if(score>best){best=score;try{localStorage.setItem(STORE,String(best))}catch(_){}}
   }
 }
 for(let i=particles.length-1;i>=0;i--){const p=particles[i];p.t-=dt;p.x+=p.vx*dt;p.y+=p.vy*dt;p.vy+=180*dt;if(p.t<=0)particles.splice(i,1)}
 shake=Math.max(0,shake-dt);flash=Math.max(0,flash-dt);
 score+=dt*(actual/10);
 if(player.x<37||player.x>W-37){score=Math.max(0,score-dt*14);speed=clamp(speed-dt*40,70,190)}
}
function drawHUD(){
 rect(4,4,W-8,19,B);rect(4,4,W-8,1,C);rect(4,22,W-8,1,C);
 text('NIGHT RUNNER',8,8,8,'left');text(String(Math.floor(score)).padStart(4,'0'),W-8,8,8,'right');
 text('BEST '+String(Math.floor(best)).padStart(4,'0'),W-8,27,6,'right',MID);
 const barW=44;rect(8,29,barW,4,MID);rect(8,29,barW*boost,4,C);text('BOOST',8,35,5,'left',MID);
}
function drawTitle(){
 rect(0,0,W,H,B);
 // Night skyline, distant road and pixel horizon.
 for(let i=0;i<22;i++){const x=(i*37+9)%W,y=(i*29+15)%145;rect(x,y,1+(i%3===0?1:0),1,C)}
 for(let i=0;i<8;i++){const x=i*29-3,h=15+(i*17)%42;rect(x,H-104-h,18,h,MID);rect(x+4,H-100-h,2,3,C);rect(x+11,H-90-h,2,3,C)}
 ctx.fillStyle=MID;ctx.beginPath();ctx.moveTo(50,H);ctx.lineTo(85,150);ctx.lineTo(123,150);ctx.lineTo(158,H);ctx.closePath();ctx.fill();
 rect(51,H-4,107,4,C);for(let y=166;y<H;y+=22)rect(W/2-1,y,2,10,C);
 car(W/2-8,222,16,26,false);
 text('NIGHT',W/2,37,23);text('RUNNER',W/2,61,23);
 rect(24,96,W-48,1,C);text('1-BIT ARCADE RACING',W/2,106,8);
 text('LEFT / RIGHT TO STEER',W/2,128,7);
 text('HOLD ACTION: BOOST',W/2,141,7);
 const pulse=Math.floor(performance.now()/420)%2===0;
 if(pulse){rect(29,174,W-58,24,C);text('TAP ACTION TO START',W/2,181,8,'center',B)}
 text('DODGE TRAFFIC',W/2,276,7);text('CHASE THE HIGH SCORE',W/2,289,6,'center',MID);
}
function drawCrash(){
 rect(20,119,W-40,78,C);rect(22,121,W-44,74,B);
 text('CRASH!',W/2,130,15);text('DISTANCE '+Math.floor(score),W/2,151,7);
 text('BEST '+Math.floor(best),W/2,164,7,'center',MID);
 text('ACTION: RETRY',W/2,211,8);
}
function render(dt){
 ctx.save();if(shake>0)ctx.translate((Math.random()-.5)*shake*18,(Math.random()-.5)*shake*18);
 drawRoad(dt);
 for(const o of traffic)car(o.x-o.w/2,o.y,o.w,o.h,true);
 if(mode==='play'||mode==='crash')car(player.x-player.w/2,player.y,player.w,player.h,false);
 for(const p of particles){rect(p.x,p.y,2,2,C)}
 drawHUD();ctx.restore();
 if(mode==='crash'){if(flash>0)rect(0,0,W,H,C);drawCrash()}
}
function frame(now){const dt=Math.min(.034,(now-last)/1000||0);last=now;
 if(mode==='title'){drawTitle()}else{update(dt);render(dt)}
 requestAnimationFrame(frame)}
function bind(id){const el=document.querySelector('[data-control="'+id+'"]');if(!el)return;
 const down=e=>{e.preventDefault();input[id==='left'?'left':id==='right'?'right':'action']=true;if(id==='action'&&mode!=='play'){reset()}};
 const up=e=>{e.preventDefault();input[id==='left'?'left':id==='right'?'right':'action']=false};
 el.addEventListener('pointerdown',down,{passive:false});el.addEventListener('pointerup',up,{passive:false});el.addEventListener('pointercancel',up,{passive:false});el.addEventListener('lostpointercapture',up);
}
bind('left');bind('right');bind('jump');
canvas.addEventListener('pointerdown',e=>{e.preventDefault();if(mode!=='play')reset()},{passive:false});
window.addEventListener('keydown',e=>{const k=e.key.toLowerCase();if(['arrowleft','a'].includes(k)){input.left=true;e.preventDefault()}if(['arrowright','d'].includes(k)){input.right=true;e.preventDefault()}if([' ','enter','arrowup','w'].includes(k)){input.action=true;if(mode!=='play')reset();e.preventDefault()}});
window.addEventListener('keyup',e=>{const k=e.key.toLowerCase();if(['arrowleft','a'].includes(k))input.left=false;if(['arrowright','d'].includes(k))input.right=false;if([' ','enter','arrowup','w'].includes(k))input.action=false});
window.addEventListener('blur',()=>{input.left=input.right=input.action=false});
requestAnimationFrame(frame);
})();