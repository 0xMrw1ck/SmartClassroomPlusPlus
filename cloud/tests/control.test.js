import {test} from 'node:test';
import assert from 'node:assert/strict';
import {controlState} from '../public/state.js';
const now=1800000000000;
const record={updatedAt:now,telemetry:{remoteControl:true,standby:false}};
test('controls require fresh telemetry and explicit firmware support',()=>{
 assert.equal(controlState(record,now).enabled,true);
 for(const r of [null,{...record,updatedAt:now-90001},{updatedAt:now,telemetry:{}},{...record,telemetry:{remoteControl:true,standby:true}}])assert.equal(controlState(r,now).enabled,false);
});
test('older firmware directs the user to the OTA update',()=>assert.match(controlState({updatedAt:now,telemetry:{}},now).reason,/1.2.0/));
