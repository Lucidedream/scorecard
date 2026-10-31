'use strict';
const CACHE='scorecard-report-v2';
const ASSETS=['./','./index.html','./manifest.webmanifest'];
self.addEventListener('install',event=>{event.waitUntil(caches.open(CACHE).then(cache=>cache.addAll(ASSETS)).then(()=>self.skipWaiting()));});
self.addEventListener('activate',event=>{
  event.waitUntil((async()=>{
    const keys=await caches.keys();
    const upgrading=keys.some(key=>key.startsWith('scorecard-report-')&&key!==CACHE);
    await Promise.all(keys.filter(key=>key.startsWith('scorecard-report-')&&key!==CACHE).map(key=>caches.delete(key)));
    await self.clients.claim();
    // A cached format-1 page has no update listener. Replace it with this report,
    // preserving its fragment, only when upgrading an existing report cache.
    if(upgrading){
      const root=new URL('./',self.location.href);
      const clients=await self.clients.matchAll({type:'window'});
      // Navigation fetches wait for activation, so start them without awaiting them.
      for(const client of clients)if(new URL(client.url).pathname.startsWith(root.pathname))client.navigate(client.url).catch(()=>{});
    }
  })());
});
self.addEventListener('fetch',event=>{
  const url=new URL(event.request.url);
  if(event.request.method!=='GET'||url.origin!==self.location.origin)return;
  const root=new URL('./',self.location.href);
  if(!ASSETS.some(path=>new URL(path,root).pathname===url.pathname))return;
  // Build the cache key from pathname only: fragments never enter requests or cache keys.
  event.respondWith(caches.match(url.origin+url.pathname).then(cached=>cached||fetch(url.origin+url.pathname)));
});
