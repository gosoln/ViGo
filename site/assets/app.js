(function () {
  "use strict";

  var REPO = "gosoln/ViGo";

  // 1. Highlight the download card that matches the visitor's OS.
  function detectOS() {
    var ua = (navigator.userAgent || "") + " " + (navigator.platform || "");
    if (/Android|iPhone|iPad|iPod/i.test(ua)) return null;
    if (/Mac/i.test(ua)) return "macos";
    if (/Win/i.test(ua)) return "windows";
    if (/Linux|X11/i.test(ua)) return "linux";
    return null;
  }
  var os = detectOS();
  if (os) {
    var card = document.querySelector('[data-os="' + os + '"]');
    if (card) card.classList.add("is-current");
    var heroBtn = document.getElementById("hero-download");
    var label = document.querySelector('[data-os="' + os + '"] .btn');
    if (heroBtn && label) {
      heroBtn.setAttribute("href", "#download");
      heroBtn.textContent = heroBtn.getAttribute("data-" + os) || heroBtn.textContent;
    }
  }

  // 2. Point download buttons at the latest release assets (falls back to the Releases page).
  fetch("https://api.github.com/repos/" + REPO + "/releases/latest", {
    headers: { Accept: "application/vnd.github+json" },
  })
    .then(function (r) { return r.ok ? r.json() : null; })
    .then(function (release) {
      if (!release || !release.assets) return;
      var version = (release.tag_name || "").replace(/^v/, "");
      document.querySelectorAll("[data-version]").forEach(function (el) {
        if (version) el.textContent = version;
      });
      document.querySelectorAll("[data-asset]").forEach(function (a) {
        var re = new RegExp(a.getAttribute("data-asset"), "i");
        var hit = release.assets.find(function (x) { return re.test(x.name); });
        if (hit) {
          a.setAttribute("href", hit.browser_download_url);
          var size = a.parentElement.querySelector("[data-size]");
          if (size) size.textContent = hit.name + " · " + (hit.size / 1048576).toFixed(1) + " MB";
        }
      });
    })
    .catch(function () { /* keep fallback links */ });

  // 3. Typing demo: Telex keys turning into Vietnamese text.
  var keysEl = document.getElementById("demo-keys");
  var outEl = document.getElementById("demo-out");
  if (!keysEl || !outEl) return;

  var steps = [
    ["t", "t"], ["i", "ti"], ["e", "tie"], ["e", "tiê"], ["n", "tiên"], ["g", "tiêng"], ["s", "tiếng"],
    [" ", "tiếng "],
    ["v", "tiếng v"], ["i", "tiếng vi"], ["e", "tiếng vie"], ["e", "tiếng viê"], ["t", "tiếng viêt"], ["j", "tiếng việt"],
  ];
  var cursor = '<span class="cur"></span>';
  function render(keys, out) {
    keysEl.textContent = keys;
    outEl.innerHTML = out + cursor;
  }
  if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
    render("tieengs vieetj", "tiếng việt");
    return;
  }
  var i = 0, keys = "";
  function tick() {
    if (i >= steps.length) {
      setTimeout(function () { i = 0; keys = ""; render("", ""); setTimeout(tick, 500); }, 2400);
      return;
    }
    keys += steps[i][0];
    render(keys, steps[i][1]);
    i++;
    setTimeout(tick, 230);
  }
  render("", "");
  setTimeout(tick, 600);
})();
