#ifndef PROVISIONING_HTML_H
#define PROVISIONING_HTML_H

// HTML page for WiFi provisioning web interface
static const char html_page[] = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi Setup</title>
<style>
body{font-family:Arial;padding:20px;max-width:400px;margin:0 auto}
h1{text-align:center}
h2{font-size:18px;margin-top:20px;padding-top:15px;border-top:1px solid #ddd}
input,select{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:4px;font-size:16px;box-sizing:border-box}
.btn{width:100%;padding:15px;background:#2196F3;color:#fff;border:none;border-radius:4px;font-size:16px;margin:5px 0;cursor:pointer}
#s{margin:10px 0;padding:10px;border-radius:4px;text-align:center;display:none}
.hidden{display:none}
label{display:block;margin:10px 0 5px;font-weight:bold}
.checkbox-label{font-weight:normal;display:inline-block;margin-left:5px}
</style>
</head>
<body>
<h1>WiFi Setup</h1>
<select id="n" onchange="toggleManual()"></select>
<button class="btn" onclick="scan()">Scan</button>
<input type="text" id="m" placeholder="Enter hidden SSID" autocapitalize="off" class="hidden">
<input type="text" id="p" placeholder="Password" autocapitalize="off">
<h2>OTA Updates</h2>
<label><input type="checkbox" id="ota_enabled" checked><span class="checkbox-label">Enable Auto-Update</span></label>
<label>Release Channel:</label>
<select id="ota_channel">
<option value="0">Stable</option>
<option value="1">Develop (includes pre-releases)</option>
</select>
<label>Check Interval (hours):</label>
<input type="number" id="ota_interval" value="24" min="1" max="168">
<button class="btn" onclick="connect()">Save & Connect</button>
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
let ota_enabled=document.getElementById('ota_enabled').checked;
let ota_channel=parseInt(document.getElementById('ota_channel').value);
let ota_interval=parseInt(document.getElementById('ota_interval').value);
document.getElementById('s').textContent='Saving...';
fetch('/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,password:pass,ota_enabled:ota_enabled,ota_channel:ota_channel,ota_interval:ota_interval})})
.then(r=>r.json()).then(d=>document.getElementById('s').textContent=d.success?'Saved! Rebooting...':'Failed')
.catch(()=>document.getElementById('s').textContent='Error');
}
window.onload=scan;
</script>
</body>
</html>)rawliteral";

#endif // PROVISIONING_HTML_H
