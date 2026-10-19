// Included inside Emscripten's runtime so FS and callMain need no public JS API.
const playButton = document.getElementById('play');
const statusLine = document.getElementById('status');
const storageLine = document.getElementById('storage');
const canvas = Module['canvas'];
const stage = document.getElementById('stage');
let started = false;

Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(() => {
  // The native XDG path resolver will use /saves/proteus. Restore before main reads it.
  ENV['XDG_DATA_HOME'] = '/saves';
  FS.mkdir('/saves');
  FS.mount(IDBFS, { autoPersist: true }, '/saves');
  addRunDependency('saved-games');
  FS.syncfs(true, error => {
    if (error) {
      console.warn('Saved games unavailable:', error);
      storageLine.textContent = 'Browser storage is unavailable. Best times may be lost on reload.';
    }
    removeRunDependency('saved-games');
  });
});

Module['onRuntimeInitialized'] = () => {
  playButton.disabled = false;
  playButton.textContent = 'Play';
  statusLine.textContent = 'Ready. Press Play to start with sound.';
};

function fitCanvas() {
  const availableHeight = document.fullscreenElement ? innerHeight : Math.max(200,
    innerHeight - document.querySelector('header').offsetHeight - document.querySelector('footer').offsetHeight);
  const scale = Math.max(1, Math.floor(Math.min(innerWidth / 320, availableHeight / 200)));
  // The fixed 960x600 buffer contains a 320x200 scene. Use whole scene-pixel scales.
  const width = 320 * scale;
  const height = 200 * scale;
  canvas.style.width = width + 'px';
  canvas.style.height = height + 'px';
}

playButton.addEventListener('click', () => {
  if (started) return;
  started = true;
  playButton.disabled = true;
  playButton.textContent = 'Playing';
  const location = document.getElementById('location').value;
  const args = location === 'course' ? ['--course'] : ['--location', location];
  if (document.getElementById('waves').checked) args.push('--waves');
  if (document.getElementById('aa').checked) args.push('--gpu-aa');
  for (const id of ['location', 'waves', 'aa']) document.getElementById(id).disabled = true;
  statusLine.textContent = 'Click the game to focus. Use ? for controls.';
  document.getElementById('fullscreen').disabled = !stage.requestFullscreen;
  canvas.focus();
  // Audio initialization and PlayMusicStream happen inside this user gesture.
  callMain(args);
  fitCanvas();
});

canvas.addEventListener('click', () => canvas.focus());
canvas.addEventListener('contextmenu', event => event.preventDefault());
canvas.addEventListener('keydown', event => {
  if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', ' ', 'PageUp', 'PageDown', 'Home', 'End', 'F3'].includes(event.key))
    event.preventDefault();
});
document.getElementById('fullscreen').addEventListener('click', () => {
  stage.requestFullscreen().then(() => canvas.focus()).catch(error => {
    statusLine.textContent = 'Fullscreen unavailable: ' + error.message;
  });
});
window.addEventListener('resize', fitCanvas);
document.addEventListener('fullscreenchange', fitCanvas);
Module['onExit'] = code => {
  statusLine.textContent = code ? 'The game could not start. Check WebGL 2 support and reload.'
    : 'Game closed. Press Restart to play again.';
};
fitCanvas();
