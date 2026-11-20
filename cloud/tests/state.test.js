import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readingState,number} from '../public/state.js';
const now=1800000000000;
const sample={updatedAt:now,telemetry:{power:44.3,meterOnline:true,radarOnline:true,personCount:1}};
test('fresh readings show actual meter power and occupancy',()=>{const s=readingState(sample,now+30000);assert.equal(s.fresh,true);assert.equal(s.power,44.3);assert.equal(s.presence,1)});
test('old or absent readings never appear as live power or live presence',()=>{for(const r of [null,{...sample,updatedAt:now-90001},{...sample,updatedAt:now+6000}]){const s=readingState(r,now);assert.equal(s.fresh,false);assert.equal(s.power,null);assert.equal(s.presence,null)}});
test('sensor faults hide cached values even when the device is connected',()=>{const s=readingState({...sample,telemetry:{...sample.telemetry,meterOnline:false,radarOnline:false}},now);assert.equal(s.fresh,true);assert.equal(s.power,null);assert.equal(s.presence,null)});
test('measured zero watts remains a valid live reading',()=>{assert.equal(readingState({...sample,telemetry:{...sample.telemetry,power:0}},now).power,0);assert.equal(number(null),'—')});
