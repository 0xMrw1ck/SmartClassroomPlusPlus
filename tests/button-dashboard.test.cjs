const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const assets = fs.readFileSync(require('node:path').join(__dirname, '../SmartClassroomS3OLED/DashboardAssets.h'), 'utf8');
const js = assets.match(/DASHBOARD_JS\[\] PROGMEM = R"ASSET\(([\s\S]*?)\)ASSET";/)[1];
new vm.Script(js); // Check the complete embedded script, not just the test subset.
const nodes = new Map();
function element() {
  return {textContent:'',hidden:false,style:{},classList:{toggle(){},add(){}},
    replaceChildren(){},append(){},setAttribute(){}};
}
const buttons = [0,1,2,3,4,5].map(() => ({...element(),hasAttribute:k=>k==='data-ch'}));
const clock = {...element(),hasAttribute:()=>false};
const save = {...element(),hasAttribute:()=>false};
const resume = {...element(),hasAttribute:()=>false};
nodes.set('resumeDevice',resume);
const context = vm.createContext({document:{
  getElementById(id){if(!nodes.has(id))nodes.set(id,element());return nodes.get(id);},
  querySelectorAll(selector){return selector.startsWith('[data-ch],')?[...buttons,clock,save,resume]:[];}
}});
vm.runInContext("const $=id=>document.getElementById(id), modeNames=['AUTO','ON','OFF']; let connected=true, commandBusy=false, data=null; const metric=()=>'',fmt=()=>''; const renderChart=()=>{},renderEvents=()=>{},renderSettings=()=>{},drawRadar=()=>{};",context);
for(const name of ['enableControls','renderWarnings','renderData']) {
  vm.runInContext(js.split('\n').find(line=>line.startsWith('function '+name+'(')),context);
}
const sample = {room:'Room',standby:true,buttonCountdown:0,time:'now',timeValid:true,
  radarOnline:true,personCount:1,meterOnline:true,storageOK:true,oledOnline:true,
  channels:[{on:false,mode:2},{on:false,mode:2}],days:[],events:[]};
context.sample=sample;
vm.runInContext('renderData(sample)',context);
assert(buttons.every(b=>b.disabled), 'Physical standby must disable web relay controls despite radar presence');
assert.equal(clock.disabled,false);
assert.equal(save.disabled,false);
assert.equal(resume.disabled,false,'Resume must remain available in standby');
assert.equal(resume.hidden,false);
assert.equal(nodes.get('deviceStatus').textContent,'Device: STANDBY');
assert.match(nodes.get('warnings').textContent,/STANDBY/);
sample.standby=false;
sample.channels.forEach(c=>c.mode=0);
vm.runInContext('renderData(sample)',context);
assert(buttons.every(b=>!b.disabled),'Physical resume must re-enable controls');
assert.equal(nodes.get('deviceStatus').textContent,'Device: RUNNING');
assert.equal(resume.hidden,true);
vm.runInContext('connected=false;enableControls()',context);
assert([...buttons,clock,save,resume].every(b=>b.disabled),'Connection loss must disable all commands');
console.log('PASS: physical standby/resume status, relay lockout, retained utility controls and disconnected lockout');
