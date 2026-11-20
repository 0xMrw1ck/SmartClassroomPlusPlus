// Copies cloud/public/xlsx.js (minus its ES export line) into the ESP32 dashboard between the XLSX markers.
// Run after editing xlsx.js:  node tools/sync-xlsx.cjs
const fs=require('node:fs'),path=require('node:path');
const root=path.join(__dirname,'..');
const lib=fs.readFileSync(path.join(root,'cloud/public/xlsx.js'),'utf8').replace(/\r\n/g,'\n').replace(/^export \{[^}]*\};\n?/m,'').trimEnd();
const file=path.join(root,'SmartClassroomS3OLED/DashboardAssets.h');
const src=fs.readFileSync(file,'utf8'),eol=src.includes('\r\n')?'\r\n':'\n';
const begin='// XLSX-BEGIN (generated copy of cloud/public/xlsx.js without its export line)'+eol,end='// XLSX-END';
const a=src.indexOf(begin),b=src.indexOf(end);
if(a<0||b<a)throw Error('XLSX markers not found in DashboardAssets.h');
if(lib.includes(')ASSET"'))throw Error('xlsx.js must not contain the raw-string terminator');
fs.writeFileSync(file,src.slice(0,a+begin.length)+lib.split('\n').join(eol)+eol+src.slice(b));
console.log('synced',lib.length,'chars into DashboardAssets.h');
