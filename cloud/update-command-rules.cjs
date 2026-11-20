const fs=require('node:fs'),path=require('node:path');
const deviceAuth="auth != null && auth.uid === $device && auth.token.firebase.sign_in_provider === 'anonymous' && root.child('registry').child($device).isString()";
const googleAuth="auth != null && auth.token.firebase.sign_in_provider === 'google.com' && auth.token.email_verified === true";
const viewerAuth=`${googleAuth} && (auth.uid === root.child('registry').child($device).val() || root.child('members').child(auth.uid).child($device).val() === true)`;
for(const file of ['database.rules.json','database.rules.example.json']){
 const target=path.join(__dirname,file),data=JSON.parse(fs.readFileSync(target,'utf8'));
 data.rules.registry.$device['.read']=`auth != null && (auth.uid === $device || (${googleAuth}))`;
 data.rules.registry.$device['.write']=`${googleAuth} && newData.val() === auth.uid && (!data.exists() || data.val() === auth.uid)`;
 data.rules.members={$viewer:{'.read':`${googleAuth} && auth.uid === $viewer`,$device:{'.write':`${googleAuth} && auth.uid === $viewer && root.child('registry').child($device).isString()`,'.validate':'newData.val() === true'}}};
 data.rules.devices.$device['.read']=viewerAuth;
 data.rules.commands={$device:{'.read':`(${deviceAuth}) || (${viewerAuth})`,'.write':viewerAuth,
  '.validate':"newData.hasChildren(['id','channel','mode','requesterUid','issuedAt']) && root.child('devices').child($device).child('telemetry').child('remoteControl').val() === true && root.child('devices').child($device).child('telemetry').child('standby').val() === false && root.child('devices').child($device).child('updatedAt').val() >= now - 90000",
  id:{'.validate':"newData.isString() && newData.val().matches(/^[A-Za-z0-9_-]{1,64}$/)"},
  channel:{'.validate':"newData.val() === 0 || newData.val() === 1"},mode:{'.validate':"newData.val() === 0 || newData.val() === 1 || newData.val() === 2"},
  requesterUid:{'.validate':"newData.isString() && newData.val() === root.child('registry').child($device).val() && ((newData.parent().child('actorUid').val() === auth.uid) || (!newData.parent().child('actorUid').exists() && auth.uid === newData.val()))"},
  actorUid:{'.validate':'newData.isString() && newData.val() === auth.uid'},issuedAt:{'.validate':"newData.isNumber() && newData.val() === now"},'$other':{'.validate':false}}};
 data.rules.acks={$device:{'.read':viewerAuth,'.write':deviceAuth,
  '.validate':"newData.hasChildren(['id','channel','mode','status','updatedAt'])",
  id:{'.validate':"newData.isString() && newData.val().matches(/^[A-Za-z0-9_-]{1,64}$/)"},
  channel:{'.validate':"newData.val() === 0 || newData.val() === 1"},mode:{'.validate':"newData.val() === 0 || newData.val() === 1 || newData.val() === 2"},
  status:{'.validate':"newData.val() === 'applied' || newData.val() === 'expired' || newData.val() === 'blocked_standby'"},
  updatedAt:{'.validate':"newData.isNumber() && newData.val() === now"},'$other':{'.validate':false}}};
 fs.writeFileSync(target,JSON.stringify(data,null,2)+'\n');
}
console.log('Updated Google-account sharing by Device ID, preserving existing enrollments and firmware compatibility.');
