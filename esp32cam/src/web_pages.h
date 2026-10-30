/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The pages served by the module (main.cpp): the remote control of the badge (/), the settings of the Wi-Fi
 * (/config, on the page), the screens of the badges of the network (/admin). Self-contained: no Internet needed.
 *
 * The screen: GET /screen?since=<id> answers 204 when it did not change, otherwise [kind][id lo][id hi] then the
 * planes (camlink.h CAMLINK_SCREEN_*: 1 plane of 5000 bytes, 2 for 4 grays, none for all white or black); 200 x 200,
 * 1 bit per pixel, bit 7 = the leftmost one. On a network, ?pin= (the code shown by the badge) or ?key= (the admin
 * key, to look only). */

#pragma once

static const char PAGE_REMOTE[] = R"HTML(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Cigale</title>
<style>
:root{--bg:#f4f5f3;--panel:#fff;--fg:#1d2120;--muted:#646b68;--line:#d4d8d5;--accent:#0b6e63;--ink:#fff}
@media (prefers-color-scheme:dark){:root{--bg:#121615;--panel:#1c211f;--fg:#e8ebe9;--muted:#9aa29e;--line:#323a37;--accent:#3fc1b0;--ink:#0f1413}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif;
 padding:12px 16px 32px;display:flex;flex-direction:column;align-items:center;gap:12px;-webkit-user-select:none;user-select:none}
h1{font-size:1.1rem;margin:0;font-weight:600}
#st{font-size:.85rem;color:var(--muted);min-height:1.2em}
canvas{width:min(92vw,400px);aspect-ratio:1;image-rendering:pixelated;border:1px solid var(--line);border-radius:6px;background:#fff;touch-action:none}
.keys{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;width:min(92vw,400px)}
.keys button{font:600 1.3rem system-ui,sans-serif;padding:18px 0;border-radius:10px;border:1px solid var(--line);
 background:var(--panel);color:var(--fg);touch-action:manipulation}
.keys button.down{background:var(--accent);color:var(--ink);border-color:var(--accent)}
.hint{font-size:.8rem;color:var(--muted);text-align:center;margin:0}
.type{display:flex;gap:8px;width:min(92vw,400px)}
.type input{flex:1;min-width:0;font:1rem system-ui,sans-serif;padding:10px;border-radius:8px;border:1px solid var(--line);background:var(--panel);color:var(--fg)}
.type button,form button{font:500 .95rem system-ui,sans-serif;padding:10px 12px;border-radius:8px;border:1px solid var(--line);background:var(--panel);color:var(--fg)}
details{width:min(92vw,400px);background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:10px 12px}
summary{cursor:pointer;font-weight:500}
form{display:grid;gap:8px;margin-top:10px}
form label{display:grid;gap:3px;font-size:.85rem;color:var(--muted)}
form input{font:1rem system-ui,sans-serif;padding:8px;border-radius:6px;border:1px solid var(--line);background:var(--bg);color:var(--fg)}
a{color:var(--accent)}
</style></head><body>
<h1 id="name">Cigale</h1>
<canvas id="scr" width="200" height="200"></canvas>
<div id="st">Connexion…</div>
<div class="keys">
 <button data-k="y">Y</button><button data-k="a">A</button><button data-k="b">B</button><button data-k="x">X</button>
</div>
<p class="hint">Comme sur le badge : Y A B X de gauche à droite. Appui long : maintenir le bouton.</p>
<div class="type">
 <input id="txt" placeholder="Texte pour l'éditeur du badge" autocomplete="off" autocapitalize="off">
 <button id="send">Taper</button><button id="bs" title="Effacer">⌫</button><button id="ok" title="Valider">⏎</button>
</div>
<details><summary>Wi-Fi du module</summary>
<p class="hint" style="text-align:left">Par défaut le module crée son propre point d'accès. Pour qu'il rejoigne un réseau,
 enregistrer son nom ici, puis sur le badge : Télécommande &gt; Wi-Fi : réseau. La clé admin permet de voir l'écran
 des badges du réseau sur la page <a href="/admin">/admin</a> (la même clé sur tous).</p>
<form id="cfg">
 <label>Réseau (SSID)<input name="ssid" maxlength="32"></label>
 <label>Mot de passe du réseau<input name="pass" type="password" maxlength="63" placeholder="inchangé si vide"></label>
 <label>Clé admin (facultative)<input name="admin" type="password" maxlength="32" placeholder="inchangée si vide"></label>
 <button>Enregistrer</button><span id="cfgst" class="hint"></span>
</form></details>
<script>
const $=id=>document.getElementById(id);
let pin=localStorage.getItem('cigale-pin')||'',id=-1,busy=false,lastOk=0;
const q=()=>pin?'&pin='+encodeURIComponent(pin):'';
const cv=$('scr'),g=cv.getContext('2d'),img=g.createImageData(200,200);
const GRAY=[0,85,170,255];
function draw(buf){
 const d=new Uint8Array(buf),kind=d[0];id=d[1]|d[2]<<8;
 for(let p=0;p<40000;p++){
  let v;const byte=(p>>3),bit=7-(p&7);
  if(kind==2)v=255;else if(kind==3)v=0;
  else if(kind==1){const l=(d[3+byte]>>bit)&1,m=(d[5003+byte]>>bit)&1;v=GRAY[m*2+l]}
  else v=((d[3+byte]>>bit)&1)?255:0;
  img.data[4*p]=img.data[4*p+1]=img.data[4*p+2]=v;img.data[4*p+3]=255}
 g.putImageData(img,0,0)}
async function poll(){
 if(busy)return;busy=true;
 try{const r=await fetch('/screen?since='+id+q(),{cache:'no-store'});
  if(r.status==401){askPin();}
  else if(r.status==200){draw(await r.arrayBuffer());$('st').textContent='Connecté'}
  else if(r.status==204){$('st').textContent='Connecté'}
  lastOk=Date.now()}
 catch(e){$('st').textContent='Module injoignable…'}
 busy=false}
function askPin(){const p=prompt('Code affiché par le badge (Télécommande > Se connecter) :');if(p!==null){pin=p.trim();localStorage.setItem('cigale-pin',pin)}}
function key(k){fetch('/key?k='+encodeURIComponent(k)+q()).then(r=>{if(r.status==401)askPin()})}
document.querySelectorAll('.keys button').forEach(b=>{
 let t0=0;
 const down=e=>{e.preventDefault();t0=Date.now();b.classList.add('down')};
 const up=e=>{e.preventDefault();if(!t0)return;b.classList.remove('down');
  key(Date.now()-t0>=800?b.dataset.k.toUpperCase():b.dataset.k);t0=0};
 b.addEventListener('pointerdown',down);b.addEventListener('pointerup',up);
 b.addEventListener('pointerleave',()=>{t0=0;b.classList.remove('down')})});
function type(t){return fetch('/type?t='+encodeURIComponent(t)+q())}
$('send').onclick=()=>{const t=$('txt').value;if(t)type(t).then(()=>$('txt').value='')};
$('txt').addEventListener('keydown',e=>{if(e.key=='Enter'){e.preventDefault();$('send').onclick()}});
$('bs').onclick=()=>type('\b');$('ok').onclick=()=>type('\r');
document.addEventListener('keydown',e=>{if(e.target.tagName=='INPUT')return;
 const k={ArrowLeft:'y',ArrowUp:'a',ArrowDown:'b',ArrowRight:'x'}[e.key];if(k)key(k)});
fetch('/info'+(pin?'?pin='+encodeURIComponent(pin):'')).then(r=>r.json()).then(i=>{$('name').textContent=i.name;document.title=i.name;
 $('cfg').ssid.value=i.ssid||''}).catch(()=>{});
$('cfg').onsubmit=e=>{e.preventDefault();const f=new URLSearchParams(new FormData($('cfg')));
 fetch('/config?'+f+q(),{method:'POST'}).then(r=>r.text()).then(t=>$('cfgst').textContent=t)};
setInterval(poll,200);poll();
</script></body></html>
)HTML";

static const char PAGE_ADMIN[] = R"HTML(<!doctype html>
<html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Cigales du réseau</title>
<style>
:root{--bg:#f4f5f3;--panel:#fff;--fg:#1d2120;--muted:#646b68;--line:#d4d8d5;--accent:#0b6e63}
@media (prefers-color-scheme:dark){:root{--bg:#121615;--panel:#1c211f;--fg:#e8ebe9;--muted:#9aa29e;--line:#323a37;--accent:#3fc1b0}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif;padding:16px}
header{display:flex;flex-wrap:wrap;gap:8px 16px;align-items:baseline;margin-bottom:14px}
h1{font-size:1.2rem;margin:0}
#st{color:var(--muted);font-size:.85rem}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(210px,1fr));gap:12px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:8px;display:grid;gap:6px}
.card canvas{width:100%;aspect-ratio:1;image-rendering:pixelated;border-radius:4px;background:#fff}
.card .n{display:flex;justify-content:space-between;font-size:.85rem}
.card .n a{color:var(--accent)}
.card.old{opacity:.45}
</style></head><body>
<header><h1>Cigales du réseau</h1><span id="st"></span></header>
<div class="grid" id="grid"></div>
<script>
let key=localStorage.getItem('cigale-admin')||'';
const cards={};const GRAY=[0,85,170,255];
function ask(){const k=prompt('Clé admin (la même que dans les réglages Wi-Fi des modules) :');if(k!==null){key=k.trim();localStorage.setItem('cigale-admin',key)}}
if(!key)ask();
function card(p){
 let c=cards[p.ip];if(c)return c;
 const el=document.createElement('div');el.className='card';
 el.innerHTML='<canvas width=200 height=200></canvas><div class=n><b></b><a target=_blank></a></div>';
 document.getElementById('grid').appendChild(el);
 const cv=el.querySelector('canvas');
 c=cards[p.ip]={el,g:cv.getContext('2d'),img:null,id:-1,ip:p.ip,busy:false};
 c.img=c.g.createImageData(200,200);
 el.querySelector('b').textContent=p.name;const a=el.querySelector('a');a.textContent=p.ip;a.href='http://'+p.ip+'/';
 return c}
function draw(c,buf){const d=new Uint8Array(buf),kind=d[0];c.id=d[1]|d[2]<<8;
 for(let p=0;p<40000;p++){let v;const by=p>>3,bit=7-(p&7);
  if(kind==2)v=255;else if(kind==3)v=0;
  else if(kind==1)v=GRAY[((d[5003+by]>>bit)&1)*2+((d[3+by]>>bit)&1)];
  else v=((d[3+by]>>bit)&1)?255:0;
  c.img.data[4*p]=c.img.data[4*p+1]=c.img.data[4*p+2]=v;c.img.data[4*p+3]=255}
 c.g.putImageData(c.img,0,0)}
async function shot(c){if(c.busy)return;c.busy=true;
 try{const r=await fetch('http://'+c.ip+'/screen?since='+c.id+'&key='+encodeURIComponent(key),{cache:'no-store'});
  if(r.status==200)draw(c,await r.arrayBuffer());
  c.el.classList.toggle('old',r.status==401)}catch(e){c.el.classList.add('old')}
 c.busy=false}
async function peers(){
 try{const r=await fetch('/peers.json?key='+encodeURIComponent(key));
  if(r.status==401){document.getElementById('st').textContent='Clé admin refusée';ask();return}
  const l=await r.json();l.forEach(card);
  document.getElementById('st').textContent=l.length+' cigale(s)';
  Object.values(cards).forEach(c=>c.el.classList.toggle('old',!l.find(p=>p.ip==c.ip)))}
 catch(e){document.getElementById('st').textContent='Module injoignable'}}
peers();setInterval(peers,5000);
setInterval(()=>Object.values(cards).forEach(shot),1000);
</script></body></html>
)HTML";
