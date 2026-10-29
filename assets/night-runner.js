/* NIGHT RUNNER — standalone 1-bit arcade racer for the shared 208x320 console display. */
(()=>{'use strict';
const SCREEN=window.CONSOLE_GAME_SCREEN;
const W=SCREEN.width,H=SCREEN.height;
const canvas=document.getElementById('c');
canvas.width=W;canvas.height=H;canvas.style.width='100%';canvas.style.height='100%';canvas.style.objectFit='contain';canvas.style.objectPosition='center';canvas.style.imageRendering='pixelated';
const ctx=canvas.getContext('2d',{alpha:false});ctx.imageSmoothingEnabled=false;
const C='#FFF8E8',B='#000000',MID='#77736b',PIX=window.PixelArt1Bit;
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const input={left:false,right:false,action:false};
const STORE='nightRunnerBest_v1';
let best=0;try{best=Number(localStorage.getItem(STORE)||0)||0}catch(_){}
let mode='title',last=0,elapsed=0,score=0,speed=90,boost=0,spawn=0,shake=0;
let player={x:W/2,y:258,w:15,h:25,steer:0};
let traffic=[],particles=[],pickups=[],hazards=[],roadOffset=0,flash=0,invuln=0,skid=0,combo=0,nearMisses=0,coins=0,level=1;
const lanes=[64,104,144];
function reset(){mode='play';elapsed=0;score=0;speed=90;boost=0;spawn=.5;shake=0;flash=0;traffic=[];particles=[];pickups=[];hazards=[];invuln=0;skid=0;combo=0;nearMisses=0;coins=0;level=1;player.x=W/2;player.steer=0;roadOffset=0}
function rect(x,y,w,h,col){ctx.fillStyle=col;ctx.fillRect(Math.round(x),Math.round(y),Math.round(w),Math.round(h))}
function text(s,x,y,size=8,align='center',col=C){ctx.fillStyle=col;ctx.font='bold '+size+'px monospace';ctx.textAlign=align;ctx.textBaseline='top';ctx.fillText(s,Math.round(x),Math.round(y))}
function car(x,y,w,h,enemy=false){x=Math.round(x);y=Math.round(y);const body=enemy?B:C,trim=enemy?C:B;
 // Wheel silhouettes, chassis, windshield, roof and headlights remain pixel-crisp.
 rect(x-2,y+5,3,6,trim);rect(x+w-1,y+5,3,6,trim);rect(x-2,y+h-10,3,6,trim);rect(x+w-1,y+h-10,3,6,trim);
 rect(x,y+2,w,h-4,trim);rect(x+2,y,w-4,h,body);
 rect(x+3,y+4,w-6,5,trim);rect(x+4,y+5,w-8,3,body);
 rect(x+3,y+11,w-6,6,trim);rect(x+5,y+12,w-10,4,body);
 rect(x+3,y+h-7,w-6,4,trim);rect(x+5,y+h-6,w-10,2,body);
 rect(x+2,y+1,3,2,enemy?C:B);rect(x+w-5,y+1,3,2,enemy?C:B);
 if(!enemy){rect(x+4,y+9,w-8,2,trim);rect(x+4,y+h-3,3,2,trim);rect(x+w-7,y+h-3,3,2,trim)}
}
function drawRoad(dt){
 rect(0,0,W,H,B);
 // Pixel-art city blocks scroll beside the road, using the shared 1-bit structure atlas.
 if(PIX){for(let y=-24+(roadOffset%72),n=0;y<H;y+=72,n++){PIX.building(ctx,1,y,22,n%3===0?'factory':n%3===1?'tower':'house');PIX.building(ctx,W-23,y+30,22,n%3===0?'house':n%3===1?'temple':'tower');}}
 // Crisp three-lane road with shoulders, rumble strips and perspective markers.
 rect(25,0,3,H,MID);rect(28,0,2,H,C);rect(W-30,0,2,H,C);rect(W-28,0,3,H,MID);
 rect(32,0,W-64,H,B);
 for(let y=-22+(roadOffset%22);y<H;y+=22){
   rect(62,y,2,9,C);rect(102,y,2,9,C);rect(142,y,2,9,C);
 }
 for(let y=-16+(roadOffset%32),n=0;y<H;y+=32,n++){
   rect(17,y,5,3,(n%2)?MID:C);rect(W-22,y,5,3,(n%2)?MID:C);
   rect(35,y+10,2,5,MID);rect(W-37,y+10,2,5,MID);
 }
 rect(31,31,W-62,1,MID);
}
function spawnCar(){
 const lane=lanes[Math.floor(Math.random()*lanes.length)];
 const types=Math.random();
 traffic.push({x:lane+(Math.random()*8-4),y:-34,w:types>.72?17:14,h:types>.72?27:23,v:Math.random()*24+15,kind:types>.72?'truck':'car',passed:false});
}
function burst(x,y){for(let i=0;i<10;i++)particles.push({x,y,vx:(Math.random()-.5)*100,vy:(Math.random()-.7)*90,t:.35+Math.random()*.3})}
function spawnPickup(){const lane=lanes[Math.floor(Math.random()*lanes.length)];pickups.push({x:lane,y:-12,type:Math.random()<.72?'nitro':'coin'})}
function spawnHazard(){const lane=lanes[Math.floor(Math.random()*lanes.length)];hazards.push({x:lane,y:-14,w:15,h:8})}
function nearMiss(o){const dx=Math.abs(o.x-player.x),dy=Math.abs((o.y+o.h/2)-(player.y+player.h/2));return dx<(o.w+player.w)*1.15&&dx>(o.w+player.w)*.72&&dy<(o.h+player.h)*.9}
function update(dt){
 if(mode!=='play')return;
 elapsed+=dt;level=1+Math.floor(elapsed/18);speed=clamp(90+elapsed*2.3,90,190);
 if(input.action){boost=clamp(boost+dt*.55,0,1)}else boost=clamp(boost-dt*.8,0,1);
 const actual=speed+boost*75;roadOffset=(roadOffset+actual*dt)%32;
 const steer=(input.right?1:0)-(input.left?1:0);
 player.steer+=(steer-player.steer)*Math.min(1,dt*10);
 const steeringScale=skid>0?.48:1;
 player.x=clamp(player.x+player.steer*(115+actual*.14)*dt*steeringScale,39,W-39);
 spawn-=dt;if(spawn<=0){spawnCar();spawn=Math.max(.34,1.08-(actual-90)*.004-level*.025)+Math.random()*.28}
 if(Math.random()<dt*(.12+level*.012))spawnPickup();
 if(Math.random()<dt*(.055+level*.008))spawnHazard();
 invuln=Math.max(0,invuln-dt);skid=Math.max(0,skid-dt);
 for(const o of traffic){o.y+=(actual*.72+o.v)*dt;
   if(!o.passed&&o.y>player.y+player.h/2){o.passed=true;score+=10}
   if(!o.nearAwarded&&nearMiss(o)){o.nearAwarded=true;nearMisses++;combo=Math.min(9,combo+1);score+=25*combo;burst(player.x,player.y+8)}
 }
 for(let i=traffic.length-1;i>=0;i--){const o=traffic[i];if(o.y>H+40){traffic.splice(i,1);continue}
   if(invuln<=0&&Math.abs(o.x-player.x)<(o.w+player.w)*.72&&Math.abs((o.y+o.h/2)-(player.y+player.h/2))<(o.h+player.h)*.68){
     burst(player.x,player.y);shake=0.24;flash=.2;mode='crash';
     if(score>best){best=score;try{localStorage.setItem(STORE,String(best))}catch(_){}}
   }
 }
 for(let i=pickups.length-1;i>=0;i--){const p=pickups[i];p.y+=(actual*.72)*dt;
   if(Math.abs(p.x-player.x)<13&&Math.abs(p.y-(player.y+12))<17){if(p.type==='nitro'){boost=clamp(boost+.48,0,1);score+=40}else{coins++;score+=75;invuln=.35}burst(p.x,p.y);pickups.splice(i,1);continue}
   if(p.y>H+15)pickups.splice(i,1)
 }
 for(let i=hazards.length-1;i>=0;i--){const h=hazards[i];h.y+=(actual*.72)*dt;
   if(!h.hit&&Math.abs(h.x-player.x)<13&&Math.abs(h.y-(player.y+12))<16){h.hit=true;skid=1.15;speed=Math.max(70,speed-18);score=Math.max(0,score-35);burst(player.x,player.y+12)}
   if(h.y>H+15)hazards.splice(i,1)
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
 text('LV '+level+' x'+Math.max(1,combo),8,27,6,'left',MID);
 const barW=44;rect(8,37,barW,4,MID);rect(8,37,barW*boost,4,C);text('N2O',8,43,5,'left',MID);
 if(skid>0)text('SKID!',W/2,52,7,'center',C);
}
function drawTitle(){
 rect(0,0,W,H,B);
 // Night skyline, distant road and pixel horizon.
 for(let i=0;i<22;i++){const x=(i*37+9)%W,y=(i*29+15)%145;rect(x,y,1+(i%3===0?1:0),1,C)}
 for(let i=0;i<8;i++){const x=i*29-3,h=15+(i*17)%42;rect(x,H-104-h,18,h,MID);rect(x+4,H-100-h,2,3,C);rect(x+11,H-90-h,2,3,C)}
 // Road starts below the instruction block and widens toward the player.
 ctx.fillStyle='#34332f';ctx.beginPath();ctx.moveTo(42,H);ctx.lineTo(87,190);ctx.lineTo(121,190);ctx.lineTo(166,H);ctx.closePath();ctx.fill();
 ctx.fillStyle=MID;ctx.beginPath();ctx.moveTo(48,H);ctx.lineTo(89,194);ctx.lineTo(119,194);ctx.lineTo(160,H);ctx.closePath();ctx.fill();
 rect(42,H-4,124,4,C);for(let i=0;i<5;i++){const y=202+i*24,w=2+i;rect(W/2-w/2,y,w,Math.max(3,10-i),C)}
 car(W/2-8,248,16,25,false);
 text('NIGHT',W/2,37,23);text('RUNNER',W/2,61,23);
 rect(24,96,W-48,1,C);text('1-BIT ARCADE RACING',W/2,106,8);
 text('LEFT / RIGHT TO STEER',W/2,126,7);
 text('HOLD ACTION: NITRO',W/2,138,7);
 text('GRAB N2O + COINS',W/2,150,7);
 text('DODGE OIL + TRAFFIC',W/2,162,7);
 const pulse=Math.floor(performance.now()/420)%2===0;
 if(pulse){rect(29,174,W-58,24,C);text('TAP ACTION TO START',W/2,181,8,'center',B)}
 text('DODGE TRAFFIC',W/2,276,7);text('CHASE THE HIGH SCORE',W/2,289,6,'center',MID);
}
function drawCrash(){
  // Keep the failure state in the same black-and-ivory 1-bit palette.
  rect(12,104,W-24,105,MID);rect(14,106,W-28,101,B);
  rect(20,112,W-40,1,C);rect(20,194,W-40,1,C);
  text('RUN OVER',W/2,119,14);
  text('DISTANCE',W/2,143,6,'center',MID);text(String(Math.floor(score)).padStart(4,'0'),W/2,153,12);
  text('BEST '+String(Math.floor(best)).padStart(4,'0'),W/2,171,7,'center',C);
  text('COINS '+coins+'  NEAR '+nearMisses,W/2,184,5,'center',MID);
  rect(28,220,W-56,22,C);text('ACTION: RETRY',W/2,227,8,'center',B);
  text('TAP SCREEN TO RACE AGAIN',W/2,252,5,'center',MID);
}
function render(dt){
 ctx.save();if(shake>0)ctx.translate((Math.random()-.5)*shake*18,(Math.random()-.5)*shake*18);
 drawRoad(dt);
 for(const h of hazards){rect(h.x-8,h.y,16,8,MID);rect(h.x-5,h.y+2,3,2,C);rect(h.x+2,h.y+4,4,2,C)}
 for(const p of pickups){if(p.type==='nitro'){rect(p.x-5,p.y-5,10,10,C);rect(p.x-2,p.y-3,4,6,B);rect(p.x-1,p.y-1,2,2,C)}else{rect(p.x-4,p.y-4,8,8,C);rect(p.x-2,p.y-2,4,4,B)}}
 for(const o of traffic)car(o.x-o.w/2,o.y,o.w,o.h,true);
 if((mode==='play'||mode==='crash')&&(invuln<=0||Math.floor(performance.now()/70)%2===0))car(player.x-player.w/2,player.y,player.w,player.h,false);
 for(const p of particles){rect(p.x,p.y,2,2,C)}
 drawHUD();ctx.restore();
 if(mode==='crash'){if(flash>0){ctx.save();ctx.globalAlpha=Math.min(.2,flash);rect(0,0,W,H,C);ctx.restore()}drawCrash()}
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
// Gameplay canvas is display-only; all race input comes from console buttons.
window.addEventListener('keydown',e=>{const k=e.key.toLowerCase();if(['arrowleft','a'].includes(k)){input.left=true;e.preventDefault()}if(['arrowright','d'].includes(k)){input.right=true;e.preventDefault()}if([' ','enter','arrowup','w'].includes(k)){input.action=true;if(mode!=='play')reset();e.preventDefault()}});
window.addEventListener('keyup',e=>{const k=e.key.toLowerCase();if(['arrowleft','a'].includes(k))input.left=false;if(['arrowright','d'].includes(k))input.right=false;if([' ','enter','arrowup','w'].includes(k))input.action=false});
window.addEventListener('blur',()=>{input.left=input.right=input.action=false});
requestAnimationFrame(frame);
})();