/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(1);
	module.exports = __webpack_require__(2);


/***/ }),
/* 1 */
/***/ (function(module, exports) {

	(function(p) {
	  if (!p === undefined) {
	    console.error('Pebble object not found!?');
	    return;
	  }
	
	  // Aliases:
	  p.on = p.addEventListener;
	  p.off = p.removeEventListener;
	
	  // For Android (WebView-based) pkjs, print stacktrace for uncaught errors:
	  if (typeof window !== 'undefined' && window.addEventListener) {
	    window.addEventListener('error', function(event) {
	      if (event.error && event.error.stack) {
	        console.error('' + event.error + '\n' + event.error.stack);
	      }
	    });
	  }
	
	})(Pebble);


/***/ }),
/* 2 */
/***/ (function(module, exports) {

	function getSettings() {
	  var s = localStorage.getItem("memos_settings");
	  if (!s) return null;
	  return JSON.parse(s);
	}
	
	
	
	var AUTO_TAGS = ["#pebble"]; // ["#separate, #by, #commas, #for, #extra, #tags"]
	
	
	/* ---------- APPMESSAGE KEYS (MUST MATCH C) ---------- */
	var KEY_MEMO_CHUNK = 0;
	var KEY_MEMO_DONE  = 1; // sent by C, not used here
	var KEY_MEMO_OK    = 2;
	var KEY_MEMO_FAIL  = 3;
	
	/* ---------- STATE ---------- */
	var isSending = false;
	
	/* =========================================================
	 * Memo parser
	 * ========================================================= */
	function parseMemo(text) {
	  var tags = AUTO_TAGS.slice();
	  var content = text;
	
	  /* ---- Replace special commands ---- */
	  content = content.replace(/check box[\s,.]*/gi, "- [ ] ");
	  content = content.replace(/checkbox[\s,.]*/gi, "- [ ] ");
	  content = content.replace(/new line[\s,.]*/gi, "\n");
	  content = content.replace(/title big[\s,.]*/gi, "# ");
	  content = content.replace(/title medium[\s,.]*/gi, "## ");
	  content = content.replace(/title small[\s,.]*/gi, "### ");
	
	  /* ---- Extract spoken hashtags ---- */
	  var tagRegex = /hashtag\s+(\w+)/gi;
	  var match;
	  while ((match = tagRegex.exec(text)) !== null) {
	    tags.push("#" + match[1].toLowerCase());
	  }
	
	  content = content.replace(tagRegex, "").trim();
	
	  return content + "\n\n" + tags.join(" ");
	}
	
	/* =========================================================
	 * AppMessage handler
	 * ========================================================= */
	Pebble.addEventListener("appmessage", function (e) {
	  console.log("Memos: Raw payload:", JSON.stringify(e.payload));
	
	var chunk = e.payload["KEY_MEMO_CHUNK"];
	if (typeof chunk !== "string") {
	  console.log("Memos: KEY_MEMO_CHUNK missing or not string", chunk);
	  return;
	}
	
	  if (!e.payload || typeof e.payload["KEY_MEMO_CHUNK"] !== "string") {
	    console.log("Memos: Invalid payload");
	    return;
	  }
	
	  if (isSending) {
	    console.log("Memos: Already sending, ignoring");
	    return;
	  }
	
	  var memoText = e.payload["KEY_MEMO_CHUNK"];
	  console.log("Memos: Received memo:", memoText);
	
	  isSending = true;
	
	  try {
	    // var finalMemo = parseMemo(memoText);
	    sendMemo(memoText);
	  } catch (err) {
	    console.log("Memos: Parse error", err);
	    isSending = false;
	    Pebble.sendAppMessage({ "KEY_MEMO_FAIL": 1 });
	  }
	});
	
	/* =========================================================
	 * Send memo to Memos server
	 * ========================================================= */
	function sendMemo(finalMemo) {
	  var xhr = new XMLHttpRequest();
	  var settings = getSettings();
	  if (!settings || !settings.url || !settings.token) {
	  Pebble.sendAppMessage({ "KEY_MEMO_FAIL": 1 });
	  return;
	}
	  AUTO_TAGS = settings.tags ? settings.tags.split(/\s+/) : ["#pebble"];
	  finalMemo = parseMemo(finalMemo);
	  xhr.open("POST", settings.url, true);
	  xhr.setRequestHeader("Content-Type", "application/json");
	  xhr.setRequestHeader("Authorization", "Bearer " + settings.token);
	
	  xhr.onload = function () {
	    isSending = false;
	
	    if (xhr.status >= 200 && xhr.status < 300) {
	      console.log("Memos: Sent successfully");
	      Pebble.sendAppMessage({ "KEY_MEMO_OK": 1 });
	    } else {
	      console.log("Memos: Server error", xhr.status);
	      Pebble.sendAppMessage({ "KEY_MEMO_FAIL": 1 });
	    }
	  };
	
	  xhr.onerror = function () {
	    isSending = false;
	    console.log("Memos: Network error");
	    Pebble.sendAppMessage({ "KEY_MEMO_FAIL": 1 });
	  };
	
	  xhr.send(JSON.stringify({ content: finalMemo }));
	}
	
	/* =========================================================
	 * Lifecycle
	 * ========================================================= */
	Pebble.addEventListener("ready", function () {
	  console.log("Memos: PebbleKit JS ready");
	});
	
	
	Pebble.addEventListener("showConfiguration", function() {
	    console.log("Memos: Opening config page");
	  Pebble.openURL("https://erimow.github.io/memos-config/config.html");
	});
	
	Pebble.addEventListener("webviewclosed", function(e) {
	  if (!e.response) return;
	
	  try {
	    var settings = JSON.parse(decodeURIComponent(e.response));
	    localStorage.setItem("memos_settings", JSON.stringify(settings));
	    console.log("Memos: Settings saved", settings);
	  } catch (err) {
	    console.log("Memos: Failed to parse settings", err);
	  }
	});
	


/***/ })
/******/ ]);
//# sourceMappingURL=pebble-js-app.js.map