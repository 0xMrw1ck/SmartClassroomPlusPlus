const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
function setup(owner){
 const nodes=new Map(),listeners=new Map(),writes=[];
 const node=id=>{if(!nodes.has(id))nodes.set(id,{textContent:'',value:'',classList:{toggle(){}},replaceChildren(){},append(){},addEventListener(t,fn){this[t]=fn}});return nodes.get(id)};
 const buttons=Array.from({length:6},(_,i)=>({dataset:{channel:String(Math.floor(i/3)),mode:String(i%3)},classList:{toggle(){}},setAttribute(){},addEventListener(t,fn){this[t]=fn}}));
 const context=vm.createContext({document:{getElementById:node,querySelectorAll:()=>buttons,createElement:()=>node('created')},crypto:{randomUUID:()=> 'shared-command'},Date,Number,String,Object,Map,
 ref:(db,p)=>p,onValue:(p,fn)=>{listeners.set(p,fn);return ()=>listeners.delete(p)},get:async()=>({exists:()=>!!owner}),serverTimestamp:()=>({'.sv':'timestamp'}),set:async(p,data)=>writes.push({p,data}),
 readingState:r=>({fresh:!!r,telemetry:r?.telemetry||{},stamp:0,power:null,presence:null}),controlState:r=>({enabled:!!r,reason:''}),number:()=> '0'});
 vm.runInContext(fs.readFileSync(require('node:path').join(__dirname,'../public/app.js'),'utf8').replace(/^import .*;\r?\n/gm,'').replace(/start\(\);\s*$/,''),context);
 vm.runInContext("currentUser={uid:'second-google-user'};database={};watchShared(currentUser)",context);
 node('pairUid').value='device';return {node,buttons,writes,listeners,context};
}
test('second Google account joins without overwriting enrollment and can send firmware-compatible OFF',async()=>{
 const h=setup('original-owner');await h.node('pairForm').submit({preventDefault(){}});
 assert.deepEqual(h.writes,[{p:'members/second-google-user/device',data:true}]);
 h.listeners.get('members/second-google-user')({val:()=>({device:true})});
 h.listeners.get('devices/device')({exists:()=>true,val:()=>({viewerUid:'original-owner',telemetry:{remoteControl:true,standby:false,channels:[{mode:0},{mode:0}]}})});
 await h.buttons[2].click();const command=h.writes[1];assert.equal(command.p,'commands/device');assert.equal(command.data.mode,2);assert.equal(command.data.requesterUid,'original-owner');assert.equal(command.data.actorUid,'second-google-user');
 h.listeners.get('members/second-google-user')({val:()=>null});assert.equal(h.listeners.has('devices/device'),false);
 vm.runInContext('clearShared()',h.context);assert.equal(h.listeners.has('members/second-google-user'),false);
});
test('first account enrolls unregistered ID before adding membership',async()=>{
 const h=setup(null);await h.node('pairForm').submit({preventDefault(){}});
 assert.deepEqual(h.writes,[{p:'registry/device',data:'second-google-user'},{p:'members/second-google-user/device',data:true}]);
 assert.match(h.node('pairStatus').textContent,/Device added/);
});
