import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {readingState,number,controlState} from '../public/state.js';

test('restored Google session opens dashboard and shows empty-device setup',async()=>{
  const nodes=new Map();let observer,redirects=0;
  const element=()=>({hidden:false,value:'',textContent:'',children:[],classList:{toggle(){}},replaceChildren(){this.children=[]},append(n){this.children.push(n)},addEventListener(type,fn){this[type]=fn}});
  const context=vm.createContext({document:{getElementById(id){if(!nodes.has(id))nodes.set(id,element());return nodes.get(id)},createElement:element,querySelectorAll:()=>[]},
    readingState,number,controlState,initializeApp:c=>c,getAuth:()=>({}),getDatabase:()=>({}),GoogleAuthProvider:class {},
    signInWithRedirect:async()=>{redirects++},getRedirectResult:async()=>null,signOut:async()=>{},
    onAuthStateChanged:(a,fn)=>{observer=fn;fn(null)},ref:(d,p)=>p,query:(r,...args)=>({path:r,args}),orderByChild:x=>x,equalTo:x=>x,set:async()=>{},
    onValue:(r,fn)=>{fn({val:()=>r==='.info/serverTimeOffset'?0:null});return ()=>{}},
    fetch:async()=>({ok:true,json:async()=>({apiKey:'public',databaseURL:'https://example.firebaseio.com'})}),setInterval:()=>0});
  const source=fs.readFileSync(new URL('../public/app.js',import.meta.url),'utf8').replace(/^import .*;\r?\n/gm,'');
  vm.runInContext(source,context);await new Promise(resolve=>setImmediate(resolve));
  assert.equal(nodes.get('dashboard').hidden,true);
  await nodes.get('signIn').click();assert.equal(redirects,1);
  observer({uid:'viewer'});
  assert.equal(nodes.get('login').hidden,true);assert.equal(nodes.get('dashboard').hidden,false);assert.equal(nodes.get('pairing').hidden,false);
  assert.match(nodes.get('message').textContent,/No classroom has published/);assert.equal(nodes.get('power').textContent,'—');
  observer(null);assert.equal(nodes.get('dashboard').hidden,true);assert.equal(nodes.get('pairing').hidden,true);
});

test('production authentication uses the hosting domain',()=>{
  const c=JSON.parse(fs.readFileSync(new URL('../public/config.json',import.meta.url)));
  const deployment=JSON.parse(fs.readFileSync(new URL('../firebase.json',import.meta.url)));
  assert.equal(c.authDomain,deployment.hosting.site+'.web.app');
});
