// Gunrunners web page: the turnable game box, the gameplay clips that play
// while they are on screen, and a lightbox for the screenshots. No
// dependencies; everything degrades to a static page without JavaScript.
(function () {
  "use strict";

  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  // ---------- the box ----------
  // The box is turned by setting --rx/--ry on .box-stage (style.css reads
  // them). Dragging turns it freely; the button flips between the front
  // and back views; left alone, it sways a little around its resting angle.
  function setupBox() {
    var stage = document.querySelector(".box-stage");
    var box = document.querySelector(".box");
    var flip = document.getElementById("flip");
    if (!stage || !box) return;

    var rest = { rx: -6, ry: -24 }; // the three-quarter view of the front
    var rx = rest.rx, ry = rest.ry;
    var drag = null;
    var swayStart = performance.now();

    function apply() {
      stage.style.setProperty("--rx", rx + "deg");
      stage.style.setProperty("--ry", ry + "deg");
      // The sheen slides across the face as it turns: one full sweep per
      // half turn, so it lines up again after a flip.
      var phase = ((ry % 180) + 180) % 180;
      stage.style.setProperty("--sheen", (phase / 180) * 100 + "%");
      if (flip) flip.textContent = showingBack() ? "Turn the box to the front" : "Turn the box over";
    }

    function showingBack() {
      var a = ((ry % 360) + 360) % 360;
      return a > 90 && a < 270;
    }

    // Turn smoothly to an angle with the CSS transition.
    function turnTo(newRx, newRy) {
      box.classList.add("animate");
      rx = newRx;
      ry = newRy;
      apply();
      window.setTimeout(function () { box.classList.remove("animate"); swayStart = performance.now(); }, 950);
    }

    if (flip) {
      // Half a turn further, always the same way round, from where the box
      // was left: front becomes back and back becomes front.
      flip.addEventListener("click", function () {
        rest.ry += 180;
        turnTo(rest.rx, rest.ry);
      });
    }

    stage.addEventListener("pointerdown", function (e) {
      drag = { x: e.clientX, y: e.clientY, rx: rx, ry: ry, id: e.pointerId };
      stage.setPointerCapture(e.pointerId);
      stage.classList.add("dragging");
    });
    stage.addEventListener("pointermove", function (e) {
      if (!drag || e.pointerId !== drag.id) return;
      ry = drag.ry + (e.clientX - drag.x) * 0.5;
      rx = Math.max(-35, Math.min(35, drag.rx - (e.clientY - drag.y) * 0.3));
      apply();
    });
    function endDrag(e) {
      if (!drag || e.pointerId !== drag.id) return;
      drag = null;
      stage.classList.remove("dragging");
      swayStart = performance.now();
      rest.ry = ry;
      rest.rx = rx;
    }
    stage.addEventListener("pointerup", endDrag);
    stage.addEventListener("pointercancel", endDrag);

    // Keyboard: the stage is focusable; the arrows turn the box.
    stage.tabIndex = 0;
    stage.addEventListener("keydown", function (e) {
      var step = { ArrowLeft: [0, -15], ArrowRight: [0, 15], ArrowUp: [5, 0], ArrowDown: [-5, 0] }[e.key];
      if (!step) return;
      e.preventDefault();
      turnTo(Math.max(-35, Math.min(35, rx + step[0])), ry + step[1]);
      rest.rx = rx;
      rest.ry = ry;
    });

    // The idle sway: a slow few degrees around where the box was left.
    function sway(now) {
      if (!drag && !box.classList.contains("animate")) {
        var t = (now - swayStart) / 1000;
        var amount = Math.min(1, t / 2); // ease in after a turn
        ry = rest.ry + Math.sin(t * 0.6) * 6 * amount;
        rx = rest.rx + Math.sin(t * 0.43) * 2 * amount;
        apply();
      }
      window.requestAnimationFrame(sway);
    }
    apply();
    if (!reduceMotion) window.requestAnimationFrame(sway);
  }

  // ---------- the clips ----------
  // Muted clips play while at least half of them is on screen, so a long
  // page doesn't decode six videos at once. A clip someone paused stays
  // paused.
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
        if (en.isIntersecting && !userPaused.has(v)) {
          v.preload = "auto";
          var p = v.play();
          if (p && p.catch) p.catch(function () {});
        } else if (!en.isIntersecting && !v.paused) {
          v.dataset.auto = "1"; // our pause, not the viewer's
          v.pause();
        }
      });
    }, { threshold: 0.5 });
    videos.forEach(function (v) { io.observe(v); });
  }

  // ---------- the screenshots ----------
  function setupLightbox() {
    if (typeof HTMLDialogElement !== "function") return;
    var dialog = document.createElement("dialog");
    dialog.className = "lightbox";
    dialog.innerHTML = '<img alt=""><button type="button" class="btn btn-small">Close</button>';
    document.body.appendChild(dialog);
    var img = dialog.querySelector("img");
    dialog.querySelector("button").addEventListener("click", function () { dialog.close(); });
    dialog.addEventListener("click", function (e) { if (e.target === dialog || e.target === img) dialog.close(); });
    document.querySelectorAll(".shot-grid a").forEach(function (a) {
      a.addEventListener("click", function (e) {
        e.preventDefault();
        var thumb = a.querySelector("img");
        img.src = a.getAttribute("href");
        img.alt = thumb ? thumb.alt : "";
        dialog.showModal();
      });
    });
  }

  setupBox();
  setupVideos();
  setupLightbox();
})();
