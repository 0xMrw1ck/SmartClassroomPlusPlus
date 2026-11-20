// Single-file Excel (.xlsx) energy report. Shared by the online dashboard and (as an embedded copy) the ESP32
// local dashboard: keep both identical (tests/xlsx-sync.test.cjs checks it). No external libraries.
const XLSX_ENC=new TextEncoder();
const XLSX_CRC=(()=>{const t=new Uint32Array(256);for(let n=0;n<256;n++){let c=n;for(let k=0;k<8;k++)c=c&1?0xEDB88320^(c>>>1):c>>>1;t[n]=c>>>0}return t})();
function xlsxCrc(b){let c=0xFFFFFFFF;for(let i=0;i<b.length;i++)c=XLSX_CRC[(c^b[i])&255]^(c>>>8);return (c^0xFFFFFFFF)>>>0}
function xlsxZip(files){
  const parts=[],central=[];let offset=0;
  for(const f of files){
    const name=XLSX_ENC.encode(f.name),data=XLSX_ENC.encode(f.data),crc=xlsxCrc(data);
    const h=new DataView(new ArrayBuffer(30));
    h.setUint32(0,0x04034b50,true);h.setUint16(4,20,true);h.setUint16(6,0x0800,true);h.setUint16(12,0x21,true);
    h.setUint32(14,crc,true);h.setUint32(18,data.length,true);h.setUint32(22,data.length,true);h.setUint16(26,name.length,true);
    parts.push(new Uint8Array(h.buffer),name,data);
    const c=new DataView(new ArrayBuffer(46));
    c.setUint32(0,0x02014b50,true);c.setUint16(4,20,true);c.setUint16(6,20,true);c.setUint16(8,0x0800,true);c.setUint16(14,0x21,true);
    c.setUint32(16,crc,true);c.setUint32(20,data.length,true);c.setUint32(24,data.length,true);c.setUint16(28,name.length,true);c.setUint32(42,offset,true);
    central.push(new Uint8Array(c.buffer),name);
    offset+=30+name.length+data.length;
  }
  const cdSize=central.reduce((n,p)=>n+p.length,0),e=new DataView(new ArrayBuffer(22));
  e.setUint32(0,0x06054b50,true);e.setUint16(8,files.length,true);e.setUint16(10,files.length,true);e.setUint32(12,cdSize,true);e.setUint32(16,offset,true);
  const all=[...parts,...central,new Uint8Array(e.buffer)],out=new Uint8Array(all.reduce((n,p)=>n+p.length,0));
  let p=0;for(const a of all){out.set(a,p);p+=a.length}
  return out;
}
const xe=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])).replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g,'');
function xcol(i){let s='';i++;while(i){const m=(i-1)%26;s=String.fromCharCode(65+m)+s;i=(i-m-1)/26}return s}
// Cell styles (see styles.xml below): plain formats, then the same formats on the shaded summary band.
const XS={text:0,datetime:1,date:2,'0.0':3,'0.000':4,'0.00':5,'0':6,header:7,label:8,title:9,note:10};
const XB={'0.0':11,'0.000':12,'0.00':13,'0':14};
const XSTYLES='<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
 +'<numFmts count="4"><numFmt numFmtId="164" formatCode="yyyy-mm-dd hh:mm"/><numFmt numFmtId="165" formatCode="yyyy-mm-dd"/><numFmt numFmtId="166" formatCode="0.0"/><numFmt numFmtId="167" formatCode="0.000"/></numFmts>'
 +'<fonts count="5"><font><sz val="11"/><name val="Calibri"/></font><font><b/><sz val="11"/><name val="Calibri"/></font><font><b/><sz val="11"/><color rgb="FFFFFFFF"/><name val="Calibri"/></font><font><b/><sz val="14"/><name val="Calibri"/></font><font><i/><sz val="10"/><color rgb="FF555555"/><name val="Calibri"/></font></fonts>'
 +'<fills count="4"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FF1F4E78"/><bgColor indexed="64"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor rgb="FFE7EEF6"/><bgColor indexed="64"/></patternFill></fill></fills>'
 +'<borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="16">'
 +'<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>'
 +['164','165','166','167','2','1'].map(f=>`<xf numFmtId="${f}" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>`).join('')
 +'<xf numFmtId="0" fontId="2" fillId="2" borderId="0" xfId="0" applyFont="1" applyFill="1" applyAlignment="1"><alignment horizontal="center" vertical="center" wrapText="1"/></xf>'
 +'<xf numFmtId="0" fontId="1" fillId="3" borderId="0" xfId="0" applyFont="1" applyFill="1"/><xf numFmtId="0" fontId="3" fillId="0" borderId="0" xfId="0" applyFont="1"/><xf numFmtId="0" fontId="4" fillId="0" borderId="0" xfId="0" applyFont="1"/>'
 +['166','167','2','1','0'].map(f=>`<xf numFmtId="${f}" fontId="0" fillId="3" borderId="0" xfId="0" applyNumberFormat="1" applyFill="1"/>`).join('')
 +'</cellXfs><cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>';
function xcell(ref,v,s){
  if(v===null||v===undefined||v===''||(typeof v==='number'&&!Number.isFinite(v)))return '';
  if(typeof v==='object'&&v.f)return `<c r="${ref}" s="${s}"><f>${xe(v.f)}</f></c>`;
  if(typeof v==='number')return `<c r="${ref}" s="${s}"><v>${v}</v></c>`;
  return `<c r="${ref}" s="${s}" t="inlineStr"><is><t xml:space="preserve">${xe(v)}</t></is></c>`;
}
const xrow=(r,cells,ht)=>`<row r="${r}"${ht?` ht="${ht}" customHeight="1"`:''}>${cells.join('')}</row>`;
const HEAD=7,FIRST=8;
const lastRow=sheet=>Math.max(FIRST,FIRST+sheet.rows.length-1);
// Data sheet: rows 2-6 hold SUBTOTAL formulas that only count visible (filtered) rows; row 7 = header with filters.
function xlsxDataSheet(sheet,selected){
  const cols=sheet.columns,last=lastRow(sheet),lastC=xcol(cols.length-1),rows=[];
  rows.push(xrow(1,[xcell('A1',sheet.title,XS.title)],22));
  const band=(r,label,fn,want)=>xrow(r,[xcell('A'+r,label,XS.label),...cols.map((c,i)=>{
    if(i===0||!want(c))return '';const L=xcol(i);
    return xcell(L+r,{f:`IFERROR(SUBTOTAL(${fn},${L}${FIRST}:${L}${last}),"")`},XB[c.fmt]??14);
  })]);
  rows.push(band(2,'Average (shown rows)',101,c=>c.stats),band(3,'Minimum',105,c=>c.stats),band(4,'Maximum',104,c=>c.stats),band(5,'Total',109,c=>c.total));
  rows.push(xrow(6,[xcell('A6','Rows shown',XS.label),xcell('B6',{f:`SUBTOTAL(103,A${FIRST}:A${last})`},XB['0'])]));
  rows.push(xrow(HEAD,cols.map((c,i)=>xcell(xcol(i)+HEAD,c.h,XS.header)),32));
  sheet.rows.forEach((r,ri)=>rows.push(xrow(FIRST+ri,cols.map((c,i)=>xcell(xcol(i)+(FIRST+ri),r[i],XS[c.fmt]??0)))));
  const widths=cols.map((c,i)=>`<col min="${i+1}" max="${i+1}" width="${c.width||(i===0?18:13)}" customWidth="1"/>`).join('');
  return '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
   +`<sheetViews><sheetView workbookViewId="0"${selected?' tabSelected="1"':''}><pane xSplit="1" ySplit="${HEAD}" topLeftCell="B${FIRST}" activePane="bottomRight" state="frozen"/></sheetView></sheetViews>`
   +`<cols>${widths}</cols><sheetData>${rows.join('')}</sheetData><autoFilter ref="A${HEAD}:${lastC}${last}"/></worksheet>`;
}
// Summary sheet: one block per data sheet, linked to that sheet's filtered statistics.
function xlsxSummarySheet(sheets,meta){
  const rows=[];let r=1;
  const put=cells=>{rows.push(xrow(r,cells.map(([col,v,s])=>xcell(col+r,v,s))));r++};
  put([['A',meta.title||'Energy report',XS.title]]);
  for(const line of meta.lines||[])put([['A',line,XS.note]]);
  put([['A','Filter any sheet with the arrows in its header row (date, hour, lights, fan, mode or any value). Averages, minimum, maximum and totals below and at the top of each sheet recalculate for the rows that are shown. Empty cells mean no meter reading, never zero.',XS.note]]);
  for(const sheet of sheets){
    r++;put([['A',sheet.name+' — '+sheet.title,XS.label],['B','',XS.label],['C','',XS.label],['D','',XS.label],['E','',XS.label]]);
    put([['A','Measure',XS.header],['B','Average',XS.header],['C','Minimum',XS.header],['D','Maximum',XS.header],['E','Total',XS.header]]);
    const ref=(col,row)=>({f:`'${sheet.name}'!${col}${row}`});
    sheet.columns.forEach((c,i)=>{if(i===0||(!c.stats&&!c.total))return;const L=xcol(i),s=XS[c.fmt]??0;
      put([['A',c.h,0],['B',c.stats?ref(L,2):'',s],['C',c.stats?ref(L,3):'',s],['D',c.stats?ref(L,4):'',s],['E',c.total?ref(L,5):'',s]]);});
    put([['A','Rows shown',0],['B',ref('B',6),XS['0']]]);
  }
  return '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
   +'<sheetViews><sheetView workbookViewId="0"/></sheetViews><cols><col min="1" max="1" width="34" customWidth="1"/><col min="2" max="5" width="14" customWidth="1"/></cols>'
   +`<sheetData>${rows.join('')}</sheetData></worksheet>`;
}
function xlsxWorkbook(sheets,meta={}){
  const names=['Summary',...sheets.map(s=>s.name)];
  const files=[
    {name:'[Content_Types].xml',data:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>'
      +names.map((_,i)=>`<Override PartName="/xl/worksheets/sheet${i+1}.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>`).join('')+'</Types>'},
    {name:'_rels/.rels',data:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>'},
    {name:'xl/workbook.xml',data:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><bookViews><workbookView activeTab="0"/></bookViews><sheets>'
      +names.map((n,i)=>`<sheet name="${xe(n)}" sheetId="${i+1}" r:id="rId${i+1}"/>`).join('')+'</sheets><definedNames>'
      +sheets.map((s,i)=>`<definedName name="_xlnm._FilterDatabase" localSheetId="${i+1}" hidden="1">'${xe(s.name)}'!$A$${HEAD}:$${xcol(s.columns.length-1)}$${lastRow(s)}</definedName>`).join('')
      +'</definedNames><calcPr calcId="191029" fullCalcOnLoad="1"/></workbook>'},
    {name:'xl/_rels/workbook.xml.rels',data:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
      +names.map((_,i)=>`<Relationship Id="rId${i+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet${i+1}.xml"/>`).join('')
      +`<Relationship Id="rId${names.length+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>`},
    {name:'xl/styles.xml',data:XSTYLES},
    {name:'xl/worksheets/sheet1.xml',data:xlsxSummarySheet(sheets,meta)},
    ...sheets.map((s,i)=>({name:`xl/worksheets/sheet${i+2}.xml`,data:xlsxDataSheet(s,false)}))
  ];
  return xlsxZip(files);
}
// ---- Energy report layout from the device CSV formats ----
function parseCsv(text){
  const rows=[];let row=[],f='',q=false;text=String(text||'');
  for(let i=0;i<text.length;i++){const c=text[i];
    if(q){if(c==='"'){if(text[i+1]==='"'){f+='"';i++}else q=false}else f+=c}
    else if(c==='"')q=true;else if(c===','){row.push(f);f=''}else if(c==='\n'){row.push(f);rows.push(row);row=[];f=''}else if(c!=='\r')f+=c}
  if(f!==''||row.length){row.push(f);rows.push(row)}
  return rows;
}
function csvObjects(text){const [head,...rows]=parseCsv(text);return head?rows.filter(r=>r.length>1).map(r=>Object.fromEntries(head.map((k,i)=>[k,r[i]??'']))):[]}
function excelTime(s){const m=/^(\d{4})-(\d{2})-(\d{2})(?:T(\d{2}):(\d{2})(?::(\d{2}))?)?/.exec(s||'');return m?Date.UTC(+m[1],m[2]-1,+m[3],+(m[4]||0),+(m[5]||0),+(m[6]||0))/86400000+25569:null}
const xn=v=>v===''||v===null||v===undefined||!Number.isFinite(Number(v))?null:Number(v);
const xday=t=>t===null?null:Math.floor(t);
const xhour=s=>{const m=/T(\d{2})/.exec(s||'');return m?Number(m[1]):null};
function energyWorkbook(minuteCsv,hourlyCsv,dailyCsv,meta={}){
  const C=(h,fmt,o={})=>({h,fmt,...o});
  const daily={name:'Daily',title:'Daily totals',columns:[C('Date','date',{width:13}),C('Energy (kWh)','0.000',{stats:1,total:1}),C('Average power (W)','0.0',{stats:1}),C('Peak power (W)','0.0',{stats:1}),C('Peak time','text'),C('Occupied (h)','0.0',{stats:1,total:1}),C('Lights on (h)','0.0',{stats:1,total:1}),C('Fan on (h)','0.0',{stats:1,total:1}),C('Minutes recorded','0',{total:1}),C('Est. cost','0.00',{stats:1,total:1})],
    rows:csvObjects(dailyCsv).map(r=>[excelTime(r.date),xn(r.energy_kWh),xn(r.avg_W),xn(r.peak_W),r.peak_time,xn(r.occupied_h),xn(r.lights_on_h),xn(r.fan_on_h),xn(r.minutes_recorded),xn(r.est_cost)])};
  const hourly={name:'Hourly',title:'Hourly totals',columns:[C('Hour start','datetime'),C('Date','date',{width:12}),C('Hour','0',{width:7}),C('Energy (kWh)','0.000',{stats:1,total:1}),C('Average power (W)','0.0',{stats:1}),C('Peak power (W)','0.0',{stats:1}),C('Lowest power (W)','0.0',{stats:1}),C('Average voltage (V)','0.0',{stats:1}),C('Average power factor','0.00',{stats:1}),C('Occupied (min)','0',{stats:1,total:1}),C('Lights on (min)','0',{stats:1,total:1}),C('Fan on (min)','0',{stats:1,total:1}),C('Minutes recorded','0',{total:1})],
    rows:csvObjects(hourlyCsv).map(r=>{const t=excelTime(r.hour_start);return [t,xday(t),xhour(r.hour_start),xn(r.energy_kWh),xn(r.avg_W),xn(r.peak_W),xn(r.min_W),xn(r.avg_V),xn(r.avg_pf),xn(r.occupied_min),xn(r.lights_on_min),xn(r.fan_on_min),xn(r.minutes_recorded)]})};
  const minute={name:'Minute',title:'1-minute readings',columns:[C('Time','datetime'),C('Date','date',{width:12}),C('Hour','0',{width:7}),C('Voltage (V)','0.0',{stats:1}),C('Current (A)','0.000',{stats:1}),C('Power avg (W)','0.0',{stats:1}),C('Power peak (W)','0.0',{stats:1}),C('Power factor','0.00',{stats:1}),C('Frequency (Hz)','0.0',{stats:1}),C('Energy (Wh)','0.000',{stats:1,total:1}),C('Meter total (Wh)','0',{width:15}),C('Lights','text',{width:9}),C('Fan','text',{width:9}),C('Mode','text',{width:10}),C('Occupied','0',{total:1,width:10}),C('Samples','0',{width:9}),C('Meter OK','0',{width:10})],
    rows:csvObjects(minuteCsv).map(r=>{const t=excelTime(r.timestamp);return [t,xday(t),xhour(r.timestamp),xn(r.voltage_V),xn(r.current_A),xn(r.power_avg_W),xn(r.power_peak_W),xn(r.pf),xn(r.frequency_Hz),xn(r.energy_Wh),xn(r.meter_total_Wh),r.lights,r.fan,r.mode,xn(r.occupied),xn(r.samples),xn(r.meter_ok)]})};
  return xlsxWorkbook([daily,hourly,minute],meta);
}
export {xlsxWorkbook,energyWorkbook,parseCsv,csvObjects,excelTime};
