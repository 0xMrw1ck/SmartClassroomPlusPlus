const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
test('mode command waits for matching device acknowledgement and reports denial',async()=>{
 const nodes=new Map();const node=id=>{if(!nodes.has(id))nodes.set(id,{textContent:'',value:'',hidden:false,classList:{toggle(){}},replaceChildren(){},append(){},addEventListener(type,fn){this.handler=fn}});return nodes.get(id)};
 const buttons=Array.from({length:6},(_,i)=>({dataset:{channel:String(Math.floor(i/3)),mode:String(i%3)},classList:{toggle(){}},setAttribute(){},addEventListener(type,fn){this.handler=fn}}));
 let written,ackListener,deny=false;
 const context=vm.createContext({document:{getElementById:node,querySelectorAll:()=>buttons,createElement:()=>node('created')},crypto:{randomUUID:()=> 'test-command'},Date,Number,String,Object,Map,
   ref:(db,p)=>p,onValue:(p,fn)=>{ackListener=fn;return ()=>{}},serverTimestamp:()=>({'.sv':'timestamp'}),set:async(p,data)=>{if(deny)throw {code:'PERMISSION_DENIED'};written={p,data}},readingState:(r)=>({fresh:!!r,telemetry:r?.telemetry||{},stamp:0,power:null,presence:null}),controlState:r=>({enabled:!!r&&!r.telemetry.standby,reason:'test'}),number:()=> '0'});
 const source=fs.readFileSync(require('node:path').join(__dirname,'../public/app.js'),'utf8').replace(/^import .*;\r?\n/gm,'').replace(/start\(\);\s*$/,'');vm.runInContext(source,context);
 vm.runInContext("currentUser={uid:'viewer'};database={};selected='device';records={device:{telemetry:{remoteControl:true,standby:false,channels:[{mode:0},{mode:0}]}}};watchAck();render();",context);
 await buttons[1].handler();assert.equal(written.p,'commands/device');assert.equal(written.data.mode,1);assert.equal(written.data.requesterUid,'viewer');assert.equal(buttons[1].disabled,true);assert.match(node('commandStatus').textContent,/Waiting/);
 ackListener({val:()=>({id:'older',status:'applied'})});assert.equal(buttons[1].disabled,true);
 ackListener({val:()=>({id:'test-command',status:'applied',channel:0,mode:1})});assert.equal(buttons[1].disabled,false);assert.match(node('commandStatus').textContent,/ESP32 applied Lights Manual ON/);
 deny=true;await buttons[2].handler();assert.match(node('commandStatus').textContent,/PERMISSION_DENIED/);assert.equal(buttons[2].disabled,false);
 vm.runInContext('records.device.telemetry.standby=true;render()',context);assert.equal(buttons[0].disabled,true);
});
