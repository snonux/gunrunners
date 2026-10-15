// Runs before main: the saves directory is IndexedDB-backed, loaded first
// and written back every few seconds and whenever the tab is hidden.
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  FS.mkdir('/saves');
  FS.mount(IDBFS, {}, '/saves');
  addRunDependency('saves');
  FS.syncfs(true, function () { removeRunDependency('saves'); });
  var busy = false;
  function flush() {
    if (busy) return;
    busy = true;
    FS.syncfs(false, function () { busy = false; });
  }
  setInterval(flush, 5000);
  document.addEventListener('visibilitychange', function () {
    if (document.visibilityState === 'hidden') flush();
  });
  window.addEventListener('pagehide', flush);
});
