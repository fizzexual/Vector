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
  var FILTER = { view: "recent", source: "all", q: "", sort: "recent" };
  var detailUid = null;
  var history = [];
  var histIndex = -1;

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
      if (s && s.defaultSort) { FILTER.sort = s.defaultSort; var sel = $("#sort"); if (sel) sel.value = s.defaultSort; }
    }).catch(function () {});

    vector.getLibrary().then(function (list) {
      LIB = list || [];
      buildBackground();
      pushHistory();
      render();
      return vector.getSettings();
    }).then(function (s) {
      if (s && s.scanOnLaunch) rescan(true);
    }).catch(function () {
      render();
      toast("Couldn't load your library", true);
    });
  }

  function rescan(silent) {
    var btn = $("#btn-rescan");
    if (btn) btn.classList.add("loading");
    vector.scan().then(function (list) {
      LIB = list || [];
      buildBackground();
      render();
      if (!silent) toast(LIB.length + " games detected");
    }).catch(function () {
      if (!silent) toast("Rescan failed", true);
    }).then(function () {
      if (btn) btn.classList.remove("loading");
    });
  }

  // ---- background collage ----
  function buildBackground() {
    var bg = $("#bg");
    if (!bg) return;
    var covers = [];
    for (var i = 0; i < LIB.length; i++) {
      var c = LIB[i].art && LIB[i].art.cover;
      if (c && covers.indexOf(c) === -1) covers.push(c);
    }
    if (!covers.length) { bg.innerHTML = ""; return; }
    var tiles = [];
    for (var k = 0; k < 24; k++) tiles.push(covers[k % covers.length]);
    bg.innerHTML = tiles.map(function (u) {
      return '<img src="' + esc(u) + '" alt="" onerror="this.style.visibility=\'hidden\'">';
    }).join("");
  }

  // ---- history (back / forward) ----
  function snapshot() { return { view: FILTER.view, source: FILTER.source }; }
  function pushHistory() {
    history = history.slice(0, histIndex + 1);
    var prev = history[histIndex];
    var cur = snapshot();
    if (prev && prev.view === cur.view && prev.source === cur.source) return;
    history.push(cur);
    histIndex = history.length - 1;
    updateNavArrows();
  }
  function applyHistory() {
    var s = history[histIndex];
    FILTER.view = s.view; FILTER.source = s.source; FILTER.q = "";
    var sb = $("#search"); if (sb) sb.value = "";
    setActiveNav(); render(); updateNavArrows();
  }
  function navBack() { if (histIndex > 0) { histIndex--; applyHistory(); } }
  function navFwd() { if (histIndex < history.length - 1) { histIndex++; applyHistory(); } }
  function updateNavArrows() {
    var b = $("#nav-back"), f = $("#nav-fwd");
    if (b) b.disabled = histIndex <= 0;
    if (f) f.disabled = histIndex >= history.length - 1;
  }

  function goView(view) {
    FILTER.view = view; FILTER.source = "all"; FILTER.q = "";
    var sb = $("#search"); if (sb) sb.value = "";
    pushHistory(); setActiveNav(); render();
  }
  function goSource(source) {
    FILTER.view = "all"; FILTER.source = source; FILTER.q = "";
    var sb = $("#search"); if (sb) sb.value = "";
    pushHistory(); setActiveNav(); render();
  }

  // ---- grid view computation ----
  function visibleGames() {
    var list = LIB.filter(function (g) { return !g.hidden; });
    if (FILTER.view === "favorites") list = list.filter(function (g) { return g.favorite; });
    else if (FILTER.view === "installed") list = list.filter(function (g) { return g.installed; });

    if (FILTER.source !== "all") list = list.filter(function (g) { return g.source === FILTER.source; });

    if (FILTER.q) {
      var q = FILTER.q.toLowerCase();
      list = list.filter(function (g) { return g.name.toLowerCase().indexOf(q) !== -1; });
    }

    list = list.slice();
    if (FILTER.sort === "name") list.sort(function (a, b) { return a.name.localeCompare(b.name); });
    else if (FILTER.sort === "size") list.sort(function (a, b) { return (b.sizeBytes || 0) - (a.sizeBytes || 0); });
    else list.sort(function (a, b) { return (b.lastPlayed || 0) - (a.lastPlayed || 0) || a.name.localeCompare(b.name); });
    return list;
  }

  // ---- rendering ----
  function render() {
    renderCounts();
    if (FILTER.view === "recent") {
      $("#dashboard").classList.remove("hidden");
      $("#library").classList.add("hidden");
      renderDashboard();
    } else {
      $("#dashboard").classList.add("hidden");
      $("#library").classList.remove("hidden");
      renderLibrary();
    }
  }

  function renderLibrary() {
    var list = visibleGames();
    var titleMap = { all: "All games", favorites: "Favorites", installed: "Installed" };
    var title = FILTER.source !== "all"
      ? (PLAT[FILTER.source] ? PLAT[FILTER.source].label : FILTER.source)
      : (titleMap[FILTER.view] || "Library");
    $("#view-title").textContent = title;
    $("#view-count").textContent = list.length + (list.length === 1 ? " game" : " games");

    var grid = $("#grid"), empty = $("#empty");
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
    var counts = { all: active.length, installed: 0, steam: 0, epic: 0, gog: 0, manual: 0 };
    active.forEach(function (g) {
      if (g.installed) counts.installed++;
      if (counts[g.source] != null) counts[g.source]++;
    });
    $$("[data-count]").forEach(function (el) {
      var k = el.getAttribute("data-count");
      el.textContent = counts[k] || 0;
    });
  }

  // ---- dashboard ----
  function renderDashboard() {
    var active = LIB.filter(function (g) { return !g.hidden; });

    var recent = active.filter(function (g) { return g.lastPlayed > 0; })
      .sort(function (a, b) { return b.lastPlayed - a.lastPlayed; }).slice(0, 16);
    var carousel = $("#recent-carousel");
    carousel.innerHTML = recent.length
      ? recent.map(card).join("")
      : '<div class="carousel-empty">No recently played games yet — launch one and it\'ll appear here.</div>';

    renderSummary(active);

    var added = active.slice().sort(function (a, b) {
      return (b.addedAt || b.lastPlayed || 0) - (a.addedAt || a.lastPlayed || 0);
    }).slice(0, 8);
    $("#recent-rows").innerHTML = added.map(tableRow).join("");
  }

  function renderSummary(active) {
    var total = active.length;
    var installed = active.filter(function (g) { return g.installed; }).length;
    var sizeBytes = active.reduce(function (s, g) { return s + (g.sizeBytes || 0); }, 0);
    var bySrc = { steam: 0, epic: 0, gog: 0, manual: 0 };
    active.forEach(function (g) { if (bySrc[g.source] != null) bySrc[g.source]++; });
    var connected = ["steam", "epic", "gog", "manual"].filter(function (k) { return bySrc[k] > 0; }).length;
    var maxc = Math.max(1, bySrc.steam, bySrc.epic, bySrc.gog, bySrc.manual);
    var bars = ["steam", "epic", "gog", "manual"].map(function (k) {
      return '<div class="bar" title="' + esc(PLAT[k].label) + ': ' + bySrc[k] +
        '" style="height:' + Math.round((bySrc[k] / maxc) * 100) + '%;background:var(--' + k + ')"></div>';
    }).join("");
    var sizeGb = sizeBytes ? (sizeBytes / 1e9 >= 100 ? Math.round(sizeBytes / 1e9) : (sizeBytes / 1e9).toFixed(1)) : 0;
    $("#summary").innerHTML =
      metric("Games in library", total, "accent") +
      metric("Installed", installed, "") +
      metric("Library size", sizeGb + " GB", "amber") +
      '<div class="metric"><div class="metric-label">By platform · ' + connected + ' connected</div>' +
      '<div class="metric-bars">' + bars + '</div></div>';
  }

  function metric(label, value, cls) {
    return '<div class="metric"><div class="metric-label">' + esc(label) + '</div>' +
      '<div class="metric-value ' + cls + '">' + esc(String(value)) + '</div></div>';
  }

  function tableRow(g) {
    var p = PLAT[g.source] || PLAT.manual;
    var cover = g.art && g.art.cover;
    var thumb = cover
      ? '<img class="rt-thumb" src="' + esc(cover) + '" alt="" onerror="this.style.visibility=\'hidden\'">'
      : '<div class="rt-thumb" style="--tile:' + tileColor(g.name) + '"></div>';
    var last = g.lastPlayed ? relDate(g.lastPlayed) : "—";
    var size = g.sizeBytes ? fmtSize(g.sizeBytes) : (g.installed ? "Installed" : "—");
    return '<tr data-uid="' + esc(g.uid) + '">' +
      '<td><div class="rt-game">' + thumb + '<span class="rt-name">' + esc(g.name) + '</span></div></td>' +
      '<td>' + esc(last) + '</td>' +
      '<td>' + esc(size) + '</td>' +
      '<td><span class="rt-source"><span class="dot dot-' + esc(g.source) + '"></span>' + esc(p.label) + '</span></td>' +
      '</tr>';
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
    g.favorite = !g.favorite;
    vector.setFavorite(uid, g.favorite).catch(function () {});
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
    $("#detail-play").classList.toggle("hidden", !g.installed);
    var fav = $("#detail-fav");
    fav.classList.toggle("active", !!g.favorite);
    fav.innerHTML = '<span class="ic">♥</span> ' + (g.favorite ? "Favorited" : "Favorite");
  }

  // ---- add game modal ----
  function openAdd() {
    $("#add-name").value = ""; $("#add-exe").value = ""; $("#add-art").value = "";
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
        pushHistory(); setActiveNav(); render();
        toast(name + " added");
      }).catch(function () { toast("Couldn't add game", true); });
  }

  // ---- modals + toast ----
  function openModal(id) { $("#" + id).classList.add("open"); }
  function closeModal(id) { $("#" + id).classList.remove("open"); if (id === "detail") detailUid = null; }

  var toastTimer = null;
  function toast(msg, isError) {
    var t = $("#toast");
    t.textContent = msg;
    t.classList.toggle("error", !!isError);
    t.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { t.classList.remove("show"); }, 2600);
  }

  function setActiveNav() {
    var bySource = FILTER.source !== "all";
    $$(".nav-item[data-view]").forEach(function (el) {
      el.classList.toggle("active", !bySource && el.getAttribute("data-view") === FILTER.view);
    });
    $$(".nav-item[data-source]").forEach(function (el) {
      el.classList.toggle("active", bySource && el.getAttribute("data-source") === FILTER.source);
    });
  }

  function guessName(path) {
    var base = String(path).replace(/\\/g, "/").split("/").pop() || "";
    return base.replace(/\.[^.]+$/, "");
  }

  // ---- connect stores (integrations) ----
  var STORE_META = {
    steam: { name: "Steam", color: "#1b2838", letter: "S" },
    epic: { name: "Epic Games", color: "#2f2f2f", letter: "E" },
    gog: { name: "GOG", color: "#7b3fe4", letter: "G" },
    xbox: { name: "Xbox", color: "#107c10", letter: "X" },
    ea: { name: "EA app", color: "#d83c3c", letter: "EA" },
    ubisoft: { name: "Ubisoft Connect", color: "#1f7bd6", letter: "U" },
    battlenet: { name: "Battle.net", color: "#1486d6", letter: "B" }
  };

  function openStores() { openModal("stores"); renderStores(); }

  function renderStores() {
    var list = $("#stores-list");
    list.innerHTML = '<div class="muted" style="padding:14px 2px;">Checking your launchers…</div>';
    vector.getStores().then(function (stores) {
      list.innerHTML = (stores || []).map(storeRow).join("");
    }).catch(function () {
      list.innerHTML = '<div class="muted" style="padding:14px 2px;">Couldn\'t read your stores.</div>';
    });
    vector.getSettings().then(function (s) {
      var t = $("#stores-scan-toggle");
      if (t) t.classList.toggle("on", !!(s && s.scanOnLaunch));
    }).catch(function () {});
  }

  function storeRow(s) {
    var m = STORE_META[s.id] || { name: s.id, color: "#444", letter: "?" };
    var statusText, right;
    if (s.comingSoon) {
      statusText = "Coming soon";
      right = '<span class="badge-soon">Soon</span>';
    } else if (!s.installed) {
      statusText = "Not installed";
      right = '<span class="store-status-muted">Not found</span>';
    } else {
      var n = s.count || 0;
      statusText = (s.enabled ? "Connected" : "Off") + " · " + n + " game" + (n === 1 ? "" : "s");
      right = '<button class="switch' + (s.enabled ? " on" : "") + '" data-store="' + esc(s.id) +
        '" role="switch" aria-checked="' + (s.enabled ? "true" : "false") + '"><span class="switch-knob"></span></button>';
    }
    var path = (s.installed && s.path) ? '<div class="store-path">' + esc(s.path) + '</div>' : '';
    return '<div class="store-row' + (s.comingSoon ? ' soon' : '') + '">' +
      '<div class="store-logo" style="background:' + m.color + '">' + esc(m.letter) + '</div>' +
      '<div class="store-info"><div class="store-name">' + esc(m.name) + '</div>' +
      '<div class="store-status">' + esc(statusText) + '</div>' + path + '</div>' +
      '<div class="store-right">' + right + '</div></div>';
  }

  function toggleStore(id, enable) {
    var patch = { stores: {} };
    patch.stores[id] = enable;
    var name = STORE_META[id] ? STORE_META[id].name : id;
    vector.setSettings(patch).then(function () {
      renderStores();
      toast((enable ? "Connected " : "Disconnected ") + name);
      return vector.scan();
    }).then(function (lib) {
      if (lib) { LIB = lib; buildBackground(); render(); }
    }).catch(function () {});
  }

  // ---- events ----
  function wireEvents() {
    var searchEl = $("#search"), searchT = null;
    if (searchEl) searchEl.addEventListener("input", function (e) {
      clearTimeout(searchT);
      var v = e.target.value;
      searchT = setTimeout(function () { FILTER.q = v; renderLibrary(); }, 120);
    });
    var sortEl = $("#sort");
    if (sortEl) sortEl.addEventListener("change", function (e) { FILTER.sort = e.target.value; renderLibrary(); });
    var rescanEl = $("#btn-rescan");
    if (rescanEl) rescanEl.addEventListener("click", function () { rescan(false); });
    var emptyRescan = $("#empty-rescan");
    if (emptyRescan) emptyRescan.addEventListener("click", function () { rescan(false); });

    $$(".nav-item[data-view]").forEach(function (el) {
      el.addEventListener("click", function () { goView(el.getAttribute("data-view")); });
    });
    $$(".nav-item[data-source]").forEach(function (el) {
      el.addEventListener("click", function () { goSource(el.getAttribute("data-source")); });
    });

    $("#btn-add").addEventListener("click", openAdd);
    $("#btn-add-top").addEventListener("click", openStores);
    $("#btn-stores").addEventListener("click", openStores);
    $("#btn-settings").addEventListener("click", openStores);
    $("#nav-back").addEventListener("click", navBack);
    $("#nav-fwd").addEventListener("click", navFwd);

    var promoX = $("#promo-x");
    if (promoX) promoX.addEventListener("click", function () { var p = $("#promo"); if (p) p.classList.add("hidden"); });
    var promoConnect = $("#promo-connect");
    if (promoConnect) promoConnect.addEventListener("click", openStores);

    var car = $("#recent-carousel");
    var rpPrev = $("#rp-prev"), rpNext = $("#rp-next");
    if (rpPrev && car) rpPrev.addEventListener("click", function () { car.scrollBy({ left: -480, behavior: "smooth" }); });
    if (rpNext && car) rpNext.addEventListener("click", function () { car.scrollBy({ left: 480, behavior: "smooth" }); });

    [$("#grid"), $("#recent-carousel")].forEach(function (container) {
      if (!container) return;
      container.addEventListener("click", function (e) {
        var playBtn = e.target.closest("[data-play]");
        if (playBtn) { e.stopPropagation(); play(playBtn.getAttribute("data-play")); return; }
        var c = e.target.closest(".card");
        if (c) openDetail(c.getAttribute("data-uid"));
      });
    });
    var gridEl = $("#grid");
    if (gridEl) gridEl.addEventListener("keydown", function (e) {
      if (e.key !== "Enter") return;
      var c = e.target.closest(".card");
      if (c) openDetail(c.getAttribute("data-uid"));
    });
    var rows = $("#recent-rows");
    if (rows) rows.addEventListener("click", function (e) {
      var tr = e.target.closest("tr[data-uid]");
      if (tr) openDetail(tr.getAttribute("data-uid"));
    });

    $("#detail-play").addEventListener("click", function () { if (detailUid) play(detailUid); });
    $("#detail-folder").addEventListener("click", function () { if (detailUid) vector.showInFolder(detailUid).catch(function () {}); });
    $("#detail-fav").addEventListener("click", function () { if (detailUid) toggleFavorite(detailUid); });
    $("#detail-hide").addEventListener("click", function () { if (detailUid) hideGame(detailUid); });

    $("#add-browse-exe").addEventListener("click", function () {
      vector.pickExecutable().then(function (p) {
        if (p) { $("#add-exe").value = p; if (!$("#add-name").value) $("#add-name").value = guessName(p); }
      }).catch(function () {});
    });
    $("#add-browse-art").addEventListener("click", function () {
      vector.pickImage().then(function (p) { if (p) $("#add-art").value = p; }).catch(function () {});
    });
    $("#add-submit").addEventListener("click", submitAdd);

    var storesList = $("#stores-list");
    if (storesList) storesList.addEventListener("click", function (e) {
      var sw = e.target.closest(".switch[data-store]");
      if (sw) toggleStore(sw.getAttribute("data-store"), !sw.classList.contains("on"));
    });
    var scanToggle = $("#stores-scan-toggle");
    if (scanToggle) scanToggle.addEventListener("click", function () {
      var on = !scanToggle.classList.contains("on");
      scanToggle.classList.toggle("on", on);
      vector.setSettings({ scanOnLaunch: on }).catch(function () {});
    });

    $$("[data-close]").forEach(function (el) {
      el.addEventListener("click", function () { closeModal(el.getAttribute("data-close")); });
    });
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape") { closeModal("detail"); closeModal("addmodal"); closeModal("stores"); }
    });
    $$(".win-btn[data-win]").forEach(function (el) {
      el.addEventListener("click", function () { vector.windowCommand(el.getAttribute("data-win")).catch(function () {}); });
    });
    var titlebar = document.querySelector(".titlebar");
    if (titlebar) titlebar.addEventListener("dblclick", function (e) {
      if (e.target.closest(".win-btn") || e.target.closest(".tb-left") || e.target.closest(".add-btn")) return;
      vector.windowCommand("maximize").catch(function () {});
    });
  }

  document.addEventListener("DOMContentLoaded", boot);
})();
