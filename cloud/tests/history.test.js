import {test} from 'node:test';
import assert from 'node:assert/strict';
import {minuteCsv,hourlyCsv,dailyCsv,tzSuffix,csvText,shiftDate,MINUTE_HEADER,HOURLY_HEADER,DAILY_HEADER} from '../public/history.js';

test('minute rows match the agreed CSV columns, sorted, with local UTC+8 timestamps',()=>{
  const rows={'0802':{t:1,z:480,V:220.8,A:1.162,W:248.6,Wp:251.3,pf:0.97,Hz:60,Wh:4.143,mWh:152417,l:1,f:1,o:1,m:'AUTO',n:30,ok:1},
    '0800':{t:0,z:480,V:221.4,A:0.021,W:1.8,Wp:2.3,pf:0.39,Hz:60,Wh:0.03,mWh:152411,l:0,f:0,o:0,m:'AUTO',n:30,ok:1}};
  const lines=minuteCsv('2026-11-20',rows,'Room 101').trim().split('\n');
  assert.equal(lines[0],MINUTE_HEADER);
  assert.equal(lines[1],'2026-11-20T08:00:00+08:00,Room 101,221.4,0.021,1.8,2.3,0.39,60.0,0.030,152411,OFF,OFF,AUTO,0,30,1');
  assert.equal(lines[2],'2026-11-20T08:02:00+08:00,Room 101,220.8,1.162,248.6,251.3,0.97,60.0,4.143,152417,ON,ON,AUTO,1,30,1');
});
test('minutes without a meter reading stay blank, never zero',()=>{
  const line=minuteCsv('2026-11-20',{'1405':{t:0,z:480,l:1,f:0,o:1,m:'MANUAL',ok:0}},'R').trim().split('\n')[1];
  assert.equal(line,'2026-11-20T14:05:00+08:00,R,,,,,,,,,ON,OFF,MANUAL,1,0,0');
});
test('hourly and daily summaries keep gaps blank and include cost when set',()=>{
  const hours=hourlyCsv({'2026-11-19':{'14':{t:0,kWh:0.187,avgW:249.2,peakW:255.9,minW:245.7,avgV:222.4,avgPf:0.97,occ:45,lights:45,fan:45,n:45},'15':{t:0,kWh:0,occ:0,lights:0,fan:0,n:0}}}).trim().split('\n');
  assert.equal(hours[0],HOURLY_HEADER);
  assert.equal(hours[1],'2026-11-19T14:00,0.187,249.2,255.9,245.7,222.4,0.97,45,45,45,45');
  assert.equal(hours[2],'2026-11-19T15:00,0.000,,,,,,0,0,0,0');
  const days=dailyCsv({'2026-11-19':{kWh:2.018,avgW:85,peakW:257.9,peakT:'08:00',occH:7.8,lightsH:8,fanH:7.9,n:1425,cost:24.22},'2026-11-18':{kWh:2.035,n:1440,occH:7.8,lightsH:7.9,fanH:8.6}}).trim().split('\n');
  assert.equal(days[0],DAILY_HEADER);
  assert.equal(days[1],'2026-11-18,2.035,,,,7.8,7.9,8.6,1440,');
  assert.equal(days[2],'2026-11-19,2.018,85.0,257.9,08:00,7.8,8.0,7.9,1425,24.22');
});
test('helpers: timezone suffix, CSV quoting, date shifting',()=>{
  assert.equal(tzSuffix(480),'+08:00');assert.equal(tzSuffix(-330),'-05:30');assert.equal(tzSuffix(undefined),'+00:00');
  assert.equal(csvText('Room, 101'),'"Room, 101"');assert.equal(csvText('a"b'),'"a""b"');
  assert.equal(shiftDate('2026-03-01',-6),'2026-02-23');
});
