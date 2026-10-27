// Test page served at http://192.168.4.1/: send modes by hand, or hammer the
// WebSocket and HTTP server from the phone while a BLE client runs alongside
#pragma once

const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Coex harness</title>
<style>
body{font:15px system-ui,sans-serif;margin:16px;background:#111;color:#eee}
h1{font-size:18px;margin:0 0 12px}
button{font:inherit;padding:10px 0;border:0;border-radius:8px;color:#000;font-weight:600}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:12px}
.wide{grid-column:span 2;background:#ddd}
fieldset{border:1px solid #444;border-radius:8px;margin:0 0 12px;padding:8px 12px}
label{display:block;margin:6px 0}
input[type=number]{width:70px}
table{border-collapse:collapse;width:100%}
td{padding:3px 0;border-bottom:1px solid #222}td:last-child{text-align:right;font-variant-numeric:tabular-nums}
#ws{font-weight:600}
</style></head><body>
<h1>Coex harness <span id="ws">connecting</span></h1>
<div class="grid" id="modes"></div>
<fieldset><legend>Hammer</legend>
<label><input type="checkbox" id="hammer"> send a random mode every <input type="number" id="interval" value="300" min="50"> ms</label>
<label><input type="checkbox" id="blob"> download /blob (64 KB) in a loop</label>
</fieldset>
<table>
<tr><td>Mode now</td><td id="mode">-</td></tr>
<tr><td>Sent / state msgs</td><td id="counts">0 / 0</td></tr>
<tr><td>Last command to state</td><td id="rtt">-</td></tr>
<tr><td>WS reconnects</td><td id="reconnects">0</td></tr>
<tr><td>Blobs ok / failed</td><td id="blobs">0 / 0</td></tr>
</table>
<h1 style="margin-top:16px">Device</h1>
<table id="stats"></table>
<script>
const colours={1:'#0f0',2:'#fb0',3:'#33f',4:'#f33',5:'#fff',6:'#0ff',7:'#f0f',8:'#f80'};
const modes=[1,2,3,4,5,6,7,8,11,12,13,14,15,16,17,18,81,82,83,84,85,86,87,88];
const $=id=>document.getElementById(id);
let sock,sent=0,states=0,reconnects=-1,sentAt=0,blobOk=0,blobFail=0;
for(const m of modes){const b=document.createElement('button');b.textContent=m;b.style.background=colours[m%10];b.onclick=()=>send(m);$('modes').append(b)}
for(const [m,t] of [[98,'Strobe'],[-1,'Auto']]){const b=document.createElement('button');b.textContent=t;b.className='wide';b.onclick=()=>send(m);$('modes').append(b)}
function connect(){
  reconnects++;$('reconnects').textContent=reconnects;
  sock=new WebSocket('ws://'+location.host+'/ws');
  sock.onopen=()=>{$('ws').textContent='live';$('ws').style.color='#0f0'};
  sock.onclose=()=>{$('ws').textContent='dropped';$('ws').style.color='#f33';setTimeout(connect,1000)};
  sock.onmessage=e=>{const s=JSON.parse(e.data);states++;
    $('mode').textContent=s.mode+(s.auto?' (auto)':'');
    if(sentAt){$('rtt').textContent=Math.round(performance.now()-sentAt)+' ms';sentAt=0}
    $('counts').textContent=sent+' / '+states};
}
function send(m){if(!sock||sock.readyState!==1)return;sock.send(JSON.stringify({mode:m}));sent++;sentAt=performance.now();$('counts').textContent=sent+' / '+states}
function hammer(){if($('hammer').checked)send(Math.random()<0.2?98:modes[Math.floor(Math.random()*modes.length)]);setTimeout(hammer,Math.max(50,+$('interval').value||300))}
async function blobLoop(){
  while(true){
    if($('blob').checked){try{const r=await fetch('/blob',{cache:'no-store'});const b=await r.arrayBuffer();b.byteLength===65536?blobOk++:blobFail++}catch(e){blobFail++}
      $('blobs').textContent=blobOk+' / '+blobFail}
    else await new Promise(r=>setTimeout(r,500));
  }
}
async function stats(){
  try{const s=await (await fetch('/stats',{cache:'no-store'})).json();
    $('stats').innerHTML=Object.entries(s).map(([k,v])=>'<tr><td>'+k+'</td><td>'+(v===null?'off':v)+'</td></tr>').join('')}catch(e){}
  setTimeout(stats,2000);
}
connect();hammer();blobLoop();stats();
</script></body></html>
)HTML";
