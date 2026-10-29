/* 1-BIT PIXEL ART KIT — deterministic monochrome sprites inspired by the supplied reference sheets. */
(()=>{'use strict';
const C='#FFF8E8',B='#000000',G='#77736B';
function px(ctx,x,y,w,h,c=C){ctx.fillStyle=c;ctx.fillRect(Math.round(x),Math.round(y),Math.round(w),Math.round(h))}
function tile(ctx,x,y,s,type='stone',v=0){
 const u=s/16; px(ctx,x,y,s,s,B);
 if(type==='stone'||type==='wall'){px(ctx,x+u,y+u,s-2*u,s-2*u,C);px(ctx,x+u,y+u*6,s-2*u,u,B);px(ctx,x+u*7,y+u,u,u*5,B);px(ctx,x+u*11,y+u*6,u,u*5,B);px(ctx,x+u*4,y+u*11,u,u*5,B)}
 else if(type==='brick'){for(let r=0;r<4;r++){px(ctx,x+u,y+u*(r*4+1),s-2*u,u*2,C);for(let c=0;c<2;c++)px(ctx,x+u*(1+c*8+(r%2)*3),y+u*(r*4+1),u,u*4,B)}}
 else if(type==='cracked'){px(ctx,x+u,y+u,s-2*u,s-2*u,C);for(let i=0;i<5;i++)px(ctx,x+u*(2+i*2),y+u*(2+i*2),u,u*2,B);px(ctx,x+u*9,y+u*3,u*4,u,B)}
 else if(type==='grass'){px(ctx,x,y+u*5,s,s-u*5,C);for(let i=0;i<8;i++)px(ctx,x+u*i*2,y+u*(i%3),u,u*4,C);px(ctx,x,y+u*7,s,u*2,B)}
 else if(type==='water'){for(let r=0;r<4;r++)for(let c=0;c<3;c++)px(ctx,x+u*(1+c*5+(r%2)*2),y+u*(2+r*4),u*3,u,C)}
 else if(type==='floor'){for(let r=0;r<4;r++)for(let c=0;c<4;c++)px(ctx,x+u*(2+c*4),y+u*(2+r*4),u*2,u*2,(r+c+v)%3?C:G)}
}
function actor(ctx,x,y,s=16,type='hero',frame=0,variant=0){
 const u=s/16; const p=(a,b,w,h,c=C)=>px(ctx,x+a*u,y+b*u,w*u,h*u,c);
 if(type==='hero'||type==='enemy'||type==='creature'){
 p(5,0,6,2);p(3,2,10,6);p(4,8,8,3);p(2,11,4,4);p(10,11,4,4);
 p(5,4,2,2,B);p(10,4,2,2,B);if(type==='enemy'){p(6,7,4,1,B);p(4,1,2,2,G)}else{p(6,7,4,1,C)}
 if(frame%2){p(1,9,2,3);p(13,9,2,3)}
 if(type==='hero'){
  const v=((variant|0)%10+10)%10;
  if(v===1){p(3,1,10,2,G);p(2,3,2,5);p(12,3,2,5)}
  else if(v===2){p(4,0,8,2,B);p(3,2,10,2,G);p(5,3,1,3,C);p(10,3,1,3,C)}
  else if(v===3){p(2,5,3,6,G);p(11,5,3,6,G);p(4,9,8,3,B)}
  else if(v===4){p(3,0,10,2,G);p(2,2,12,2);p(6,3,4,3,B);p(5,11,6,4,G)}
  else if(v===5){p(5,0,6,2,G);p(4,2,8,2,B);p(4,5,8,2,G);p(6,7,4,2,B);p(2,11,3,4,G);p(11,11,3,4,G)}
  else if(v===6){p(4,0,8,2,B);p(3,2,10,2,G);p(2,8,3,3,G);p(11,8,3,3,G);p(5,12,6,3,B)}
  else if(v===7){p(3,1,10,3,G);p(2,4,3,5,B);p(11,4,3,5,B);p(4,9,8,2,G)}
  else if(v===8){p(5,0,6,2,B);p(3,2,10,4,G);p(4,6,8,2,B);p(1,9,3,4,G);p(12,9,3,4,G)}
  else if(v===9){p(5,0,6,2,G);p(3,2,10,2);p(2,4,3,5,G);p(11,4,3,5,G);p(5,8,6,3,B);p(4,11,8,4,G)}
 }
 }else if(type==='ship'){p(7,0,2,4);p(5,4,6,5);p(1,8,14,4);p(4,12,3,3);p(9,12,3,3);p(7,5,2,2,B)}
 else if(type==='car'){p(4,0,8,3);p(2,3,12,10);p(4,5,8,3,B);p(0,5,3,4);p(13,5,3,4);p(3,13,3,3);p(10,13,3,3)}
 else if(type==='tree'){
 // Layered conifer silhouette with a narrow trunk; avoids cross-shaped canopy.
 p(7,0,2,2);p(6,2,4,2);p(5,4,6,2);p(4,6,8,2);p(3,8,10,2);p(2,10,12,2);p(7,12,2,4);
 // Black cut-outs create distinct stepped branches in the white silhouette.
 p(7,3,2,1,B);p(6,5,2,1,B);p(9,5,2,1,B);p(5,7,2,1,B);p(9,7,2,1,B);p(4,9,2,1,B);p(10,9,2,1,B);
 }
}
function building(ctx,x,y,s=32,type='house',v=0){
 const u=s/16,p=(a,b,w,h,c=C)=>px(ctx,x+a*u,y+b*u,w*u,h*u,c);
 if(type==='tower'||type==='castle'){p(3,4,10,12);p(1,2,4,4);p(6,0,4,4);p(11,2,4,4);p(6,10,4,6,B);p(4,6,2,2,B);p(10,6,2,2,B)}
 else if(type==='hospital'){p(2,2,12,14);p(6,5,4,7,B);p(4,7,8,3,B);p(6,12,4,4,B)}
 else if(type==='temple'){p(0,3,16,2);p(2,1,12,2);for(let i=0;i<3;i++)p(3+i*4,5,2,8);p(0,13,16,2)}
 else if(type==='factory'){p(1,6,14,9);p(1,3,4,4);p(6,1,3,6);p(10,4,5,3);for(let i=0;i<4;i++)p(3+i*3,9,1,3,B)}
 else {p(3,6,10,9);p(1,6,14,2);p(2,4,12,2);p(6,10,4,5,B);p(5,7,2,2,B);p(10,7,2,2,B)}
}
function toggle(ctx,x,y,w,h,on=true){px(ctx,x,y,w,h,C);px(ctx,x+1,y+1,w-2,h-2,B);const k=Math.max(3,h-4);px(ctx,on?x+w-k-2:x+2,y+2,k,k,C);px(ctx,x+3,y+h-3,w-6,1,G)}
window.PixelArt1Bit=Object.freeze({tile,actor,building,toggle,colors:{white:C,black:B,gray:G}});
})();