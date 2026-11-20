// CSV builders for the 1-minute / hourly / daily history the ESP32 (firmware 1.3.0+) uploads.
// Database layout: history/<device>/minutes/<YYYY-MM-DD>/<HHMM>, hours/<YYYY-MM-DD>/<HH>, days/<YYYY-MM-DD>
export const MINUTE_HEADER='timestamp,device_id,voltage_V,current_A,power_avg_W,power_peak_W,pf,frequency_Hz,energy_Wh,meter_total_Wh,lights,fan,mode,occupied,samples,meter_ok';
export const HOURLY_HEADER='hour_start,energy_kWh,avg_W,peak_W,min_W,avg_V,avg_pf,occupied_min,lights_on_min,fan_on_min,minutes_recorded';
export const DAILY_HEADER='date,energy_kWh,avg_W,peak_W,peak_time,occupied_h,lights_on_h,fan_on_h,minutes_recorded,est_cost';

const num=(v,d)=>v===null||v===undefined||v===''||!Number.isFinite(Number(v))?'':Number(v).toFixed(d);
const int=v=>Number.isFinite(Number(v))?String(Math.round(Number(v))):'0';
export function csvText(value){const s=String(value??'');return /[",\n\r]/.test(s)?'"'+s.replace(/"/g,'""')+'"':s}
export function tzSuffix(minutes){const m=Number.isFinite(Number(minutes))?Number(minutes):0,a=Math.abs(m);return (m<0?'-':'+')+String(Math.floor(a/60)).padStart(2,'0')+':'+String(a%60).padStart(2,'0')}
const sorted=obj=>Object.keys(obj||{}).sort();
const finish=lines=>lines.join('\n')+'\n';

export function minuteCsv(date,rows,deviceName){
  const lines=[MINUTE_HEADER],device=csvText(deviceName);
  for(const key of sorted(rows)){
    if(!/^\d{4}$/.test(key))continue;
    const r=rows[key]||{},ok=r.ok===1||r.ok===true;
    const time=`${date}T${key.slice(0,2)}:${key.slice(2)}:00${tzSuffix(r.z)}`;
    const electrical=ok?[num(r.V,1),num(r.A,3),num(r.W,1),num(r.Wp,1),num(r.pf,2),num(r.Hz,1),num(r.Wh,3),int(r.mWh)]:['','','','','','','',''];
    lines.push([time,device,...electrical,r.l?'ON':'OFF',r.f?'ON':'OFF',csvText(r.m||'AUTO'),r.o?'1':'0',ok?int(r.n):'0',ok?'1':'0'].join(','));
  }
  return finish(lines);
}
export function hourlyCsv(hoursByDate){
  const lines=[HOURLY_HEADER];
  for(const date of sorted(hoursByDate))for(const hh of sorted(hoursByDate[date])){
    if(!/^\d{2}$/.test(hh))continue;
    const h=hoursByDate[date][hh]||{},has=Number(h.n)>0;
    lines.push([`${date}T${hh}:00`,num(h.kWh,3),has?num(h.avgW,1):'',has?num(h.peakW,1):'',has?num(h.minW,1):'',has?num(h.avgV,1):'',has?num(h.avgPf,2):'',int(h.occ),int(h.lights),int(h.fan),int(h.n)].join(','));
  }
  return finish(lines);
}
export function dailyCsv(days){
  const lines=[DAILY_HEADER];
  for(const date of sorted(days)){
    const d=days[date]||{},has=Number(d.n)>0;
    lines.push([date,num(d.kWh,3),has?num(d.avgW,1):'',has?num(d.peakW,1):'',has?csvText(d.peakT||''):'',num(d.occH,1),num(d.lightsH,1),num(d.fanH,1),int(d.n),num(d.cost,2)].join(','));
  }
  return finish(lines);
}
export function shiftDate(date,days){const t=new Date(date+'T00:00:00Z');t.setUTCDate(t.getUTCDate()+days);return t.toISOString().slice(0,10)}
