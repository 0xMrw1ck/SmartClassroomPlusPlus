const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../SmartClassroomS3OLED/DashboardAssets.h'),'utf8');
const js=source.match(/DASHBOARD_JS\[\] PROGMEM = R"ASSET\(([\s\S]*?)\)ASSET";/)[1];
const nodes=new Map();
function element(){return {children:[],value:'',append(n){this.children.push(n)},replaceChildren(){this.children=[]},addEventListener(type,fn){this.handler=fn}}}
const config={room:'Classroom',lightsSeconds:30,fanSeconds:15,radarTimeoutMs:3000,oledRefreshMs:1000,meterPollMs:2000,checkpointSeconds:300,buttonHoldMs:5000,xMin:-4000,xMax:4000,yMin:0,yMax:6000,timezoneMinutes:480,ssid:'Test network',apName:'Test AP',webUser:'admin',repository:'owner/repo',autoUpdate:true,activeLow:true,pins:[16,17,4,5,8,9,10,11,3]};
let posted;
const context=vm.createContext({document:{getElementById(id){if(!nodes.has(id))nodes.set(id,element());return nodes.get(id)},createElement:element},fetch:async(url,options)=>{if(options?.method){posted=JSON.parse(options.body);return {ok:true,json:async()=>({ok:true})}}return {ok:true,json:async()=>config}}});
vm.runInContext("const $=id=>document.getElementById(id);let configValues=null;",context);
vm.runInContext(js.split('\n').find(l=>l.startsWith('const configLabels=')),context);
vm.runInContext(js.split('\n').find(l=>l.startsWith('async function loadConfig(')),context);
vm.runInContext(js.split('\n').find(l=>l.startsWith("$('configForm').addEventListener")),context);
(async()=>{
 await vm.runInContext('loadConfig()',context);
 const inputs=nodes.get('configFields').children.flatMap(n=>n.children);
 const find=name=>inputs.find(n=>n.name===name);
 assert.equal(find('lightsSeconds').value,30);
 assert.equal(find('fanSeconds').value,15);
 assert.equal(find('wifiPassword').type,'password');
 assert.equal(find('wifiPassword').value,'','Existing passwords must never populate the form');
 assert.equal(inputs.filter(n=>n.name.startsWith('pin')).length,9);
 find('lightsSeconds').value='60';find('fanSeconds').value='20';find('pin8').value='2';
 nodes.get('configForm').elements=inputs;
 await nodes.get('configForm').handler({preventDefault(){}});
 assert.equal(posted.lightsSeconds,60);assert.equal(posted.fanSeconds,20);
 assert.equal(typeof posted.buttonHoldMs,'number');assert.equal(posted.autoUpdate,true);
 assert.deepEqual(posted.pins,[16,17,4,5,8,9,10,11,2]);
 assert.equal(posted.wifiPassword,'');assert.equal(posted.clearWifiPassword,false);
 assert.match(nodes.get('configMessage').textContent,/Saved/);
 console.log('PASS: actual settings form loads defaults, keeps passwords blank and submits typed timer/GPIO edits');
})().catch(e=>{console.error(e);process.exitCode=1});

