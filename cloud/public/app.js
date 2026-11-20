import {initializeApp} from 'https://www.gstatic.com/firebasejs/12.19.0/firebase-app.js';
import {getAuth,GoogleAuthProvider,signInWithRedirect,getRedirectResult,onAuthStateChanged,signOut} from 'https://www.gstatic.com/firebasejs/12.19.0/firebase-auth.js';
import {getDatabase,ref,onValue,query,orderByChild,orderByKey,startAt,endAt,equalTo,set,get,serverTimestamp} from 'https://www.gstatic.com/firebasejs/12.19.0/firebase-database.js';
import {readingState,number,controlState} from './state.js';
import {minuteCsv,hourlyCsv,dailyCsv,shiftDate} from './history.js';
const $=id=>document.getElementById(id);
let auth,database,unsubscribe=null,records={},selected='',serverOffset=0,currentUser=null;
let unsubscribeAck=null,ackDevice='';
let unsubscribeMembers=null,ownedRecords={},sharedRecords={};
const sharedListeners=new Map();
function mergeDevices(){records={...ownedRecords,...sharedRecords};selectDevices();message(Object.keys(records).length?'':'No classroom has published to your account yet. Add your ESP32 Device ID below.')}
function clearShared(){if(unsubscribeMembers)unsubscribeMembers();unsubscribeMembers=null;for(const stop of sharedListeners.values())stop();sharedListeners.clear();ownedRecords={};sharedRecords={}}
function watchShared(user){unsubscribeMembers=onValue(ref(database,'members/'+user.uid),snapshot=>{
  const members=snapshot.val()||{};
  for(const [device,stop] of sharedListeners)if(members[device]!==true){stop();sharedListeners.delete(device);delete sharedRecords[device]}
  for(const device of Object.keys(members))if(members[device]===true&&!sharedListeners.has(device)){
    const stop=onValue(ref(database,'devices/'+device),s=>{if(s.exists())sharedRecords[device]=s.val();else delete sharedRecords[device];mergeDevices()},()=>{delete sharedRecords[device];mergeDevices();message('Shared device data unavailable.')});sharedListeners.set(device,stop);
  }
  mergeDevices();
},()=>message('Shared device list unavailable.'))}
const pending=new Map(),commandMessages=new Map(),modeNames=['AUTO','Manual ON','Manual OFF'];
function watchAck(){
  if(ackDevice===selected)return;
  if(unsubscribeAck)unsubscribeAck();unsubscribeAck=null;ackDevice=selected;
  if(!selected || !currentUser)return;
  const device=selected;
  unsubscribeAck=onValue(ref(database,'acks/'+device),snapshot=>{
    const ack=snapshot.val(),sent=pending.get(device);if(!sent || ack?.id!==sent.id)return;
    pending.delete(device);
    commandMessages.set(device,ack.status==='applied'?'ESP32 applied '+(ack.channel===0?'Lights':'Fan')+' '+modeNames[ack.mode]+'. Output is commanded; physical contacts have no feedback.':ack.status==='expired'?'Command expired without being applied. Try again while the device is online.':'ESP32 rejected command: standby or update in progress.');render();
  },()=>{commandMessages.set(device,'Command acknowledgement unavailable. Check device status.');render()});
}
function message(text){$('message').textContent=text}
function rows(id,values){const root=$(id);root.replaceChildren();for(const [name,value] of values){const row=document.createElement('div'),dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=name;dd.textContent=value;row.append(dt,dd);root.append(row)}}
function render(){
  const record=records[selected],state=readingState(record,Date.now()+serverOffset),d=state.telemetry;
  $('connection').textContent=state.fresh?'ESP32 connected':'ESP32 offline / awaiting data';$('connection').classList.toggle('offline',!state.fresh);
  $('lastSeen').textContent=Number.isFinite(state.stamp)&&state.stamp>0?'Last received: '+new Date(state.stamp).toLocaleString()+(state.fresh?'':' · Last reported data; live readings unavailable'):'No ESP32 reading received yet.';
  $('power').textContent=state.power===null?'—':number(state.power)+' W';$('powerNote').textContent=!state.fresh?'ESP32 offline':d.meterOnline?'Measured by PZEM':'PZEM unavailable';
  $('presence').textContent=state.presence===null?'Unknown':state.presence>0?'Detected':'No targets';$('presenceNote').textContent=state.presence===null?'Live radar unavailable':state.presence+' tracked target(s) in zone';
  $('energy').textContent=number(d.todayKWh,3)+(Number.isFinite(d.todayKWh)?' kWh':'');$('energyNote').textContent=state.fresh?'Device daily ledger':'Last reported daily total';
  $('deviceState').textContent=state.fresh?(d.standby?'STANDBY':'RUNNING'):'OFFLINE';$('deviceNote').textContent=state.fresh?'Receiving updates':record?'Last state: '+(d.standby?'STANDBY':'RUNNING'):'Waiting for device';
  for(const [id,i] of [['lights',0],['fan',1]])$(id).textContent=d.channels?.[i]?(d.channels[i].on?'ON':'OFF')+(state.fresh?'':' (last reported)'):'—';
  const permission=controlState(record,Date.now()+serverOffset),sent=pending.get(selected);
  if(sent && Date.now()>sent.expires){pending.delete(selected);commandMessages.set(selected,'No acknowledgement received. Check the actual device state before retrying.');}
  for(const button of document.querySelectorAll('[data-channel][data-mode]')) {
    const channel=Number(button.dataset.channel),mode=Number(button.dataset.mode);
    button.disabled=!currentUser || !permission.enabled || pending.has(selected);
    const active=state.fresh&&d.channels?.[channel]?.mode===mode;
    button.classList.toggle('selected',active);button.setAttribute('aria-pressed',String(active));
  }
  $('controlHelp').textContent=permission.reason;
  $('commandStatus').textContent=pending.has(selected)?'Command sent. Waiting for ESP32 acknowledgement (normally within 5-15 seconds).':commandMessages.get(selected)||'';
  rows('electrical',[['Voltage',state.fresh&&d.meterOnline?number(d.voltage)+' V':'—'],['Current',state.fresh&&d.meterOnline?number(d.current,3)+' A':'—'],['Frequency',state.fresh&&d.meterOnline?number(d.frequency)+' Hz':'—'],['Power factor',state.fresh&&d.meterOnline?number(d.pf,2):'—']]);
  $('sensorStatus').textContent=state.fresh?'Radar: '+(d.radarOnline?'online':'offline')+' · PZEM: '+(d.meterOnline?'online':'offline')+' · OLED: '+(d.oledOnline?'online':'offline'):'Sensor status unavailable while ESP32 is offline';
  $('lifetime').textContent='Last reported meter lifetime: '+number(d.totalKWh,3)+' kWh';
  // Prefer the 1.3.0 history (peak + cost); fall back to the legacy daily ledger in telemetry.
  const detailed=Object.keys(historyDays).length>0;
  const days=detailed?Object.entries(historyDays).map(([date,v])=>({date,kWh:Number(v.kWh),peakW:Number(v.peakW),cost:Number(v.cost)})).sort((a,b)=>b.date.localeCompare(a.date)).slice(0,31)
    :Array.isArray(d.days)?[...d.days].sort((a,b)=>String(b.date).localeCompare(String(a.date))):[];
  $('history').replaceChildren();for(const day of days){const row=document.createElement('tr');for(const value of [String(day.date),number(day.kWh,3),Number.isFinite(day.peakW)?number(day.peakW,1):'—',Number.isFinite(day.cost)?number(day.cost,2):'—']){const cell=document.createElement('td');cell.textContent=value;row.append(cell)}$('history').append(row)}$('historyEmpty').hidden=days.length>0;
  $('historyNote').textContent=detailed?'From 1-minute history uploaded by the ESP32':'Up to 31 recorded dates (device daily ledger)';
  for(const id of ['downloadMinute','downloadHourly','downloadDaily'])$(id).disabled=!currentUser||!selected||downloadBusy;
}
let historyDays={},historyDevice='',unsubscribeHistory=null,downloadBusy=false;
function watchHistory(){
  if(historyDevice===selected)return;
  if(unsubscribeHistory)unsubscribeHistory();unsubscribeHistory=null;historyDays={};historyDevice=selected;
  if(!selected||!currentUser)return;
  unsubscribeHistory=onValue(ref(database,'history/'+selected+'/days'),s=>{historyDays=s.val()||{};render()},()=>{historyDays={};render()});
}
function saveCsv(name,text){const url=URL.createObjectURL(new Blob([text],{type:'text/csv'})),a=document.createElement('a');a.href=url;a.download=name;document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000)}
async function download(kind){
  const device=selected,date=$('historyDate').value,room=records[device]?.telemetry?.room||'classroom';
  if(!device||downloadBusy)return;
  if(kind!=='daily'&&!/^\d{4}-\d{2}-\d{2}$/.test(date)){$('downloadStatus').textContent='Choose a date first.';return}
  downloadBusy=true;render();$('downloadStatus').textContent='Downloading…';
  try{
    const base='history/'+device+'/';let text,rows=0,name;
    if(kind==='minute'){const v=(await get(ref(database,base+'minutes/'+date))).val()||{};rows=Object.keys(v).length;text=minuteCsv(date,v,room);name=`classroom-energy-1min-${date}.csv`}
    else if(kind==='hourly'){const v=(await get(query(ref(database,base+'hours'),orderByKey(),startAt(shiftDate(date,-6)),endAt(date)))).val()||{};rows=Object.values(v).reduce((n,day)=>n+Object.keys(day||{}).length,0);text=hourlyCsv(v);name=`classroom-energy-hourly-${shiftDate(date,-6)}_to_${date}.csv`}
    else{const v=(await get(ref(database,base+'days'))).val()||{};rows=Object.keys(v).length;text=dailyCsv(v);name='classroom-energy-daily.csv'}
    if(!rows){$('downloadStatus').textContent='No rows uploaded for that period yet. The device needs firmware 1.3.0 and internet.';return}
    saveCsv(name,text);$('downloadStatus').textContent=`Downloaded ${rows} row(s).`;
  }catch(e){$('downloadStatus').textContent='Download failed: '+(e.code||e.message)}
  finally{downloadBusy=false;render()}
}
$('downloadMinute').addEventListener('click',()=>download('minute'));
$('downloadHourly').addEventListener('click',()=>download('hourly'));
$('downloadDaily').addEventListener('click',()=>download('daily'));
$('historyDate').value=new Date(Date.now()-new Date().getTimezoneOffset()*60000).toISOString().slice(0,10);
function selectDevices(){const previous=selected;$('deviceSelect').replaceChildren();for(const [uid,record] of Object.entries(records)){const option=document.createElement('option');option.value=uid;option.textContent=record.telemetry?.room||'Classroom '+uid.slice(0,6);$('deviceSelect').append(option)}selected=records[previous]?previous:Object.keys(records)[0]||'';$('deviceSelect').value=selected;watchAck();watchHistory();render()}
$('deviceSelect').addEventListener('change',()=>{selected=$('deviceSelect').value;watchAck();watchHistory();render()});
for(const button of document.querySelectorAll('[data-channel][data-mode]'))button.addEventListener('click',async()=>{
  const device=selected,channel=Number(button.dataset.channel),mode=Number(button.dataset.mode);
  if(!currentUser || !controlState(records[device],Date.now()+serverOffset).enabled || pending.has(device))return;
  const id=crypto.randomUUID();pending.set(device,{id,expires:Date.now()+65000});commandMessages.delete(device);render();
  // Keep the enrollment UID for compatibility with installed 1.2.0 firmware.
  try{await set(ref(database,'commands/'+device),{id,channel,mode,requesterUid:records[device].viewerUid||currentUser.uid,actorUid:currentUser.uid,issuedAt:serverTimestamp()})}
  catch(e){if(pending.get(device)?.id===id)pending.delete(device);commandMessages.set(device,'Command could not be sent: '+e.code);render()}
});
$('signIn').addEventListener('click',async()=>{if(!auth)return;$('signIn').disabled=true;try{await signInWithRedirect(auth,new GoogleAuthProvider())}catch(e){message('Sign-in failed: '+e.code)}finally{$('signIn').disabled=false}});
$('signOut').addEventListener('click',()=>signOut(auth));
$('pairForm').addEventListener('submit',async e=>{e.preventDefault();const uid=$('pairUid').value.trim();if(!currentUser || !/^[A-Za-z0-9_-]{1,128}$/.test(uid)){ $('pairStatus').textContent='Enter a valid ESP32 device ID.';return }try{
  const enrollment=ref(database,'registry/'+uid);
  if(!(await get(enrollment)).exists())try{await set(enrollment,currentUser.uid)}catch(error){if(!(await get(enrollment)).exists())throw error}
  await set(ref(database,'members/'+currentUser.uid+'/'+uid),true);
  $('pairStatus').textContent='Device added to your account. Wait for its next update. Keep its ID private: anyone signed in with Google who knows it can join.';
}catch(e){$('pairStatus').textContent='Could not add device: '+e.code}});
async function start(){
  try{
    const response=await fetch('config.json',{cache:'no-store'});if(!response.ok)throw Error('Firebase configuration unavailable');const config=await response.json();
    const app=initializeApp(config);auth=getAuth(app);database=getDatabase(app);
    rows('setupValues',[['Firebase API key',config.apiKey],['Database URL',config.databaseURL],['Publish interval','30 seconds recommended']]);
    onValue(ref(database,'.info/serverTimeOffset'),s=>{serverOffset=s.val()||0});
    onAuthStateChanged(auth,user=>{
      if(unsubscribe){unsubscribe();unsubscribe=null}clearShared();if(unsubscribeAck){unsubscribeAck();unsubscribeAck=null}ackDevice='';if(unsubscribeHistory){unsubscribeHistory();unsubscribeHistory=null}historyDevice='';historyDays={};pending.clear();commandMessages.clear();currentUser=user;records={};selected='';render();
      $('login').hidden=!!user;$('dashboard').hidden=!user;$('signOut').hidden=!user;$('pairing').hidden=!user;message('');
      if(user){watchShared(user);unsubscribe=onValue(query(ref(database,'devices'),orderByChild('viewerUid'),equalTo(user.uid)),s=>{ownedRecords=s.val()||{};mergeDevices()},e=>{ownedRecords={};mergeDevices();message('Data access unavailable: '+e.code)})}
    });
    await getRedirectResult(auth).catch(e=>message('Sign-in failed: '+e.code));
    setInterval(render,5000);
  }catch(e){$('signIn').disabled=true;message('Dashboard setup incomplete: '+e.message)}
}
start();
