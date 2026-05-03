#pragma once

static const char WEB_UI_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>FarFarWest Mod</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#1a1a2e;color:#e0e0e0;font-family:'Segoe UI',sans-serif;padding:16px;max-width:600px;margin:0 auto}
h1{text-align:center;color:#00d4ff;margin-bottom:8px;font-size:1.5em}
.subtitle{text-align:center;color:#666;font-size:.85em;margin-bottom:20px}
.card{background:#16213e;border-radius:10px;padding:16px;margin-bottom:12px;border:1px solid #0f3460}
.card-title{font-size:1em;color:#00d4ff;margin-bottom:12px;font-weight:600}
.row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid #0f3460}
.row:last-child{border-bottom:none}
.label{font-size:.95em}
.toggle{position:relative;width:48px;height:26px;cursor:pointer}
.toggle input{opacity:0;width:0;height:0}
.slider{position:absolute;top:0;left:0;right:0;bottom:0;background:#333;border-radius:26px;transition:.3s}
.slider:before{content:'';position:absolute;width:20px;height:20px;left:3px;bottom:3px;background:#888;border-radius:50%;transition:.3s}
.toggle input:checked+.slider{background:#00d4ff}
.toggle input:checked+.slider:before{transform:translateX(22px);background:#fff}
.input-row{display:flex;align-items:center;gap:10px;padding:8px 0}
.input-row label{flex:1;font-size:.95em}
.input-row input[type=number]{width:100px;padding:6px 10px;border:1px solid #0f3460;border-radius:6px;background:#1a1a2e;color:#e0e0e0;font-size:.95em;text-align:center}
.input-row input:focus{outline:none;border-color:#00d4ff}
.btn{padding:6px 14px;border:none;border-radius:6px;cursor:pointer;font-size:.85em;font-weight:600;transition:.2s}
.btn-primary{background:#00d4ff;color:#1a1a2e}
.btn-primary:hover{background:#00b8d4}
.btn-danger{background:#e94560;color:#fff}
.btn-danger:hover{background:#c81e45}
.btn-sm{padding:4px 10px;font-size:.8em}
.player-list{max-height:300px;overflow-y:auto}
.player-item{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid #0f3460}
.player-item:last-child{border-bottom:none}
.player-info{flex:1}
.player-name{font-weight:600;font-size:.95em}
.player-pos{color:#666;font-size:.8em}
.status-bar{display:flex;justify-content:center;gap:20px;margin-bottom:16px;font-size:.85em}
.status-dot{width:8px;height:8px;border-radius:50%;display:inline-block;margin-right:6px}
.dot-green{background:#0f0}
.dot-red{background:#f00}
.dot-yellow{background:#ff0}
.no-data{text-align:center;color:#666;padding:20px;font-size:.9em}
::-webkit-scrollbar{width:6px}
::-webkit-scrollbar-track{background:#1a1a2e}
::-webkit-scrollbar-thumb{background:#0f3460;border-radius:3px}
</style>
</head>
<body>
<h1>FarFarWest Mod</h1>
<div class="subtitle">localhost:1145</div>

<div class="status-bar">
  <span><span class="status-dot dot-green" id="connDot"></span><span id="connStatus">连接中...</span></span>
  <span><span class="status-dot dot-green" id="acDot"></span>反作弊</span>
</div>

<div class="card">
  <div class="card-title">功能开关</div>
  <div class="row">
    <span class="label">自动瞄准</span>
    <label class="toggle"><input type="checkbox" id="aim" onchange="toggle('aim')"><span class="slider"></span></label>
  </div>
  <div class="row">
    <span class="label">无冷却</span>
    <label class="toggle"><input type="checkbox" id="nocd" onchange="toggle('nocd')"><span class="slider"></span></label>
  </div>
  <div class="row">
    <span class="label">无限跳跃</span>
    <label class="toggle"><input type="checkbox" id="jump" onchange="toggle('jump')"><span class="slider"></span></label>
  </div>
  <div class="row">
    <span class="label">无限弹药</span>
    <label class="toggle"><input type="checkbox" id="ammo" onchange="toggle('ammo')"><span class="slider"></span></label>
  </div>
  <div class="row">
    <span class="label">移速修改</span>
    <label class="toggle"><input type="checkbox" id="speed" onchange="toggle('speed')"><span class="slider"></span></label>
  </div>
</div>

<div class="card">
  <div class="card-title">参数调节</div>
  <div class="input-row">
    <label>移速倍率</label>
    <input type="number" id="speedVal" value="1.0" min="0.5" max="10.0" step="0.1">
    <button class="btn btn-primary btn-sm" onclick="setSpeed()">设置</button>
  </div>
  <div class="input-row">
    <label>跳跃高度</label>
    <input type="number" id="jumpHeight" value="0" min="0" max="5000" step="100" placeholder="0=默认">
    <button class="btn btn-primary btn-sm" onclick="setJumpHeight()">设置</button>
  </div>
</div>

<div class="card">
  <div class="card-title">传送到玩家</div>
  <div class="player-list" id="playerList">
    <div class="no-data">扫描中...</div>
  </div>
</div>

<script>
const API='http://127.0.0.1:1145';
let prevStatus=null;

async function toggle(f){
  try{await fetch(API+'/api/toggle/'+f,{method:'POST'})}catch(e){}
  refresh();
}

async function setSpeed(){
  const v=parseFloat(document.getElementById('speedVal').value);
  if(isNaN(v)||v<0.5||v>10)return;
  try{await fetch(API+'/api/speed',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({multiplier:v})})}catch(e){}
}

async function setJumpHeight(){
  const v=parseFloat(document.getElementById('jumpHeight').value);
  if(isNaN(v)||v<0)return;
  try{await fetch(API+'/api/jumpheight',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({height:v})})}catch(e){}
}

async function teleport(idx){
  try{await fetch(API+'/api/teleport',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx})})}catch(e){}
}

function renderPlayers(players){
  const el=document.getElementById('playerList');
  if(!players||players.length===0){
    el.innerHTML='<div class="no-data">未发现其他玩家</div>';
    return;
  }
  let html='';
  players.forEach((p,i)=>{
    html+='<div class="player-item"><div class="player-info">';
    html+='<div class="player-name">玩家 '+(i+1)+'</div>';
    html+='<div class="player-pos">('+p.x.toFixed(0)+', '+p.y.toFixed(0)+', '+p.z.toFixed(0)+')</div>';
    html+='</div><button class="btn btn-primary btn-sm" onclick="teleport('+i+')">传送</button></div>';
  });
  el.innerHTML=html;
}

async function refresh(){
  try{
    const r=await fetch(API+'/api/status');
    const d=await r.json();
    document.getElementById('connDot').className='status-dot dot-green';
    document.getElementById('connStatus').text='已连接';

    const ids=['aim','nocd','jump','ammo','speed'];
    ids.forEach(id=>{
      const el=document.getElementById(id);
      if(el&&d[id]!==undefined)el.checked=d[id];
    });
    if(d.speedMultiplier!==undefined){
      document.getElementById('speedVal').value=d.speedMultiplier;
    }
    if(d.jumpHeight!==undefined&&d.jumpHeight>0){
      document.getElementById('jumpHeight').value=d.jumpHeight;
    }
    renderPlayers(d.players);
    prevStatus=d;
  }catch(e){
    document.getElementById('connDot').className='status-dot dot-red';
    document.getElementById('connStatus').text='连接失败';
  }
}

refresh();
setInterval(refresh,500);
</script>
</body>
</html>)rawliteral";
