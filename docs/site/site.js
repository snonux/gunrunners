// Gunrunners web page: the turnable game box, the gameplay clips that play
// while they are on screen, a lightbox for the screenshots and little
// synthesized sound effects. No dependencies. Without JavaScript the page
// still reads fine: the box stands at its resting angle, its turn controls
// and the sound switch stay hidden, the clips have their own controls and
// the screenshots link to the full-size files.
(function () {
  "use strict";

  // Read live (the visitor can switch it while the page is open).
  var reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  function reduceMotion() { return reducedMotion.matches; }
  var TURN_MS = 950; // a little over the .box.animate transition in style.css

  // ---------- the box ----------
  // The box is turned by setting --rx/--ry on .box-stage (style.css reads
  // them). `rest` is where the box was left (dragged to, flipped to or turned
  // with the keys); the idle spin carries on from there.
  function createBox(stage) {
    var state = {
      rest: { rx: -10, ry: -24 }, // the three-quarter view of the front, from a little above
      rx: -10, ry: -24,
      dragging: false,
      turning: false,
      turnTimer: 0,
      swayStart: performance.now(),
      nodFrom: 0, // tilt left over from before a pause, fading out (startSway)
      written: {}, // what is in the DOM, so unchanged values aren't rewritten
    };

    function write(key, value, set) {
      if (state.written[key] === value) return;
      state.written[key] = value;
      set(value);
    }

    // Push the current angles to the page.
    function apply() {
      write("rx", state.rx.toFixed(2) + "deg", function (v) { stage.style.setProperty("--rx", v); });
      write("ry", state.ry.toFixed(2) + "deg", function (v) { stage.style.setProperty("--ry", v); });
      // One full sweep of the sheen per half turn, so it lines up after a flip.
      var phase = ((state.ry % 180) + 180) % 180;
      write("sheen", ((phase / 180) * 100).toFixed(1) + "%", function (v) { stage.style.setProperty("--sheen", v); });
    }

    return { state: state, apply: apply };
  }

  // Turning with the CSS transition (.box.animate in style.css).
  function addTurns(box, b) {
    var s = b.state;
    // Turn smoothly to the resting angle. A turn started during another one
    // restarts the timer, so the transition isn't cut short.
    b.turnToRest = function () {
      window.clearTimeout(s.turnTimer);
      box.classList.add("animate");
      s.turning = true;
      s.nodFrom = 0; // the turn ends exactly at the rest
      s.rx = s.rest.rx;
      s.ry = s.rest.ry;
      b.apply();
      s.turnTimer = window.setTimeout(function () {
        box.classList.remove("animate");
        s.turning = false;
        s.swayStart = performance.now();
      }, reduceMotion() ? 0 : TURN_MS);
    };
    // Cut a running turn short for a drag: without the transition the box
    // jumps to where the turn was heading, and the drag starts from there.
    b.stopTurn = function () {
      window.clearTimeout(s.turnTimer);
      box.classList.remove("animate");
      s.turning = false;
    };
  }

  // Drag with the mouse (main button), a finger or a pen to turn the box.
  // The browser's own image drag would steal the pointer (pointercancel),
  // so it is blocked here as well as with draggable="false" on the covers.
  function setupDrag(stage, b) {
    var s = b.state, drag = null;
    stage.addEventListener("dragstart", function (e) { e.preventDefault(); });
    stage.addEventListener("pointerdown", function (e) {
      if (e.pointerType === "mouse" && e.button !== 0) return;
      b.stopTurn();
      drag = { x: e.clientX, y: e.clientY, rx: s.rx, ry: s.ry, id: e.pointerId };
      s.dragging = true;
      stage.setPointerCapture(e.pointerId);
      stage.classList.add("dragging");
    });
    stage.addEventListener("pointermove", function (e) {
      if (!drag || e.pointerId !== drag.id) return;
      s.ry = drag.ry + (e.clientX - drag.x) * 0.5;
      s.rx = Math.max(-35, Math.min(35, drag.rx - (e.clientY - drag.y) * 0.3));
      b.apply();
    });
    function endDrag(e) {
      if (!drag || e.pointerId !== drag.id) return;
      drag = null;
      s.dragging = false;
      stage.classList.remove("dragging");
      s.rest = { rx: s.rx, ry: s.ry };
      s.nodFrom = 0; // the rest already holds the tilt
      s.swayStart = performance.now();
    }
    stage.addEventListener("pointerup", endDrag);
    stage.addEventListener("pointercancel", endDrag);
  }

  // The button flips the box half a turn on from wherever the spin has got
  // to; the arrow keys turn it a step from there (left/right) or tilt its
  // resting nod (up/down). Then the spin carries on from the new angle.
  // Arrows with a modifier are left to the browser (Alt+Left is "back").
  function setupControls(stage, flip, b) {
    var s = b.state;
    if (flip) {
      // From wherever the spin has got to, half a turn on.
      flip.addEventListener("click", function () {
        s.rest = { rx: s.rest.rx, ry: s.ry + 180 };
        b.turnToRest();
      });
    }
    stage.tabIndex = 0;
    stage.addEventListener("keydown", function (e) {
      if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) return;
      var step = { ArrowLeft: [0, -15], ArrowRight: [0, 15], ArrowUp: [5, 0], ArrowDown: [-5, 0] }[e.key];
      if (!step) return;
      e.preventDefault();
      s.rest = { rx: Math.max(-35, Math.min(35, s.rest.rx + step[0])), ry: s.ry + step[1] };
      b.turnToRest();
    });
  }

  // The idle spin: the box keeps turning all the way round (SPIN degrees a
  // second about its upright axis) while nodding up and down a little,
  // starting from wherever it was left (dragged, flipped or turned with the
  // keys) and easing in over the first two seconds. It runs only while the
  // box is on screen (it repaints the box and its SVG filters) and stops
  // whenever reduced motion is switched on. After any pause (off screen, a
  // background tab) it starts again from the angle it stopped at.
  var SPIN = 24;
  function startSway(stage, b) {
    var s = b.state, visible = false, frame = 0, last = 0;
    // Degrees turned t seconds after the spin (re)started: the speed ramps
    // up linearly over 2 s, then stays at SPIN.
    function turned(t) { return t < 2 ? SPIN * t * t / 4 : SPIN * (t - 1); }
    function sway(now) {
      frame = 0;
      if (!visible || reduceMotion()) return;
      if (now - last > 250) { // a pause: carry on from here, tilt and all
        s.nodFrom = s.rx - s.rest.rx;
        s.rest = { rx: s.rest.rx, ry: s.ry };
        s.swayStart = now;
      }
      last = now;
      if (!s.dragging && !s.turning) {
        var t = (now - s.swayStart) / 1000;
        s.ry = s.rest.ry + turned(t);
        // The nod fades in over 2 s, and the tilt the box had when it
        // paused (nodFrom) fades out, so it never jumps.
        var ease = Math.min(1, t / 2);
        s.rx = s.rest.rx + (s.nodFrom || 0) * (1 - ease) + Math.sin(t * 0.5) * 4 * ease;
        b.apply();
      }
      frame = window.requestAnimationFrame(sway);
    }
    function resume() { if (visible && !frame && !reduceMotion()) frame = window.requestAnimationFrame(sway); }
    if (!("IntersectionObserver" in window)) return;
    new IntersectionObserver(function (entries) {
      visible = entries[entries.length - 1].isIntersecting;
      resume();
    }).observe(stage);
    // Old Safari (13 and before) only has addListener on media queries.
    if (reducedMotion.addEventListener) reducedMotion.addEventListener("change", resume);
    else if (reducedMotion.addListener) reducedMotion.addListener(resume);
  }

  function setupBox() {
    var stage = document.querySelector(".box-stage");
    var box = document.querySelector(".box");
    var flip = document.getElementById("flip");
    var controls = document.querySelector(".box-controls");
    if (!stage || !box) return;
    var b = createBox(stage);
    addTurns(box, b);
    setupDrag(stage, b);
    setupControls(stage, flip, b);
    if (controls) controls.hidden = false;
    stage.classList.add("turnable");
    stage.setAttribute("aria-label", "The Gunrunners game box. Drag it, or use the arrow keys, to turn it.");
    b.apply();
    startSway(stage, b);
  }

  // ---------- the clips ----------
  // Muted clips play while at least half of them is on screen, so a long
  // page doesn't decode six videos at once. The ratio is checked by hand:
  // isIntersecting alone is true for a single visible pixel. A clip someone paused stays
  // paused; dataset.auto marks the pauses made here.
  function setupVideos() {
    var videos = document.querySelectorAll(".video video");
    if (!("IntersectionObserver" in window) || reduceMotion()) return;
    var userPaused = new WeakSet();
    videos.forEach(function (v) {
      v.addEventListener("pause", function () { if (v.dataset.auto !== "1") userPaused.add(v); v.dataset.auto = ""; });
      v.addEventListener("play", function () { userPaused.delete(v); });
    });
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) {
        var v = en.target;
        var onScreen = en.intersectionRatio >= 0.5;
        if (onScreen && !userPaused.has(v)) {
          v.preload = "auto";
          var p = v.play();
          if (p && p.catch) p.catch(function () {});
        } else if (!onScreen && !v.paused) {
          v.dataset.auto = "1";
          v.pause();
        }
      });
    }, { threshold: [0, 0.5] });
    videos.forEach(function (v) { io.observe(v); });
  }

  // ---------- the screenshots ----------
  function setupLightbox() {
    if (typeof HTMLDialogElement !== "function") return;
    var dialog = document.createElement("dialog");
    dialog.className = "lightbox";
    dialog.setAttribute("aria-label", "Screenshot");
    dialog.innerHTML = '<img alt=""><button type="button" class="btn btn-small">Close</button>';
    document.body.appendChild(dialog);
    var img = dialog.querySelector("img");
    dialog.querySelector("button").addEventListener("click", function () { dialog.close(); });
    dialog.addEventListener("click", function (e) { if (e.target === dialog || e.target === img) dialog.close(); });
    document.querySelectorAll(".shot-grid a").forEach(function (a) {
      a.addEventListener("click", function (e) {
        e.preventDefault();
        var thumb = a.querySelector("img");
        img.removeAttribute("src"); // don't show the last picture while this one loads
        img.src = a.getAttribute("href");
        img.alt = thumb ? thumb.alt : "";
        dialog.setAttribute("aria-label", img.alt || "Screenshot");
        dialog.showModal();
      });
    });
  }

  // ---------- sound effects ----------
  // Little blips synthesized on the spot with Web Audio, like the game's own
  // sounds (no samples): moving parts and cards make a noise when the mouse
  // comes over them, buttons when clicked. Browsers allow sound only after a
  // click or a key press, so the first one starts the audio. The speaker in
  // the header switches the effects off and on, remembered in this browser.
  // Each voice: oscillator type (or noise through a band-pass sweep), the
  // frequency sweep f0 -> f1 in Hz, length t and start offset at in seconds,
  // peak gain g.
  var SFX = {
    blip: [{ type: "square", f0: 880, f1: 1320, t: 0.06, g: 0.04 }],
    tick: [{ type: "triangle", f0: 2600, f1: 1900, t: 0.03, g: 0.05 }],
    boing: [{ type: "square", f0: 180, f1: 900, t: 0.18, g: 0.035 }],
    zap: [{ type: "sawtooth", f0: 1500, f1: 80, t: 0.24, g: 0.035 }],
    whoosh: [{ noise: true, f0: 250, f1: 3200, t: 0.4, g: 0.16 }],
    glitch: [{ noise: true, f0: 5000, f1: 500, t: 0.16, g: 0.1 }, { type: "square", f0: 70, f1: 45, t: 0.16, g: 0.03 }],
    coin: [{ type: "square", f0: 988, f1: 988, t: 0.07, g: 0.03 }, { type: "square", f0: 1319, f1: 1319, t: 0.22, g: 0.03, at: 0.07 }],
  };
  var SOUND_KEY = "gunrunners-sfx"; // localStorage: "off" when switched off

  // One voice of an effect (see SFX), starting `now` on the context's clock:
  // an oscillator, or the white `noise` buffer through a band-pass filter,
  // sweeping from f0 to f1 under a quickly falling volume.
  function playVoice(ctx, noise, v, now) {
    var start = now + (v.at || 0), end = start + v.t, src, sweep;
    var gain = ctx.createGain();
    gain.gain.setValueAtTime(v.g, start);
    gain.gain.exponentialRampToValueAtTime(0.0001, end);
    gain.connect(ctx.destination);
    if (noise) {
      src = ctx.createBufferSource();
      src.buffer = noise;
      sweep = ctx.createBiquadFilter();
      sweep.type = "bandpass";
      sweep.Q.value = 1.5;
      sweep.frequency.setValueAtTime(v.f0, start);
      sweep.frequency.exponentialRampToValueAtTime(v.f1, end);
      src.connect(sweep); // two calls: old WebKit's connect() returns nothing to chain on
      sweep.connect(gain);
    } else {
      src = ctx.createOscillator();
      src.type = v.type;
      src.frequency.setValueAtTime(v.f0, start);
      src.frequency.exponentialRampToValueAtTime(v.f1, end);
      src.connect(gain);
    }
    src.start(start);
    src.stop(end + 0.02);
  }

  // The audio engine: unlock() on a user gesture, play(name) afterwards.
  function createSfx() {
    var ctx = null, noise = null, on = true;
    try { on = window.localStorage.getItem(SOUND_KEY) !== "off"; } catch (e) { /* storage blocked: default on */ }

    // Called on every click, tap and key press (the only moments a browser
    // lets audio start). No audio device is opened while the sound is off;
    // a context the browser suspended or interrupted (iOS) is resumed.
    // Returns a promise that settles once the audio runs (or right away).
    function unlock() {
      if (!on) return Promise.resolve();
      var AC = window.AudioContext || window.webkitAudioContext;
      if (!ctx && AC) ctx = new AC();
      if (ctx && ctx.state !== "running") return Promise.resolve(ctx.resume()).catch(function () {});
      return Promise.resolve();
    }
    function noiseBuffer() {
      if (noise) return noise;
      noise = ctx.createBuffer(1, Math.floor(ctx.sampleRate / 2), ctx.sampleRate);
      var d = noise.getChannelData(0);
      for (var i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
      return noise;
    }
    // True if the sound actually played.
    function play(name) {
      if (!on || !ctx || ctx.state !== "running") return false;
      SFX[name].forEach(function (v) { playVoice(ctx, v.noise ? noiseBuffer() : null, v, ctx.currentTime); });
      return true;
    }
    // Switched on from a click, so the audio may start right away (the
    // promise settles once it runs); switched off, the audio device is let go.
    function setOn(value) {
      on = value;
      try { window.localStorage.setItem(SOUND_KEY, on ? "on" : "off"); } catch (e) { /* not remembered */ }
      if (on) return unlock();
      if (ctx && ctx.state === "running") ctx.suspend();
      return Promise.resolve();
    }
    return { unlock: unlock, play: play, isOn: function () { return on; }, setOn: setOn };
  }

  // Which sound goes with which part of the page: on mouse-over (not touch:
  // a tap would also be a click) and on click or press. One hover sound and
  // one click sound per element at most every 250 ms (counted from a sound
  // that actually played), so sweeping across the page doesn't turn into
  // noise and a quick click after a hover still gets its own sound.
  var HOVER_SOUNDS = [
    [".box-stage", "whoosh"], [".runner", "boing"], [".boss", "zap"], [".logo", "glitch"],
    [".filmstrip", "tick"], [".episode, .video, .get-card, .cast-card, .shot-grid a", "blip"],
    [".btn, .top nav a, .sound-toggle", "tick"],
  ];
  var CLICK_SOUNDS = [["#flip", "whoosh"], [".btn:not(#flip)", "coin"]];

  function bindSounds(sfx) {
    var hovered = new WeakMap(), clicked = new WeakMap();
    function sound(times, el, name) {
      var now = performance.now();
      if (now - (times.get(el) || 0) < 250) return;
      if (sfx.play(name)) times.set(el, now);
    }
    HOVER_SOUNDS.forEach(function (pair) {
      document.querySelectorAll(pair[0]).forEach(function (el) {
        el.addEventListener("pointerenter", function (e) { if (e.pointerType !== "touch") sound(hovered, el, pair[1]); });
      });
    });
    CLICK_SOUNDS.forEach(function (pair) {
      document.querySelectorAll(pair[0]).forEach(function (el) {
        el.addEventListener("click", function () { sound(clicked, el, pair[1]); });
      });
    });
    var stage = document.querySelector(".box-stage");
    if (stage) stage.addEventListener("pointerdown", function () { sound(clicked, stage, "whoosh"); });
  }

  function setupSound() {
    var button = document.querySelector(".sound-toggle");
    var sfx = createSfx();
    // pointerup and click too: iOS starts audio only on the end of a touch.
    ["pointerdown", "pointerup", "click", "keydown"].forEach(function (type) {
      document.addEventListener(type, sfx.unlock, { capture: true, passive: true });
    });
    if (button) {
      button.hidden = false;
      button.setAttribute("aria-pressed", String(sfx.isOn()));
      button.addEventListener("click", function () {
        sfx.setOn(!sfx.isOn()).then(function () { sfx.play("coin"); }); // heard only when switched on
        button.setAttribute("aria-pressed", String(sfx.isOn()));
      });
    }
    bindSounds(sfx);
  }

  setupBox();
  setupVideos();
  setupLightbox();
  setupSound();
})();
