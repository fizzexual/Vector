// mock-data.js — browser-only fake backend. Inert under CEF (bridge uses the
// native handler when window.cefQuery exists). Lets us build & screenshot the
// UI without the C++ host. Real cover art comes from the native Steam scan.
(function () {
  "use strict";
  if (typeof window.cefQuery === "function") return; // native present, skip mock

  var steamImg = function (id) {
    return "https://cdn.cloudflare.steamstatic.com/steam/apps/" + id + "/library_600x900.jpg";
  };
  var DAY = 86400000;
  var now = Date.now();
  var ago = function (d) { return now - d * DAY; };

  function g(uid, source, id, name, installed, sizeBytes, cover, launch, lastPlayed) {
    return {
      uid: uid, source: source, id: id, name: name,
      installed: installed, installDir: installed ? "D:/Games/" + name.replace(/[^A-Za-z0-9]/g, "") : "",
      executable: "", sizeBytes: sizeBytes,
      art: { cover: cover || "", hero: cover || "", logo: "" },
      launch: { kind: source === "steam" ? "uri" : "exe", value: launch },
      lastPlayed: lastPlayed || 0, favorite: false, hidden: false, addedAt: now
    };
  }

  var games = [
    g("steam:1091500", "steam", "1091500", "Cyberpunk 2077", true, 70e9, steamImg(1091500), "steam://rungameid/1091500", ago(2)),
    g("steam:1245620", "steam", "1245620", "Elden Ring", true, 60e9, steamImg(1245620), "steam://rungameid/1245620", ago(1)),
    g("steam:1086940", "steam", "1086940", "Baldur's Gate 3", true, 122e9, steamImg(1086940), "steam://rungameid/1086940", ago(5)),
    g("steam:367520", "steam", "367520", "Hollow Knight", true, 9e9, steamImg(367520), "steam://rungameid/367520", ago(12)),
    g("steam:413150", "steam", "413150", "Stardew Valley", true, 1.2e9, steamImg(413150), "steam://rungameid/413150", ago(3)),
    g("steam:1174180", "steam", "1174180", "Red Dead Redemption 2", true, 120e9, steamImg(1174180), "steam://rungameid/1174180", ago(20)),
    g("steam:292030", "steam", "292030", "The Witcher 3: Wild Hunt", true, 50e9, steamImg(292030), "steam://rungameid/292030", ago(8)),
    g("steam:782330", "steam", "782330", "DOOM Eternal", true, 80e9, steamImg(782330), "steam://rungameid/782330", ago(6)),
    g("steam:268910", "steam", "268910", "Cuphead", true, 4e9, steamImg(268910), "steam://rungameid/268910", ago(15)),
    g("steam:632470", "steam", "632470", "Disco Elysium", false, 0, steamImg(632470), "steam://rungameid/632470", ago(40)),
    g("epic:Fortnite", "epic", "Fortnite", "Fortnite", true, 30e9, "", "com.epicgames.launcher://apps/Fortnite?action=launch", ago(1)),
    g("epic:Sugar", "epic", "Sugar", "Rocket League", true, 22e9, "", "com.epicgames.launcher://apps/Sugar?action=launch", ago(4)),
    g("gog:1207664663", "gog", "1207664663", "The Witcher: Enhanced Edition", true, 8e9, "", "D:/GOG/Witcher/witcher.exe", ago(30)),
    g("gog:1430740", "gog", "1430740", "CYBERPUNK 2077 (GOG)", false, 0, "", "D:/GOG/Cyberpunk/launch.exe", 0),
    g("manual:retro-1", "manual", "retro-1", "Minecraft Java Edition", true, 1.5e9, "", "C:/Games/Minecraft/MinecraftLauncher.exe", ago(2))
  ];

  var favorites = { "steam:1245620": true, "steam:1086940": true };
  var hidden = {};

  function overlay(list) {
    return list.map(function (it) {
      var c = Object.assign({}, it);
      c.favorite = !!favorites[it.uid];
      c.hidden = !!hidden[it.uid];
      c.art = Object.assign({}, it.art);
      return c;
    });
  }
  function delay(v, ms) { return new Promise(function (r) { setTimeout(function () { r(v); }, ms || 120); }); }
  function find(uid) { for (var i = 0; i < games.length; i++) if (games[i].uid === uid) return games[i]; return null; }

  window.__VECTOR_MOCK__ = function (p) {
    switch (p.cmd) {
      case "getSettings":
        return delay({ scanOnLaunch: true, stores: { steam: true, epic: true, gog: true }, defaultSort: "recent" });
      case "setSettings":
        return delay({ ok: true });
      case "getStores":
        return delay([
          { id: "steam", installed: true, path: "C:/Program Files (x86)/Steam", count: 9, enabled: true },
          { id: "epic", installed: true, path: "C:/ProgramData/Epic/EpicGamesLauncher", count: 2, enabled: true },
          { id: "gog", installed: false, path: "", count: 0, enabled: true },
          { id: "xbox", installed: false, comingSoon: true, count: 0, enabled: false },
          { id: "ea", installed: false, comingSoon: true, count: 0, enabled: false },
          { id: "ubisoft", installed: false, comingSoon: true, count: 0, enabled: false },
          { id: "battlenet", installed: false, comingSoon: true, count: 0, enabled: false }
        ], 300);
      case "getLibrary":
        return delay(overlay(games));
      case "scan":
        return delay(overlay(games), 900);
      case "launch":
        return delay({ ok: true });
      case "showInFolder":
        return delay({ ok: true });
      case "setFavorite":
        if (p.value) favorites[p.uid] = true; else delete favorites[p.uid];
        return delay({ ok: true });
      case "setHidden":
        if (p.value) hidden[p.uid] = true; else delete hidden[p.uid];
        return delay({ ok: true });
      case "addManualGame": {
        var inp = p.input || {};
        var ng = g("manual:" + (now + games.length), "manual", "m" + games.length,
          inp.name || "Untitled game", true, 0, inp.art || "", inp.executable || "", now);
        games.push(ng);
        return delay(overlay([ng])[0]);
      }
      case "removeManualGame":
        games = games.filter(function (x) { return x.uid !== p.uid; });
        return delay({ ok: true });
      case "pickExecutable":
        return delay("C:/Games/Sample/sample.exe", 200);
      case "pickImage":
        return delay("", 200);
      case "window":
        return delay({ ok: true });
      default:
        return Promise.reject(new Error("Unknown command: " + p.cmd));
    }
  };
})();
