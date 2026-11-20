export const STALE_MS=90000;
export function readingState(record,now=Date.now()) {
  const stamp=Number(record?.updatedAt),age=now-stamp;
  const fresh=Number.isFinite(stamp)&&stamp>0&&age>=-5000&&age<=STALE_MS;
  const telemetry=record?.telemetry||{};
  return {fresh,stamp,telemetry,power:fresh&&telemetry.meterOnline&&Number.isFinite(telemetry.power)?telemetry.power:null,
    presence:fresh&&telemetry.radarOnline?Number(telemetry.personCount)||0:null};
}
export function number(value,digits=1) {return Number.isFinite(value)?value.toFixed(digits):'—'}
export function controlState(record,now=Date.now()) {
  const state=readingState(record,now);
  if(!state.fresh)return {enabled:false,reason:'Device offline. Commands are disabled.'};
  if(state.telemetry.remoteControl!==true)return {enabled:false,reason:'Install firmware 1.2.0 or newer to enable online controls.'};
  if(state.telemetry.standby)return {enabled:false,reason:'Device is in standby. Resume using the physical button or local dashboard.'};
  return {enabled:true,reason:'AUTO follows radar timers. Manual ON/OFF overrides automation until AUTO is selected.'};
}
