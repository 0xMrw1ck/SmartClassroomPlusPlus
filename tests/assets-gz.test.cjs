// The ESP32 serves DashboardAssetsGz.h; it must decompress to exactly the editable DashboardAssets.h text.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),zlib=require('node:zlib');
const root=path.join(__dirname,'..');
const src=fs.readFileSync(path.join(root,'SmartClassroomS3OLED/DashboardAssets.h'),'utf8').replace(/\r\n/g,'\n');
const gz=fs.readFileSync(path.join(root,'SmartClassroomS3OLED/DashboardAssetsGz.h'),'utf8');
for(const name of ['DASHBOARD_HTML','DASHBOARD_CSS','DASHBOARD_JS','DASHBOARD_SW']){
  const raw=src.match(new RegExp(name+'\\[\\] PROGMEM = R"ASSET\\(([\\s\\S]*?)\\)ASSET";'))[1];
  const m=gz.match(new RegExp(name+'_GZ\\[\\] PROGMEM = \\{([\\s\\S]*?)\\};'));
  assert.ok(m,name+'_GZ missing: run node tools/build-assets.cjs');
  const bytes=Buffer.from(m[1].split(',').map(s=>parseInt(s.trim(),16)));
  assert.equal(zlib.gunzipSync(bytes).toString('utf8'),raw,name+' is out of date: run node tools/build-assets.cjs');
}
console.log('PASS: gzip dashboard assets match DashboardAssets.h');
