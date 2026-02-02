#ifndef PROVISIONING_HTML_H
#define PROVISIONING_HTML_H

// HTML page for WiFi provisioning web interface
static const char html_page[] = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi</title>
<style>
body{font-family:Arial;padding:20px;max-width:400px;margin:0 auto}
h1{text-align:center}
input,select{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:4px;font-size:16px;box-sizing:border-box}
.btn{width:100%;padding:15px;background:#2196F3;color:#fff;border:none;border-radius:4px;font-size:16px;margin:5px 0;cursor:pointer}
#s{margin:10px 0;padding:10px;border-radius:4px;text-align:center;display:none}
.hidden{display:none}
</style>
</head>
<body>
<h1>WiFi Setup</h1>
<select id="n" onchange="toggleManual()"></select>
<button class="btn" onclick="scan()">Scan</button>
<input type="text" id="m" placeholder="Enter hidden SSID" autocapitalize="off" class="hidden">
<input type="text" id="p" placeholder="Password" autocapitalize="off">
<button class="btn" onclick="connect()">Connect</button>
<div id="s"></div>
<script>
function toggleManual(){
let sel=document.getElementById('n');
let manual=document.getElementById('m');
manual.className=sel.value==='__HIDDEN__'?'':'hidden';
}
function scan(){
document.getElementById('s').style.display='block';
document.getElementById('s').textContent='Scanning...';
fetch('/scan').then(r=>r.json()).then(d=>{
let o='<option value="">Select</option><option value="__HIDDEN__">=== Hidden SSID ===</option>';
d.forEach(n=>o+='<option value="'+n.ssid+'">'+n.ssid+'</option>');
document.getElementById('n').innerHTML=o;
document.getElementById('s').textContent='Found '+d.length;
}).catch(()=>document.getElementById('s').textContent='Scan failed');
}
function connect(){
let sel=document.getElementById('n').value;
let ssid=sel==='__HIDDEN__'?document.getElementById('m').value:sel;
let pass=document.getElementById('p').value;
if(!ssid){document.getElementById('s').textContent='Select or enter SSID';return;}
document.getElementById('s').textContent='Saving...';
fetch('/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,password:pass})})
.then(r=>r.json()).then(d=>document.getElementById('s').textContent=d.success?'Saved! Rebooting...':'Failed')
.catch(()=>document.getElementById('s').textContent='Error');
}
window.onload=scan;
</script>
</body>
</html>)rawliteral";

#endif // WEB_PROVISIONING_H
