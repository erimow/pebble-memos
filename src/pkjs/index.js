var MEMOS_URL = "https://notes.erimow.com/api/v1/memos";
var MEMOS_TOKEN = "eyJhbGciOiJIUzI1NiIsImtpZCI6InYxIiwidHlwIjoiSldUIn0.eyJuYW1lIjoiZXJpbW93IiwiaXNzIjoibWVtb3MiLCJzdWIiOiIxIiwiYXVkIjpbInVzZXIuYWNjZXNzLXRva2VuIl0sImlhdCI6MTc2NzcyNjAxM30.i3HuB3z-5pfgY1kJ3m9St7zHEi_VxrHNq2jykpZnMj8";

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
    var finalMemo = parseMemo(memoText);
    sendMemo(finalMemo);
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
  xhr.open("POST", MEMOS_URL, true);
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.setRequestHeader("Authorization", "Bearer " + MEMOS_TOKEN);

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

