#pragma once

static const char WEB_UI_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>无限火力</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#1a1a2e;color:#e0e0e0;font-family:'Segoe UI',sans-serif;padding:16px;max-width:600px;margin:0 auto}
h1{text-align:center;color:#00d4ff;margin-bottom:4px;font-size:1.5em}
.author{text-align:center;color:#e94560;font-size:.8em;margin-bottom:6px}
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
.input-group{padding:10px 0;border-bottom:1px solid #0f3460}
.input-group:last-child{border-bottom:none}
.input-label{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
.input-label .label{font-size:.95em}
.real-val{color:#00d4ff;font-size:.85em}
.input-row{display:flex;align-items:center;gap:10px}
.input-row input[type=number]{flex:1;padding:8px 10px;border:1px solid #0f3460;border-radius:6px;background:#1a1a2e;color:#e0e0e0;font-size:.95em;text-align:center}
.input-row input:focus{outline:none;border-color:#00d4ff}
.btn{padding:8px 16px;border:none;border-radius:6px;cursor:pointer;font-size:.85em;font-weight:600;transition:.2s}
.btn-primary{background:#00d4ff;color:#1a1a2e}
.btn-primary:hover{background:#00b8d4}
.btn-sm{padding:6px 12px;font-size:.8em}
.hint{color:#666;font-size:.8em;margin-top:4px}
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
.no-data{text-align:center;color:#666;padding:20px;font-size:.9em}
.footer{text-align:center;color:#444;font-size:.75em;margin-top:20px;padding-top:12px;border-top:1px solid #0f3460}
::-webkit-scrollbar{width:6px}
::-webkit-scrollbar-track{background:#1a1a2e}
::-webkit-scrollbar-thumb{background:#0f3460;border-radius:3px}
</style>
</head>
<body>
<h1>无限火力</h1>
<div class="author">by songzhearen</div>
<div class="subtitle">localhost:1145</div>

<div class="status-bar">
  <span><span class="status-dot" id="connDot"></span><span id="connStatus">连接中...</span></span>
</div>

<div class="card">
  <div class="card-title">功能开关</div>
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
  <div class="input-group">
    <div class="input-label">
      <span class="label">移动速度</span>
      <span class="real-val" id="realSpeed">读取中...</span>
    </div>
    <div class="input-row">
      <input type="number" id="speedVal" value="0" min="0" max="5000" step="50" placeholder="0=不修改">
      <button class="btn btn-primary btn-sm" onclick="setSpeed()">设置</button>
    </div>
    <div class="hint">默认约600，设为0恢复原速</div>
  </div>
</div>

<div class="card">
  <div class="card-title">传送到玩家</div>
  <div class="player-list" id="playerList">
    <div class="no-data">扫描中...</div>
  </div>
</div>

<div class="card">
  <div class="card-title">传送点</div>
  <div class="player-list" id="pointList">
    <div class="no-data">加载中...</div>
  </div>
</div>

<div class="footer">无限火力 Mod by songzhearen</div>

<script>
const API='';
let connected=false;
let prevPlayerHash='';

function toggle(f){fetch(API+'/api/toggle/'+f,{method:'POST'}).catch(()=>{})}

function setSpeed(){
  const v=parseFloat(document.getElementById('speedVal').value);
  if(isNaN(v)||v<0)return;
  fetch(API+'/api/speed',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({speed:v})}).catch(()=>{});
}

function teleport(idx){
  fetch(API+'/api/teleport',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx})}).catch(()=>{});
}

function savePoint(idx){
  fetch(API+'/api/savepoint',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx})}).catch(()=>{});
}

function gotoPoint(idx){
  fetch(API+'/api/gotopoint',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx})}).catch(()=>{});
}

function delPoint(idx){
  fetch(API+'/api/delpoint',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx})}).catch(()=>{});
}

function renderPoints(points){
  if(!points)return;
  let html='';
  for(let i=0;i<points.length;i++){
    const p=points[i];
    html+='<div class="player-item"><div class="player-info">';
    if(p.active){
      html+='<div class="player-name">传送点 '+(i+1)+'</div>';
      html+='<div class="player-pos">('+p.x.toFixed(0)+', '+p.y.toFixed(0)+', '+p.z.toFixed(0)+')</div>';
    }else{
      html+='<div class="player-name">传送点 '+(i+1)+'</div>';
      html+='<div class="player-pos">未设置</div>';
    }
    html+='</div><div style="display:flex;gap:6px">';
    if(!p.active){
      html+='<button class="btn btn-primary btn-sm" onclick="savePoint('+i+')">保存</button>';
    }else{
      html+='<button class="btn btn-primary btn-sm" onclick="gotoPoint('+i+')">传送</button>';
      html+='<button class="btn btn-primary btn-sm" onclick="savePoint('+i+')">更新</button>';
      html+='<button class="btn btn-primary btn-sm" onclick="delPoint('+i+')">删除</button>';
    }
    html+='</div></div>';
  }
  document.getElementById('pointList').innerHTML=html;
}

function renderPlayers(players){
  if(!players||players.length===0){
    if(prevPlayerHash!=='empty'){
      document.getElementById('playerList').innerHTML='<div class="no-data">未发现其他玩家</div>';
      prevPlayerHash='empty';
    }
    return;
  }
  let hash=players.map(p=>p.x.toFixed(0)+','+p.y.toFixed(0)).join('|');
  if(hash===prevPlayerHash)return;
  prevPlayerHash=hash;

  let html='';
  players.forEach((p,i)=>{
    html+='<div class="player-item"><div class="player-info">';
    html+='<div class="player-name">玩家 '+(i+1)+'</div>';
    html+='<div class="player-pos">('+p.x.toFixed(0)+', '+p.y.toFixed(0)+', '+p.z.toFixed(0)+')</div>';
    html+='</div><button class="btn btn-primary btn-sm" onclick="teleport('+i+')">传送</button></div>';
  });
  document.getElementById('playerList').innerHTML=html;
}

async function refresh(){
  try{
    const r=await fetch(API+'/api/status');
    if(!r.ok)return;
    const d=await r.json();

    if(!connected){
      connected=true;
      document.getElementById('connDot').className='status-dot dot-green';
      document.getElementById('connStatus').textContent='已连接';
    }

    ['nocd','jump','ammo','speed'].forEach(id=>{
      const el=document.getElementById(id);
      if(el&&d[id]!==undefined&&el!==document.activeElement)el.checked=d[id];
    });

    const speedInput=document.getElementById('speedVal');
    if(speedInput!==document.activeElement&&d.targetSpeed!==undefined&&d.targetSpeed>0)
      speedInput.value=d.targetSpeed;

    if(d.realSpeed!==undefined)
      document.getElementById('realSpeed').textContent='当前: '+d.realSpeed.toFixed(1);

    renderPlayers(d.players);
    renderPoints(d.points);
  }catch(e){
    connected=false;
    document.getElementById('connDot').className='status-dot dot-red';
    document.getElementById('connStatus').textContent='连接失败';
  }
}

refresh();
setInterval(refresh,1000);
</script>
</body>
</html>)rawliteral";
