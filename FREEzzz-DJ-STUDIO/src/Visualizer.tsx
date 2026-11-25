import {useEffect,useRef} from 'react'
import * as THREE from 'three'
type Props={analyser:AnalyserNode|null;mode:string;intensity:number}
export default function Visualizer({analyser,mode,intensity}:Props){
 const host=useRef<HTMLDivElement>(null), latest=useRef({analyser,mode,intensity})
 latest.current={analyser,mode,intensity}
 useEffect(()=>{
  if(!host.current)return
  const scene=new THREE.Scene();scene.background=new THREE.Color('#070a10')
  const camera=new THREE.PerspectiveCamera(55,1,.1,100);camera.position.z=7
  const renderer=new THREE.WebGLRenderer({antialias:true});renderer.setPixelRatio(Math.min(devicePixelRatio,2));host.current.appendChild(renderer.domElement)
  const n=1800,pos=new Float32Array(n*3),phase=new Float32Array(n)
  for(let i=0;i<n;i++){const a=Math.random()*Math.PI*2,r=.5+Math.random()*3.1;pos[3*i]=Math.cos(a)*r;pos[3*i+1]=Math.sin(a)*r;pos[3*i+2]=(Math.random()-.5)*2;phase[i]=Math.random()*Math.PI*2}
  const geo=new THREE.BufferGeometry();geo.setAttribute('position',new THREE.BufferAttribute(pos,3))
  const mat=new THREE.PointsMaterial({color:'#65f5e8',size:.025,transparent:true,opacity:.9})
  const cloud=new THREE.Points(geo,mat);scene.add(cloud)
  const ring=new THREE.Mesh(new THREE.TorusKnotGeometry(1.35,.012,220,8,2,5),new THREE.MeshBasicMaterial({color:'#ffb703',wireframe:true,transparent:true,opacity:.65}));scene.add(ring)
  const resize=()=>{if(!host.current)return;const w=host.current.clientWidth,h=host.current.clientHeight;camera.aspect=w/Math.max(h,1);camera.updateProjectionMatrix();renderer.setSize(w,h)}
  resize();const obs=new ResizeObserver(resize);obs.observe(host.current)
  const bins=new Uint8Array(1024);let frame=0,raf=0
  const animate=()=>{raf=requestAnimationFrame(animate);frame++;const {analyser:a,mode:m,intensity:p}=latest.current;let bass=.12,avg=.1
   if(a){a.getByteFrequencyData(bins);let s=0;for(let i=0;i<160;i++)s+=bins[i]??0;avg=s/160/255;bass=(bins.slice(0,18).reduce((x,y)=>x+y,0)/18)/255}
   ring.scale.setScalar(1+bass*p*1.8);ring.rotation.x+=.002;ring.rotation.y+=.003+avg*.01;cloud.rotation.z+=.0008+avg*.004
   const aPos=geo.attributes.position.array as Float32Array
   for(let i=0;i<n;i++){const q=phase[i],r=.5+(i%37)/37*2.8,w=Math.sin(frame*.012+q)*(.04+avg*p*.4);const ang=q+frame*.0007
    if(m==='WAVE')aPos[3*i+1]=Math.sin(aPos[3*i]*2+frame*.025+q)*(.45+bass*p)
    else{aPos[3*i]=Math.cos(ang)*(r+w);aPos[3*i+1]=Math.sin(ang)*(r+w)}
   }geo.attributes.position.needsUpdate=true;renderer.render(scene,camera)}
  animate()
  return()=>{cancelAnimationFrame(raf);obs.disconnect();geo.dispose();mat.dispose();ring.geometry.dispose();(ring.material as THREE.Material).dispose();renderer.dispose();renderer.domElement.remove()}
 },[])
 return <div className="visual-canvas" ref={host}/>
}