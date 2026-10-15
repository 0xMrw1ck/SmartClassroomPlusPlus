const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../SmartClassroomS3OLED/DashboardAssets.h'),'utf8');
const js=source.match(/DASHBOARD_JS\[\] PROGMEM = R"ASSET\(([\s\S]*?)\)ASSET";/)[1];
const nodes=new Map();
function element(){return {children:[],value:'',append(n){this.children.push(n)},replaceChildren(){this.children=[]},addEventListener(type,fn){this.handler=fn}}}
let config={saved:true,room:'Classroom',lightsSeconds:30,fanSeconds:15,radarTimeoutMs:3000,oledRefreshMs:1000,meterPollMs:2000,checkpointSeconds:300,buttonHoldMs:5000,xMin:-4000,xMax:4000,yMin:0,yMax:6000,timezoneMinutes:480,ssid:'Test network',apName:'Test AP',webUser:'admin',repository:'owner/repo',autoUpdate:true,activeLow:true,pins:[16,17,4,5,8,9,18,11,3]};
let posted,saveCount=0,rejectSave=false;
const context=vm.createContext({document:{getElementById(id){if(!nodes.has(id))nodes.set(id,element());return nodes.get(id)},createElement:element},fetch:async(url,options)=>{if(options?.method){posted=JSON.parse(options.body);saveCount++;if(rejectSave)return {ok:false,json:async()=>({error:'Settings could not be saved'})};config={...posted,saved:true};return {ok:true,json:async()=>({ok:true})}}return {ok:true,json:async()=>config}}});
vm.runInContext("const $=id=>document.getElementById(id);let configValues=null;",context);
vm.runInContext(js.split('\n').find(l=>l.startsWith('const configLabels=')),context);
vm.runInContext(js.split('\n').find(l=>l.startsWith('async function loadConfig(')),context);
vm.runInContext(js.split('\n').find(l=>l.startsWith("$('configForm').addEventListener")),context);
(async()=>{
 await vm.runInContext('loadConfig()',context);
 const inputs=[...nodes.get('automationFields').children,...nodes.get('configFields').children].flatMap(n=>n.children);
 const find=name=>inputs.find(n=>n.name===name);
 assert.equal(find('lightsSeconds').value,30);
 assert.equal(find('fanSeconds').value,15);
 assert.equal(nodes.get('automationFields').children.length,4);
 assert.equal(find('yMin').value,0);assert.equal(find('yMax').value,6);
 assert.equal(find('yMax').max,6);assert.equal(find('lightsSeconds').step,1);
 assert.match(nodes.get('configMessage').textContent,/restored from flash/);
 assert.equal(find('wifiPassword').type,'password');
 assert.equal(find('wifiPassword').value,'','Existing passwords must never populate the form');
 assert.equal(inputs.filter(n=>n.name.startsWith('pin')).length,9);
 find('lightsSeconds').value='60';find('fanSeconds').value='20';find('pin8').value='2';find('yMin').value='0.5';find('yMax').value='4.25';
 nodes.get('configForm').elements=inputs;
 await nodes.get('configForm').handler({preventDefault(){}});
 assert.equal(posted.lightsSeconds,60);assert.equal(posted.fanSeconds,20);
 assert.equal(posted.yMin,500);assert.equal(posted.yMax,4250);
 assert.equal(typeof posted.buttonHoldMs,'number');assert.equal(posted.autoUpdate,true);
 assert.deepEqual(posted.pins,[16,17,4,5,8,9,18,11,2]);
 assert.equal(posted.wifiPassword,'');assert.equal(posted.clearWifiPassword,false);
 assert.match(nodes.get('configMessage').textContent,/Saved/);
 // A fresh form load after the simulated device restart must show saved values.
 await vm.runInContext('loadConfig()',context);
 const restored=[...nodes.get('automationFields').children,...nodes.get('configFields').children].flatMap(n=>n.children);
 const saved=name=>restored.find(n=>n.name===name);
 assert.equal(saved('lightsSeconds').value,60);assert.equal(saved('fanSeconds').value,20);
 assert.equal(saved('yMin').value,0.5);assert.equal(saved('yMax').value,4.25);
 nodes.get('configForm').elements=restored;
 // Reject reversed/empty zones and invalid timers without sending a write.
 const writes=saveCount;saved('yMin').value='5';saved('yMax').value='4';
 await nodes.get('configForm').handler({preventDefault(){}});
 assert.equal(saveCount,writes);assert.match(nodes.get('configMessage').textContent,/near distance/);
 saved('yMin').value='0.5';saved('yMax').value='4.25';saved('lightsSeconds').value='0';
 await nodes.get('configForm').handler({preventDefault(){}});
 assert.equal(saveCount,writes);assert.match(nodes.get('configMessage').textContent,/whole seconds/);
 saved('lightsSeconds').value='60';rejectSave=true;
 await nodes.get('configForm').handler({preventDefault(){}});
 assert.match(nodes.get('configMessage').textContent,/could not be saved/);
 console.log('PASS: automation settings, metre/mm conversion, saved-value reload, bounds, failed-save feedback and typed GPIO edits');
})().catch(e=>{console.error(e);process.exitCode=1});

