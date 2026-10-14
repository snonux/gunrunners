// Gunrunners web page: the turnable game box, the gameplay clips that play
// while they are on screen, one trailer at a time, and a lightbox for the
// screenshots. No dependencies. Without JavaScript the page still reads
// fine: the box stands at its resting angle, its turn controls stay hidden,
// the clips and trailers have their own controls and the screenshots link
// to the full-size files.
(function () {
  "use strict";

  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  var TURN_MS = 950; // a little over the .box.animate transition in style.css

  // ---------- the box ----------
  // The box is turned by setting --rx/--ry on .box-stage (style.css reads
  // them). `rest` is where the box was left (dragged to, flipped to or turned
  // with the keys); the idle sway only wobbles a few degrees around it.
  function createBox(stage, flip) {
    var state = {
      rest: { rx: -6, ry: -24 }, // the three-quarter view of the front
      rx: -6, ry: -24,
      dragging: false,
      turning: false,
      turnTimer: 0,
      swayStart: performance.now(),
      written: {}, // what is in the DOM, so unchanged values aren't rewritten
    };

    function write(key, value, set) {
      if (state.written[key] === value) return;
      state.written[key] = value;
      set(value);
    }

    function showingBack() {
      var a = ((state.ry % 360) + 360) % 360;
      return a > 90 && a < 270;
    }

    // Push the current angles to the page.
    function apply() {
      write("rx", state.rx.toFixed(2) + "deg", function (v) { stage.style.setProperty("--rx", v); });
      write("ry", state.ry.toFixed(2) + "deg", function (v) { stage.style.setProperty("--ry", v); });
      // One full sweep of the sheen per half turn, so it lines up after a flip.
      var phase = ((state.ry % 180) + 180) % 180;
      write("sheen", ((phase / 180) * 100).toFixed(1) + "%", function (v) { stage.style.setProperty("--sheen", v); });
      if (flip) {
        write("label", showingBack() ? "Turn the box to the front" : "Turn the box over",
          function (v) { flip.textContent = v; });
      }
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
      s.rx = s.rest.rx;
      s.ry = s.rest.ry;
      b.apply();
      s.turnTimer = window.setTimeout(function () {
        box.classList.remove("animate");
        s.turning = false;
        s.swayStart = performance.now();
      }, reduceMotion ? 0 : TURN_MS);
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
      s.swayStart = performance.now();
    }
    stage.addEventListener("pointerup", endDrag);
    stage.addEventListener("pointercancel", endDrag);
  }

  // The button flips the box half a turn, always the same way round; the
  // arrow keys turn it in steps from where it rests (not from the swayed
  // angle, so repeated presses don't drift). Arrows with a modifier are left
  // to the browser (Alt+Left is "back").
  function setupControls(stage, flip, b) {
    var s = b.state;
    if (flip) {
      flip.addEventListener("click", function () {
        s.rest.ry += 180;
        b.turnToRest();
      });
    }
    stage.tabIndex = 0;
    stage.addEventListener("keydown", function (e) {
      if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) return;
      var step = { ArrowLeft: [0, -15], ArrowRight: [0, 15], ArrowUp: [5, 0], ArrowDown: [-5, 0] }[e.key];
      if (!step) return;
      e.preventDefault();
      s.rest = { rx: Math.max(-35, Math.min(35, s.rest.rx + step[0])), ry: s.rest.ry + step[1] };
      b.turnToRest();
    });
  }

  // The idle sway: a slow few degrees around the resting angle, only while
  // the box is on screen (it repaints the box and its SVG filters).
  function startSway(stage, b) {
    var s = b.state, visible = false, frame = 0;
    function sway(now) {
      frame = 0;
      if (!visible) return;
      if (!s.dragging && !s.turning) {
        var t = (now - s.swayStart) / 1000;
        var amount = Math.min(1, t / 2); // ease in after a turn
        s.ry = s.rest.ry + Math.sin(t * 0.6) * 6 * amount;
        s.rx = s.rest.rx + Math.sin(t * 0.43) * 2 * amount;
        b.apply();
      }
      frame = window.requestAnimationFrame(sway);
    }
    if (reduceMotion || !("IntersectionObserver" in window)) return;
    new IntersectionObserver(function (entries) {
      visible = entries[entries.length - 1].isIntersecting;
      if (visible && !frame) frame = window.requestAnimationFrame(sway);
    }).observe(stage);
  }

  function setupBox() {
    var stage = document.querySelector(".box-stage");
    var box = document.querySelector(".box");
    var flip = document.getElementById("flip");
    var controls = document.querySelector(".box-controls");
    if (!stage || !box) return;
    var b = createBox(stage, flip);
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
    if (!("IntersectionObserver" in window) || reduceMotion) return;
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

  // ---------- the trailers ----------
  // The trailers have voice-over and play only when clicked, one at a time:
  // a trailer that starts pauses any other. Trailers and clips don't talk
  // over each other: a trailer playing with sound mutes the game clips (they
  // stay muted; a click on their speaker brings the sound back), and
  // unmuting a clip pauses a trailer that plays with sound. A muted trailer,
  // say one watched with captions, leaves the clips alone. (Clips the
  // visitor unmutes side by side are left as they are.)
  function setupTrailers() {
    var trailers = [].slice.call(document.querySelectorAll(".trailer video"));
    var clips = [].slice.call(document.querySelectorAll(".video video"));
    var ours = new WeakSet(); // clips whose next volumechange we caused

    function audible(t) { return !t.paused && !t.muted && t.volume > 0; }

    function pauseOthers(t) {
      trailers.forEach(function (other) { if (other !== t && !other.paused) other.pause(); });
    }

    function muteClips(t) {
      if (!audible(t)) return;
      clips.forEach(function (c) {
        if (c.muted) return;
        ours.add(c);
        c.muted = true;
      });
    }

    trailers.forEach(function (t) {
      t.addEventListener("play", function () { pauseOthers(t); muteClips(t); });
      t.addEventListener("volumechange", function () { muteClips(t); });
    });
    clips.forEach(function (c) {
      c.addEventListener("volumechange", function () {
        if (ours.delete(c) || c.muted) return;
        trailers.forEach(function (t) { if (audible(t)) t.pause(); });
      });
    });
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

  setupBox();
  setupVideos();
  setupTrailers();
  setupLightbox();
})();
