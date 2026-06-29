// app.js — Vector renderer logic (vanilla). Talks to the native host via
// window.vector (see bridge.js). All filtering/sorting/search is client-side.
(function () {
  "use strict";

  var PLAT = {
    steam: { label: "Steam" },
    epic: { label: "Epic" },
    gog: { label: "GOG" },
    manual: { label: "Manual" }
  };

  var LIB = [];
  var FILTER = { view: "all", source: "all", q: "", sort: "recent" };
  var detailUid = null;

  // ---- helpers ----
  var $ = function (sel, root) { return (root || document).querySelector(sel); };
  var $$ = function (sel, root) { return Array.prototype.slice.call((root || document).querySelectorAll(sel)); };

  function esc(s) {
    return String(s == null ? "" : s)
      .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;").replace(/'/g, "&#39;");
  }
  function hashHue(s) {
    var h = 0; s = s || "";
    for (var i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) | 0;
    return Math.abs(h) % 360;
  }
  function tileColor(name) { return "hsl(" + hashHue(name) + ", 42%, 23%)"; }
  function fmtSize(b) {
    if (!b) return "";
    var gb = b / 1e9;
    if (gb >= 1) return (gb >= 10 ? Math.round(gb) : gb.toFixed(1)) + " GB";
    return Math.max(1, Math.round(b / 1e6)) + " MB";
  }
  function relDate(ts) {
    if (!ts) return "";
    var d = Math.floor((Date.now() - ts) / 86400000);
    if (d <= 0) return "today";
    if (d === 1) return "yesterday";
    if (d < 30) return d + " days ago";
    var m = Math.floor(d / 30);
    return m + (m === 1 ? " month ago" : " months ago");
  }

  // ---- data flow ----
  function boot() {
    wireEvents();
    vector.getSettings().then(function (s) {
      if (s && s.defaultSort) { FILTER.sort = s.defaultSort; $("#sort").value = s.defaultSort; }
    }).catch(function () {});

    vector.getLibrary().then(function (list) {
      LIB = list || [];
      render();
      // background refresh
      return vector.getSettings();
    }).then(function (s) {
      if (s && s.scanOnLaunch) rescan(true);
    }).catch(function (e) {
      render();
      toast("Couldn't load your library", true);
    });
  }

  function rescan(silent) {
    var btn = $("#btn-rescan");
    btn.classList.add("loading");
    vector.scan().then(function (list) {
      LIB = list || [];
      render();
      if (!silent) toast(LIB.length + " games detected");
    }).catch(function () {
      if (!silent) toast("Rescan failed", true);
    }).then(function () {
      btn.classList.remove("loading");
    });
  }

  // ---- view computation ----
  function visibleGames() {
    var list = LIB.filter(function (g) { return !g.hidden; });
    if (FILTER.view === "favorites") list = list.filter(function (g) { return g.favorite; });
    else if (FILTER.view === "installed") list = list.filter(function (g) { return g.installed; });
    else if (FILTER.view === "recent") list = list.filter(function (g) { return g.lastPlayed > 0; });

    if (FILTER.source !== "all") list = list.filter(function (g) { return g.source === FILTER.source; });

    if (FILTER.q) {
      var q = FILTER.q.toLowerCase();
      list = list.filter(function (g) { return g.name.toLowerCase().indexOf(q) !== -1; });
    }

    var sort = FILTER.view === "recent" ? "recent" : FILTER.sort;
    list = list.slice();
    if (sort === "name") list.sort(function (a, b) { return a.name.localeCompare(b.name); });
    else if (sort === "size") list.sort(function (a, b) { return (b.sizeBytes || 0) - (a.sizeBytes || 0); });
    else list.sort(function (a, b) { return (b.lastPlayed || 0) - (a.lastPlayed || 0) || a.name.localeCompare(b.name); });
    return list;
  }

  // ---- rendering ----
  function render() {
    renderCounts();
    var list = visibleGames();
    var titleMap = { all: "All games", recent: "Recently played", favorites: "Favorites", installed: "Installed" };
    $("#view-title").textContent = titleMap[FILTER.view] || "Library";
    $("#view-count").textContent = list.length + (list.length === 1 ? " game" : " games");

    var grid = $("#grid");
    var empty = $("#empty");
    if (!list.length) {
      grid.innerHTML = "";
      empty.classList.remove("hidden");
      var anyLib = LIB.length > 0;
      $("#empty-title").textContent = anyLib ? "Nothing matches" : "No games yet";
      $("#empty-body").innerHTML = anyLib
        ? "Try a different filter or search term."
        : "Press <strong>Rescan</strong> to detect installed games, or add one manually.";
    } else {
      empty.classList.add("hidden");
      grid.innerHTML = list.map(card).join("");
    }
  }

  function renderCounts() {
    var active = LIB.filter(function (g) { return !g.hidden; });
    var counts = { all: active.length, steam: 0, epic: 0, gog: 0, manual: 0 };
    active.forEach(function (g) { if (counts[g.source] != null) counts[g.source]++; });
    $$("[data-count]").forEach(function (el) {
      var k = el.getAttribute("data-count");
      el.textContent = counts[k] || 0;
    });
  }

  function card(g) {
    var p = PLAT[g.source] || PLAT.manual;
    var sub = g.installed ? (g.sizeBytes ? fmtSize(g.sizeBytes) : "Installed") : "Not installed";
    var cover = g.art && g.art.cover;
    return '' +
      '<div class="card' + (g.installed ? "" : " not-installed") + '" data-uid="' + esc(g.uid) + '" tabindex="0">' +
        '<div class="cover" style="--tile:' + tileColor(g.name) + '">' +
          '<span class="tile-title">' + esc(g.name) + '</span>' +
          (cover ? '<img class="art" src="' + esc(cover) + '" alt="" loading="lazy" onerror="this.remove()">' : '') +
          '<div class="badge"><span class="dot dot-' + esc(g.source) + '"></span>' + esc(p.label) + '</div>' +
          (g.favorite ? '<div class="fav-mark">♥</div>' : '') +
          '<div class="card-hover"><button class="play-btn" data-play="' + esc(g.uid) + '"><span class="ic">▶</span> Play</button></div>' +
          '<div class="card-foot"><div class="card-name">' + esc(g.name) + '</div><div class="card-sub">' + esc(sub) + '</div></div>' +
        '</div>' +
      '</div>';
  }

  function gameByUid(uid) {
    for (var i = 0; i < LIB.length; i++) if (LIB[i].uid === uid) return LIB[i];
    return null;
  }

  // ---- actions ----
  function play(uid) {
    var g = gameByUid(uid);
    if (!g) return;
    toast("Launching " + g.name + "…");
    vector.launch(uid).catch(function () { toast("Couldn't start " + g.name, true); });
  }

  function toggleFavorite(uid) {
    var g = gameByUid(uid);
    if (!g) return;
    var nv = !g.favorite;
    g.favorite = nv;
    vector.setFavorite(uid, nv).catch(function () {});
    render();
    if (detailUid === uid) syncDetailButtons(g);
  }

  function hideGame(uid) {
    var g = gameByUid(uid);
    if (!g) return;
    g.hidden = true;
    vector.setHidden(uid, true).catch(function () {});
    closeModal("detail");
    render();
    toast(g.name + " hidden");
  }

  // ---- detail modal ----
  function openDetail(uid) {
    var g = gameByUid(uid);
    if (!g) return;
    detailUid = uid;
    var p = PLAT[g.source] || PLAT.manual;
    var hero = $("#detail-hero");
    hero.style.setProperty("--tile", tileColor(g.name));
    hero.style.backgroundImage = (g.art && g.art.hero) ? "url('" + g.art.hero + "')" : "none";

    $("#detail-badge").innerHTML = '<span class="dot dot-' + esc(g.source) + '"></span> ' + esc(p.label);
    $("#detail-name").textContent = g.name;

    var meta = [];
    meta.push(g.installed ? "<b>Installed</b>" : "<b>Not installed</b>");
    if (g.sizeBytes) meta.push("<b>" + fmtSize(g.sizeBytes) + "</b> on disk");
    if (g.lastPlayed) meta.push("Last played <b>" + relDate(g.lastPlayed) + "</b>");
    $("#detail-meta").innerHTML = meta.map(function (m) { return "<span>" + m + "</span>"; }).join("");

    $("#detail-path").textContent = g.installDir || g.executable || (g.launch && g.launch.value) || "";
    syncDetailButtons(g);
    openModal("detail");
  }

  function syncDetailButtons(g) {
    var play = $("#detail-play");
    play.classList.toggle("hidden", !g.installed);
    var fav = $("#detail-fav");
    fav.classList.toggle("active", !!g.favorite);
    fav.innerHTML = '<span class="ic">♥</span> ' + (g.favorite ? "Favorited" : "Favorite");
  }

  // ---- add game modal ----
  function openAdd() {
    $("#add-name").value = "";
    $("#add-exe").value = "";
    $("#add-art").value = "";
    openModal("addmodal");
  }
  function submitAdd() {
    var name = $("#add-name").value.trim();
    var exe = $("#add-exe").value.trim();
    if (!name) { toast("Enter a title", true); return; }
    if (!exe) { toast("Choose an executable", true); return; }
    vector.addManualGame({ name: name, executable: exe, art: $("#add-art").value.trim() })
      .then(function (game) {
        if (game) LIB.push(game);
        closeModal("addmodal");
        FILTER.view = "all"; FILTER.source = "all";
        setActiveNav();
        render();
        toast(name + " added");
      }).catch(function () { toast("Couldn't add game", true); });
  }

  // ---- modals ----
  function openModal(id) { $("#" + id).classList.add("open"); }
  function closeModal(id) { $("#" + id).classList.remove("open"); if (id === "detail") detailUid = null; }

  // ---- toast ----
  var toastTimer = null;
  function toast(msg, isError) {
    var t = $("#toast");
    t.textContent = msg;
    t.classList.toggle("error", !!isError);
    t.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { t.classList.remove("show"); }, 2600);
  }

  // ---- nav ----
  function setActiveNav() {
    $$(".nav-item[data-view]").forEach(function (el) {
      el.classList.toggle("active", el.getAttribute("data-view") === FILTER.view);
    });
    $$(".nav-item[data-source]").forEach(function (el) {
      el.classList.toggle("active", el.getAttribute("data-source") === FILTER.source);
    });
  }

  // ---- events ----
  function wireEvents() {
    var searchT = null;
    $("#search").addEventListener("input", function (e) {
      clearTimeout(searchT);
      var v = e.target.value;
      searchT = setTimeout(function () { FILTER.q = v; render(); }, 120);
    });
    $("#sort").addEventListener("change", function (e) { FILTER.sort = e.target.value; render(); });
    $("#btn-rescan").addEventListener("click", function () { rescan(false); });
    $("#empty-rescan").addEventListener("click", function () { rescan(false); });
    $("#btn-add").addEventListener("click", openAdd);
    $("#btn-settings").addEventListener("click", function () { toast("Settings coming soon"); });

    $$(".nav-item[data-view]").forEach(function (el) {
      el.addEventListener("click", function () { FILTER.view = el.getAttribute("data-view"); setActiveNav(); render(); });
    });
    $$(".nav-item[data-source]").forEach(function (el) {
      el.addEventListener("click", function () { FILTER.source = el.getAttribute("data-source"); setActiveNav(); render(); });
    });

    // grid delegation
    $("#grid").addEventListener("click", function (e) {
      var playBtn = e.target.closest("[data-play]");
      if (playBtn) { e.stopPropagation(); play(playBtn.getAttribute("data-play")); return; }
      var c = e.target.closest(".card");
      if (c) openDetail(c.getAttribute("data-uid"));
    });
    $("#grid").addEventListener("keydown", function (e) {
      if (e.key !== "Enter") return;
      var c = e.target.closest(".card");
      if (c) openDetail(c.getAttribute("data-uid"));
    });

    // detail buttons
    $("#detail-play").addEventListener("click", function () { if (detailUid) play(detailUid); });
    $("#detail-folder").addEventListener("click", function () { if (detailUid) vector.showInFolder(detailUid).catch(function () {}); });
    $("#detail-fav").addEventListener("click", function () { if (detailUid) toggleFavorite(detailUid); });
    $("#detail-hide").addEventListener("click", function () { if (detailUid) hideGame(detailUid); });

    // add buttons
    $("#add-browse-exe").addEventListener("click", function () {
      vector.pickExecutable().then(function (p) {
        if (p) { $("#add-exe").value = p; if (!$("#add-name").value) $("#add-name").value = guessName(p); }
      }).catch(function () {});
    });
    $("#add-browse-art").addEventListener("click", function () {
      vector.pickImage().then(function (p) { if (p) $("#add-art").value = p; }).catch(function () {});
    });
    $("#add-submit").addEventListener("click", submitAdd);

    // modal close (backdrop, x, cancel)
    $$("[data-close]").forEach(function (el) {
      el.addEventListener("click", function () { closeModal(el.getAttribute("data-close")); });
    });
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape") { closeModal("detail"); closeModal("addmodal"); }
    });

    // window controls
    $$(".win-btn[data-win]").forEach(function (el) {
      el.addEventListener("click", function () { vector.windowCommand(el.getAttribute("data-win")).catch(function () {}); });
    });
    var titlebar = document.querySelector(".titlebar");
    if (titlebar) {
      titlebar.addEventListener("dblclick", function (e) {
        if (!e.target.closest(".win-btn")) vector.windowCommand("maximize").catch(function () {});
      });
    }
  }

  function guessName(path) {
    var base = String(path).replace(/\\/g, "/").split("/").pop() || "";
    return base.replace(/\.[^.]+$/, "");
  }

  document.addEventListener("DOMContentLoaded", boot);
})();
