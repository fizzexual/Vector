// bridge.js — wraps CEF's window.cefQuery into a promise API.
// In a normal browser (no CEF), it falls back to window.__VECTOR_MOCK__ so the
// UI is fully testable. Vector.exe serves these same files over vector://app/.
(function () {
  "use strict";

  var native = typeof window.cefQuery === "function";

  function nativeCall(payload) {
    return new Promise(function (resolve, reject) {
      window.cefQuery({
        request: JSON.stringify(payload),
        persistent: false,
        onSuccess: function (resp) {
          try { resolve(resp ? JSON.parse(resp) : null); }
          catch (e) { resolve(resp); }
        },
        onFailure: function (code, msg) {
          reject(new Error(msg || ("cefQuery error " + code)));
        }
      });
    });
  }

  function call(payload) {
    if (native) return nativeCall(payload);
    if (typeof window.__VECTOR_MOCK__ === "function") return window.__VECTOR_MOCK__(payload);
    return Promise.reject(new Error("No Vector backend available"));
  }

  window.vector = {
    isNative: native,
    scan: function () { return call({ cmd: "scan" }); },
    getLibrary: function () { return call({ cmd: "getLibrary" }); },
    getSettings: function () { return call({ cmd: "getSettings" }); },
    setSettings: function (patch) { return call({ cmd: "setSettings", patch: patch }); },
    getStores: function () { return call({ cmd: "getStores" }); },
    launch: function (uid) { return call({ cmd: "launch", uid: uid }); },
    showInFolder: function (uid) { return call({ cmd: "showInFolder", uid: uid }); },
    setFavorite: function (uid, value) { return call({ cmd: "setFavorite", uid: uid, value: value }); },
    setHidden: function (uid, value) { return call({ cmd: "setHidden", uid: uid, value: value }); },
    addManualGame: function (input) { return call({ cmd: "addManualGame", input: input }); },
    removeManualGame: function (uid) { return call({ cmd: "removeManualGame", uid: uid }); },
    pickExecutable: function () { return call({ cmd: "pickExecutable" }); },
    pickImage: function () { return call({ cmd: "pickImage" }); },
    windowCommand: function (action) { return call({ cmd: "window", action: action }); }
  };
})();
