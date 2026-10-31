'use strict';
const CACHE='scorecard-report-v1';
const ASSETS=['./','./index.html','./manifest.webmanifest'];
self.addEventListener('install',event=>{event.waitUntil(caches.open(CACHE).then(cache=>cache.addAll(ASSETS)).then(()=>self.skipWaiting()));});
self.addEventListener('activate',event=>{event.waitUntil(caches.keys().then(keys=>Promise.all(keys.filter(key=>key.startsWith('scorecard-report-')&&key!==CACHE).map(key=>caches.delete(key)))).then(()=>self.clients.claim()));});
self.addEventListener('fetch',event=>{
  const url=new URL(event.request.url);
  if(event.request.method!=='GET'||url.origin!==self.location.origin)return;
  const root=new URL('./',self.location.href);
  if(!ASSETS.some(path=>new URL(path,root).pathname===url.pathname))return;
  // Build the cache key from pathname only: fragments never enter requests or cache keys.
  event.respondWith(caches.match(url.origin+url.pathname).then(cached=>cached||fetch(url.origin+url.pathname)));
});
