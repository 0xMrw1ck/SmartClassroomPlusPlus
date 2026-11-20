import {test} from 'node:test';
import assert from 'node:assert/strict';
import {minuteCsv,hourlyCsv,dailyCsv} from '../public/history.js';
import {energyWorkbook,csvObjects,excelTime} from '../public/xlsx.js';

const workbookText=bytes=>Buffer.from(bytes).toString('latin1');
const sheet=(text,n)=>text.split(`xl/worksheets/sheet${n}.xml<?xml`)[1].split('</worksheet>')[0];

test('online history becomes one workbook with Summary, Daily, Hourly and Minute sheets',()=>{
  const minutes={'0802':{t:1,z:480,V:220.8,A:1.162,W:248.6,Wp:251.3,pf:0.97,Hz:60,Wh:4.143,mWh:152417,l:1,f:1,o:1,m:'AUTO',n:30,ok:1},'1405':{t:2,z:480,l:1,f:0,o:1,m:'MANUAL',ok:0}};
  const hours={'2026-11-20':{'08':{t:0,kWh:0.239,avgW:239.3,peakW:256.4,minW:1.7,avgV:221,avgPf:0.97,occ:57,lights:58,fan:56,n:60}}};
  const days={'2026-11-20':{kWh:1.115,avgW:85.8,peakW:258.1,peakT:'09:37',occH:4.4,lightsH:4.6,fanH:4.3,n:780,cost:13.38}};
  const text=workbookText(energyWorkbook(minuteCsv('2026-11-20',minutes,'Room 101'),hourlyCsv(hours),dailyCsv(days),{title:'Report'}));
  assert.ok(text.startsWith('PK'));
  assert.match(text,/<sheet name="Summary" sheetId="1"[^>]*\/><sheet name="Daily"[^>]*\/><sheet name="Hourly"[^>]*\/><sheet name="Minute"/);
  const minute=sheet(text,4),daily=sheet(text,2);
  assert.match(minute,/<autoFilter ref="A7:Q9"\/>/);
  assert.ok(minute.includes('IFERROR(SUBTOTAL(101,D8:D9),&quot;&quot;)'),'filtered average of voltage');
  assert.ok(minute.includes('SUBTOTAL(109,J8:J9)'),'filtered energy total');
  assert.ok(!/<c r="D9"/.test(minute),'no-meter minute keeps empty voltage');
  assert.match(daily,/<c r="B8" s="4"><v>1.115<\/v>/);
  assert.ok(sheet(text,1).includes("'Minute'!D2"),'summary links to the minute averages');
});
test('CSV parsing handles quoted commas and Excel serial dates',()=>{
  assert.deepEqual(csvObjects('a,b\n"x, y","q""z"\n'),[{a:'x, y',b:'q"z'}]);
  assert.equal(excelTime('2026-11-20T12:00'),46346.5);
  assert.equal(excelTime(''),null);
});
