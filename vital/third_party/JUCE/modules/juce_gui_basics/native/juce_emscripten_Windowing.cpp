namespace juce
{

static int numAlwaysOnTopPeers = 0;
bool juce_areThereAnyAlwaysOnTopWindows()  { return numAlwaysOnTopPeers > 0; }

namespace WebKeys
{
    static constexpr int extended = 0x10000000;
}

const int KeyPress::spaceKey              = 0x20;
const int KeyPress::returnKey             = 0x0d;
const int KeyPress::escapeKey             = 0x1b;
const int KeyPress::backspaceKey          = 0x08;
const int KeyPress::leftKey               = 0x51 | WebKeys::extended;
const int KeyPress::rightKey              = 0x53 | WebKeys::extended;
const int KeyPress::upKey                 = 0x52 | WebKeys::extended;
const int KeyPress::downKey               = 0x54 | WebKeys::extended;
const int KeyPress::pageUpKey             = 0x55 | WebKeys::extended;
const int KeyPress::pageDownKey           = 0x56 | WebKeys::extended;
const int KeyPress::endKey                = 0x57 | WebKeys::extended;
const int KeyPress::homeKey               = 0x50 | WebKeys::extended;
const int KeyPress::insertKey             = 0x63 | WebKeys::extended;
const int KeyPress::deleteKey             = 0xff | WebKeys::extended;
const int KeyPress::tabKey                = 0x09;
const int KeyPress::F1Key                 = 0xbe | WebKeys::extended;
const int KeyPress::F2Key                 = 0xbf | WebKeys::extended;
const int KeyPress::F3Key                 = 0xc0 | WebKeys::extended;
const int KeyPress::F4Key                 = 0xc1 | WebKeys::extended;
const int KeyPress::F5Key                 = 0xc2 | WebKeys::extended;
const int KeyPress::F6Key                 = 0xc3 | WebKeys::extended;
const int KeyPress::F7Key                 = 0xc4 | WebKeys::extended;
const int KeyPress::F8Key                 = 0xc5 | WebKeys::extended;
const int KeyPress::F9Key                 = 0xc6 | WebKeys::extended;
const int KeyPress::F10Key                = 0xc7 | WebKeys::extended;
const int KeyPress::F11Key                = 0xc8 | WebKeys::extended;
const int KeyPress::F12Key                = 0xc9 | WebKeys::extended;
const int KeyPress::F13Key                = 0xca | WebKeys::extended;
const int KeyPress::F14Key                = 0xcb | WebKeys::extended;
const int KeyPress::F15Key                = 0xcc | WebKeys::extended;
const int KeyPress::F16Key                = 0xcd | WebKeys::extended;
const int KeyPress::F17Key                = 0xce | WebKeys::extended;
const int KeyPress::F18Key                = 0xcf | WebKeys::extended;
const int KeyPress::F19Key                = 0xd0 | WebKeys::extended;
const int KeyPress::F20Key                = 0xd1 | WebKeys::extended;
const int KeyPress::F21Key                = 0xd2 | WebKeys::extended;
const int KeyPress::F22Key                = 0xd3 | WebKeys::extended;
const int KeyPress::F23Key                = 0xd4 | WebKeys::extended;
const int KeyPress::F24Key                = 0xd5 | WebKeys::extended;
const int KeyPress::F25Key                = 0xd6 | WebKeys::extended;
const int KeyPress::F26Key                = 0xd7 | WebKeys::extended;
const int KeyPress::F27Key                = 0xd8 | WebKeys::extended;
const int KeyPress::F28Key                = 0xd9 | WebKeys::extended;
const int KeyPress::F29Key                = 0xda | WebKeys::extended;
const int KeyPress::F30Key                = 0xdb | WebKeys::extended;
const int KeyPress::F31Key                = 0xdc | WebKeys::extended;
const int KeyPress::F32Key                = 0xdd | WebKeys::extended;
const int KeyPress::F33Key                = 0xde | WebKeys::extended;
const int KeyPress::F34Key                = 0xdf | WebKeys::extended;
const int KeyPress::F35Key                = 0xe0 | WebKeys::extended;
const int KeyPress::numberPad0            = 0xb0 | WebKeys::extended;
const int KeyPress::numberPad1            = 0xb1 | WebKeys::extended;
const int KeyPress::numberPad2            = 0xb2 | WebKeys::extended;
const int KeyPress::numberPad3            = 0xb3 | WebKeys::extended;
const int KeyPress::numberPad4            = 0xb4 | WebKeys::extended;
const int KeyPress::numberPad5            = 0xb5 | WebKeys::extended;
const int KeyPress::numberPad6            = 0xb6 | WebKeys::extended;
const int KeyPress::numberPad7            = 0xb7 | WebKeys::extended;
const int KeyPress::numberPad8            = 0xb8 | WebKeys::extended;
const int KeyPress::numberPad9            = 0xb9 | WebKeys::extended;
const int KeyPress::numberPadAdd          = 0xab | WebKeys::extended;
const int KeyPress::numberPadSubtract     = 0xad | WebKeys::extended;
const int KeyPress::numberPadMultiply     = 0xaa | WebKeys::extended;
const int KeyPress::numberPadDivide       = 0xaf | WebKeys::extended;
const int KeyPress::numberPadSeparator    = 0xac | WebKeys::extended;
const int KeyPress::numberPadDecimalPoint = 0xae | WebKeys::extended;
const int KeyPress::numberPadEquals       = 0xbd | WebKeys::extended;
const int KeyPress::numberPadDelete       = 0x9f | WebKeys::extended;
const int KeyPress::playKey               = 0x30 | WebKeys::extended | 0x01000000;
const int KeyPress::stopKey               = 0x31 | WebKeys::extended | 0x01000000;
const int KeyPress::fastForwardKey        = 0x32 | WebKeys::extended | 0x01000000;
const int KeyPress::rewindKey             = 0x33 | WebKeys::extended | 0x01000000;

EM_JS (void, juce_web_setup, (), {
    if (Module.juceWeb)
        return;

    var W = {};
    Module.juceWeb = W;
    W.peers = {};
    W.vx = 0; W.vy = 0;
    W.warpX = 0; W.warpY = 0;
    W.buttons = 0;
    W.capturePeer = 0;
    W.focusedPeer = 0;
    W.clipboard = "";
    W.pendingPaste = null;
    W.isMac = /Mac|iPhone|iPad|iPod/.test (navigator.platform || navigator.userAgent);
    W.dpr = function() { return window.devicePixelRatio || 1; };

    W.root = document.getElementById ("juce-root");
    if (! W.root)
    {
        W.root = document.createElement ("div");
        W.root.id = "juce-root";
        document.body.appendChild (W.root);
    }

    W.root.style.overflow = "hidden";
    W.lastW = 0;
    W.lastH = 0;
    W.touches = {};

    W.rootMetrics = function() {
        var r = W.root.getBoundingClientRect();
        var cw = W.root.clientWidth, ch = W.root.clientHeight;
        return {
            left: r.left, top: r.top,
            sx: (r.width > 0 && cw > 0) ? cw / r.width : 1,
            sy: (r.height > 0 && ch > 0) ? ch / r.height : 1
        };
    };

    W.localPoint = function (e) {
        var m = W.rootMetrics();
        return { x: (e.clientX - m.left) * m.sx, y: (e.clientY - m.top) * m.sy };
    };

    W.viewportSize = function() {
        var w = W.root.clientWidth, h = W.root.clientHeight;
        if (w > 0 && h > 0)
        {
            W.lastW = w;
            W.lastH = h;
        }
        if (W.lastW > 0 && W.lastH > 0)
            return { w: W.lastW, h: W.lastH };
        return { w: Math.max (1, Math.floor (window.innerWidth)), h: Math.max (1, Math.floor (window.innerHeight)) };
    };

    W.touchIndexFor = function (e, create) {
        if (W.touches.hasOwnProperty (e.pointerId))
            return W.touches[e.pointerId];
        if (! create)
            return -1;
        var used = {};
        for (var k in W.touches) used[W.touches[k]] = true;
        var idx = 0;
        while (used[idx]) idx++;
        W.touches[e.pointerId] = idx;
        return idx;
    };

    W.touchCount = function() {
        var n = 0;
        for (var k in W.touches) n++;
        return n;
    };

    W.sendTouch = function (id, type, e, idx) {
        var pt = W.localPoint (e);
        W.vx = pt.x;
        W.vy = pt.y;
        var others = W.touchCount() > ((type == 2 || type == 3) ? 0 : 1) ? 1 : 0;
        var flags = (type == 1) ? 16 : 0;
        Module._juce_web_touch (id, type, pt.x, pt.y, flags, W.keyMods (e), idx, others);
    };

    W.keyMods = function (e) {
        var m = 0;
        if (e.shiftKey) m |= 1;
        if (e.ctrlKey) m |= 2;
        if (e.altKey) m |= 4;
        if (e.metaKey) m |= 8;
        return m;
    };

    W.buttonFlags = function (b) {
        var f = 0;
        if (b & 1) f |= 16;
        if (b & 2) f |= 32;
        if (b & 4) f |= 64;
        return f;
    };

    W.updatePosition = function (e) {
        if (document.pointerLockElement)
        {
            W.vx += e.movementX || 0;
            W.vy += e.movementY || 0;
        }
        else
        {
            if (W.buttons == 0)
            {
                W.warpX = 0;
                W.warpY = 0;
            }

            var pt = W.localPoint (e);
            W.vx = pt.x + W.warpX;
            W.vy = pt.y + W.warpY;
        }
    };

    W.peerForEvent = function (e, fallback) {
        if (W.capturePeer && W.peers[W.capturePeer])
            return W.capturePeer;
        return fallback;
    };

    W.sendPointer = function (id, type, e) {
        var buttons = W.buttons;
        var mods = W.keyMods (e);
        Module._juce_web_pointer (id, type, W.vx, W.vy, W.buttonFlags (buttons), mods, e.pointerType == "touch" ? 1 : 0);
    };

    W.translateButton = function (e) {
        var b = e.button;
        if (b == 0 && W.isMac && e.ctrlKey)
            return 2;
        if (b == 1) return 4;
        if (b == 2) return 2;
        return 1;
    };

    W.createPeer = function (id, zIndex) {
        var div = document.createElement ("div");
        div.style.position = "absolute";
        div.style.left = "0px";
        div.style.top = "0px";
        div.style.width = "1px";
        div.style.height = "1px";
        div.style.overflow = "hidden";
        div.style.zIndex = zIndex;
        div.style.display = "none";
        div.style.touchAction = "none";
        div.style.userSelect = "none";
        div.style.webkitUserSelect = "none";
        div.style.outline = "none";
        div.tabIndex = -1;

        var canvas = document.createElement ("canvas");
        canvas.style.position = "absolute";
        canvas.style.left = "0px";
        canvas.style.top = "0px";
        canvas.style.width = "100%";
        canvas.style.height = "100%";
        canvas.style.pointerEvents = "none";
        canvas.width = 1;
        canvas.height = 1;
        div.appendChild (canvas);

        var peer = { id: id, div: div, canvas: canvas, ctx: canvas.getContext ("2d"), cursor: "default" };
        W.peers[id] = peer;

        div.addEventListener ("contextmenu", function (e) { e.preventDefault(); });

        div.addEventListener ("pointerdown", function (e) {
            e.preventDefault();
            if (e.pointerType == "touch")
            {
                var idx = W.touchIndexFor (e, true);
                try { div.setPointerCapture (e.pointerId); } catch (err) {}
                Module._juce_web_activate (id);
                W.sendTouch (id, 0, e, idx);
                W.sendTouch (id, 1, e, idx);
                if (W.onUserGesture) W.onUserGesture();
                return;
            }
            try { div.setPointerCapture (e.pointerId); } catch (err) {}
            W.capturePeer = id;
            W.buttons |= W.translateButton (e);
            W.updatePosition (e);
            Module._juce_web_activate (id);
            W.sendPointer (id, 1, e);
            if (W.onUserGesture) W.onUserGesture();
        });

        div.addEventListener ("pointermove", function (e) {
            if (e.pointerType == "touch")
            {
                var idx = W.touchIndexFor (e, false);
                if (idx >= 0)
                    W.sendTouch (id, 1, e, idx);
                return;
            }
            W.updatePosition (e);
            var target = W.peerForEvent (e, id);
            if (W.buttons == 0 && target != id)
                target = id;
            W.sendPointer (target, 0, e);
        });

        var release = function (e) {
            if (e.pointerType == "touch")
            {
                var idx = W.touchIndexFor (e, false);
                if (idx < 0)
                    return;
                delete W.touches[e.pointerId];
                try { div.releasePointerCapture (e.pointerId); } catch (err) {}
                W.sendTouch (id, 2, e, idx);
                W.sendTouch (id, 3, e, idx);
                if (W.onUserGesture) W.onUserGesture();
                return;
            }
            W.updatePosition (e);
            var target = W.peerForEvent (e, id);
            W.buttons &= ~W.translateButton (e);
            if (e.type == "pointercancel")
                W.buttons = 0;
            if (W.buttons == 0)
            {
                W.capturePeer = 0;
                if (document.pointerLockElement)
                    document.exitPointerLock();
            }
            try { div.releasePointerCapture (e.pointerId); } catch (err) {}
            W.sendPointer (target, 2, e);
            if (e.pointerType == "touch")
                W.sendPointer (target, 3, e);
            if (W.onUserGesture) W.onUserGesture();
        };

        div.addEventListener ("pointerup", release);
        div.addEventListener ("pointercancel", release);

        div.addEventListener ("pointerleave", function (e) {
            if (e.pointerType == "touch")
                return;
            if (W.buttons != 0 || document.pointerLockElement)
                return;
            W.updatePosition (e);
            W.sendPointer (id, 3, e);
        });

        div.addEventListener ("wheel", function (e) {
            e.preventDefault();
            W.updatePosition (e);
            var scale = 1.0;
            if (e.deltaMode == 1) scale = 1.0 / 3.0;
            else if (e.deltaMode == 2) scale = 1.0;
            else scale = 1.0 / 100.0;
            var dx = -e.deltaX * scale * 0.2;
            var dy = -e.deltaY * scale * 0.2;
            var smooth = (e.deltaMode == 0 && Math.abs (e.deltaY) < 50 && e.deltaY != Math.round (e.deltaY / 100) * 100) ? 1 : 0;
            Module._juce_web_wheel (id, W.vx, W.vy, dx, dy, W.keyMods (e), smooth);
        }, { passive: false });

        div.addEventListener ("dragover", function (e) {
            e.preventDefault();
            if (e.dataTransfer) e.dataTransfer.dropEffect = "copy";
        });

        div.addEventListener ("drop", function (e) {
            e.preventDefault();
            W.updatePosition (e);
            var x = W.vx, y = W.vy;
            if (! e.dataTransfer)
                return;
            var files = e.dataTransfer.files;
            if (files && files.length > 0)
            {
                W.importFiles (files, "/tmp/juce-drop", function (paths) {
                    if (paths.length == 0)
                        return;
                    var joined = paths.join (String.fromCharCode (10));
                    var n = lengthBytesUTF8 (joined) + 1;
                    var p = _malloc (n);
                    stringToUTF8 (joined, p, n);
                    Module._juce_web_drop (id, x, y, p, 0);
                    _free (p);
                });
            }
            else
            {
                var text = e.dataTransfer.getData ("text/plain");
                if (text)
                {
                    var n = lengthBytesUTF8 (text) + 1;
                    var p = _malloc (n);
                    stringToUTF8 (text, p, n);
                    Module._juce_web_drop (id, x, y, p, 1);
                    _free (p);
                }
            }
        });

        W.root.appendChild (div);
    };

    W.importFiles = function (fileList, baseDir, done) {
        var files = Array.prototype.slice.call (fileList);
        var stamp = Date.now().toString (36) + Math.floor (Math.random() * 1e6).toString (36);
        var dir = baseDir + "/" + stamp;
        var paths = [];
        var remaining = files.length;
        if (remaining == 0) { done (paths); return; }

        var mkdirs = function (path) {
            var parts = path.split ("/");
            var current = "";
            for (var i = 1; i < parts.length; ++i)
            {
                current += "/" + parts[i];
                try { FS.mkdir (current); } catch (err) {}
            }
        };

        files.forEach (function (file) {
            var reader = new FileReader();
            reader.onload = function() {
                var rel = file.webkitRelativePath && file.webkitRelativePath.length > 0 ? file.webkitRelativePath : file.name;
                var full = dir + "/" + rel;
                mkdirs (full.substring (0, full.lastIndexOf ("/")));
                try {
                    FS.writeFile (full, new Uint8Array (reader.result));
                    paths.push (full);
                } catch (err) { console.error (err); }
                if (--remaining == 0) done (paths);
            };
            reader.onerror = function() { if (--remaining == 0) done (paths); };
            reader.readAsArrayBuffer (file);
        });
    };

    W.keyCodeFor = function (e) {
        var code = e.code || "";
        var ext = 0x10000000;
        if (code.indexOf ("Key") == 0 && code.length == 4)
            return code.toLowerCase().charCodeAt (3);
        if (code.indexOf ("Digit") == 0 && code.length == 6)
            return code.charCodeAt (5);
        var table = {
            "Space": 0x20, "Enter": 0x0d, "NumpadEnter": 0x0d, "Escape": 0x1b, "Backspace": 0x08, "Tab": 0x09,
            "Semicolon": 0x3b, "Quote": 0x27, "Comma": 0x2c, "Period": 0x2e, "Slash": 0x2f, "Backslash": 0x5c,
            "BracketLeft": 0x5b, "BracketRight": 0x5d, "Minus": 0x2d, "Equal": 0x3d, "Backquote": 0x60, "IntlBackslash": 0x5c,
            "ArrowLeft": ext | 0x51, "ArrowRight": ext | 0x53, "ArrowUp": ext | 0x52, "ArrowDown": ext | 0x54,
            "PageUp": ext | 0x55, "PageDown": ext | 0x56, "End": ext | 0x57, "Home": ext | 0x50,
            "Insert": ext | 0x63, "Delete": ext | 0xff,
            "Numpad0": ext | 0xb0, "Numpad1": ext | 0xb1, "Numpad2": ext | 0xb2, "Numpad3": ext | 0xb3, "Numpad4": ext | 0xb4,
            "Numpad5": ext | 0xb5, "Numpad6": ext | 0xb6, "Numpad7": ext | 0xb7, "Numpad8": ext | 0xb8, "Numpad9": ext | 0xb9,
            "NumpadAdd": ext | 0xab, "NumpadSubtract": ext | 0xad, "NumpadMultiply": ext | 0xaa, "NumpadDivide": ext | 0xaf,
            "NumpadDecimal": ext | 0xae, "NumpadEqual": ext | 0xbd, "NumpadComma": ext | 0xac,
            "MediaPlayPause": ext | 0x01000030, "MediaStop": ext | 0x01000031,
            "MediaTrackNext": ext | 0x01000032, "MediaTrackPrevious": ext | 0x01000033
        };
        if (table.hasOwnProperty (code))
            return table[code];
        var f = /^F([0-9]+)$/.exec (code);
        if (f)
            return ext | (0xbe + parseInt (f[1]) - 1);
        if (e.key && e.key.length == 1)
            return e.key.toLowerCase().charCodeAt (0);
        return 0;
    };

    W.isModifierKey = function (e) {
        return e.key == "Shift" || e.key == "Control" || e.key == "Alt" || e.key == "Meta" || e.key == "OS" || e.key == "AltGraph";
    };

    W.textCharFor = function (e) {
        if (e.key && e.key.length > 0)
        {
            var c = e.key.codePointAt (0);
            if (String.fromCodePoint (c) == e.key)
                return c;
        }
        if (e.key == "Enter") return 0x0d;
        if (e.key == "Tab") return 0x09;
        return 0;
    };

    W.dispatchKey = function (e, isDown) {
        var mods = W.keyMods (e);
        if (W.isModifierKey (e))
        {
            Module._juce_web_modifiers (mods);
            return false;
        }
        var keyCode = W.keyCodeFor (e);
        return Module._juce_web_key (isDown ? 1 : 0, keyCode, isDown ? W.textCharFor (e) : 0, mods, e.repeat ? 1 : 0) != 0;
    };

    W.isTextFieldTarget = function (e) {
        var t = e.target;
        if (Module.flwebHost && ! Module.flwebHost.wantsKeys (e))
            return true;
        return t && t !== document.body && (t.tagName == "INPUT" || t.tagName == "TEXTAREA" || t.isContentEditable);
    };

    window.addEventListener ("keydown", function (e) {
        if (W.isTextFieldTarget (e))
            return;
        if (W.onUserGesture) W.onUserGesture();
        var command = e.ctrlKey || e.metaKey;
        if (command && ! e.altKey && (e.code == "KeyV"))
        {
            var ev = { key: e.key, code: e.code, shiftKey: e.shiftKey, ctrlKey: e.ctrlKey, altKey: e.altKey, metaKey: e.metaKey, repeat: e.repeat };
            if (W.pendingPaste) clearTimeout (W.pendingPaste.timer);
            W.pendingPaste = { ev: ev, timer: setTimeout (function() {
                var p = W.pendingPaste;
                W.pendingPaste = null;
                if (p) W.dispatchKey (p.ev, true);
            }, 150) };
            return;
        }
        var handled = W.dispatchKey (e, true);
        var nav = ["Tab", "Backspace", "Space", "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "PageUp", "PageDown", "Home", "End", "Quote", "Slash"];
        if (handled || nav.indexOf (e.code) >= 0 || (! command && ! e.altKey && e.key && e.key.length == 1))
            e.preventDefault();
    }, true);

    window.addEventListener ("keyup", function (e) {
        if (W.isTextFieldTarget (e))
            return;
        W.dispatchKey (e, false);
    }, true);

    window.addEventListener ("paste", function (e) {
        if (W.isTextFieldTarget (e))
            return;
        var text = e.clipboardData ? e.clipboardData.getData ("text/plain") : "";
        W.clipboard = text || "";
        var p = W.pendingPaste;
        W.pendingPaste = null;
        if (p)
        {
            clearTimeout (p.timer);
            W.dispatchKey (p.ev, true);
        }
        e.preventDefault();
    });

    window.addEventListener ("blur", function() {
        W.buttons = 0;
        W.touches = {};
        Module._juce_web_window_focus (0);
    });

    window.addEventListener ("focus", function() {
        Module._juce_web_window_focus (1);
    });

    var resizeTimer = null;
    var onResize = function() {
        Module._juce_web_resized();
    };
    window.addEventListener ("resize", onResize);
    if (window.ResizeObserver)
        new ResizeObserver (function() {
            if (W.root.clientWidth > 0 && W.root.clientHeight > 0 && (W.root.clientWidth != W.lastW || W.root.clientHeight != W.lastH))
                onResize();
        }).observe (W.root);
    if (window.visualViewport)
        window.visualViewport.addEventListener ("resize", onResize);

    var watchDpr = function() {
        var mq = window.matchMedia ("(resolution: " + W.dpr() + "dppx)");
        var handler = function() {
            Module._juce_web_resized();
            watchDpr();
        };
        if (mq.addEventListener)
            mq.addEventListener ("change", handler, { once: true });
    };
    watchDpr();

    var onFullscreenChange = function() {
        if (! (document.fullscreenElement || document.webkitFullscreenElement))
            Module._juce_web_fullscreen_exited();
    };
    document.addEventListener ("fullscreenchange", onFullscreenChange);
    document.addEventListener ("webkitfullscreenchange", onFullscreenChange);

    document.addEventListener ("pointerlockchange", function() {
        Module._juce_web_pointer_lock (document.pointerLockElement ? 1 : 0);
    });
});

EM_JS (void, juce_web_createPeer, (int id, int zIndex), {
    Module.juceWeb.createPeer (id, zIndex);
});

EM_JS (void, juce_web_destroyPeer, (int id), {
    var W = Module.juceWeb;
    var p = W.peers[id];
    if (! p) return;
    if (p.div.parentNode) p.div.parentNode.removeChild (p.div);
    delete W.peers[id];
    if (W.capturePeer == id) W.capturePeer = 0;
});

EM_JS (void, juce_web_setPeerBounds, (int id, int x, int y, int w, int h, int pw, int ph), {
    var p = Module.juceWeb.peers[id];
    if (! p) return;
    p.div.style.left = x + "px";
    p.div.style.top = y + "px";
    p.div.style.width = w + "px";
    p.div.style.height = h + "px";
    if (p.canvas.width != pw || p.canvas.height != ph)
    {
        p.canvas.width = pw;
        p.canvas.height = ph;
    }
});

EM_JS (void, juce_web_setPeerVisible, (int id, int visible), {
    var p = Module.juceWeb.peers[id];
    if (! p) return;
    p.div.style.display = visible ? "block" : "none";
});

EM_JS (void, juce_web_setPeerZ, (int id, int z), {
    var p = Module.juceWeb.peers[id];
    if (! p) return;
    p.div.style.zIndex = z;
});

EM_JS (void, juce_web_setPeerAlpha, (int id, float alpha), {
    var p = Module.juceWeb.peers[id];
    if (! p) return;
    p.div.style.opacity = alpha;
});

EM_JS (void, juce_web_blit, (int id, const unsigned char* pixels, int x, int y, int w, int h), {
    var p = Module.juceWeb.peers[id];
    if (! p || w <= 0 || h <= 0) return;
    var len = w * h * 4;
    var copy = new Uint8ClampedArray (len);
    copy.set (HEAPU8.subarray (pixels, pixels + len));
    p.ctx.putImageData (new ImageData (copy, w, h), x, y);
});

EM_JS (void, juce_web_setCursor, (int id, const char* css), {
    var W = Module.juceWeb;
    var p = W.peers[id];
    if (! p) return;
    var cursor = UTF8ToString (css);
    p.cursor = cursor;
    p.div.style.cursor = cursor;
    if (cursor == "none")
    {
        if (W.buttons != 0 && ! document.pointerLockElement && p.div.requestPointerLock)
        {
            try {
                var r = p.div.requestPointerLock();
                if (r && r.catch) r.catch (function() {});
            } catch (err) {}
        }
    }
    else if (document.pointerLockElement)
    {
        document.exitPointerLock();
    }
});

EM_JS (void, juce_web_setRawMouse, (float x, float y), {
    var W = Module.juceWeb;
    if (! W) return;
    if (! document.pointerLockElement)
    {
        W.warpX += x - W.vx;
        W.warpY += y - W.vy;
    }
    W.vx = x;
    W.vy = y;
});

EM_JS (float, juce_web_mouseX, (), {
    return Module.juceWeb ? Module.juceWeb.vx : 0;
});

EM_JS (float, juce_web_mouseY, (), {
    return Module.juceWeb ? Module.juceWeb.vy : 0;
});

EM_JS (int, juce_web_viewportWidth, (), {
    var W = Module.juceWeb;
    return W ? W.viewportSize().w : Math.max (1, Math.floor (window.innerWidth));
});

EM_JS (int, juce_web_viewportHeight, (), {
    var W = Module.juceWeb;
    return W ? W.viewportSize().h : Math.max (1, Math.floor (window.innerHeight));
});

EM_JS (double, juce_web_devicePixelRatio, (), {
    return window.devicePixelRatio || 1;
});

EM_JS (void, juce_web_focusPeerElement, (int id), {
    var p = Module.juceWeb.peers[id];
    if (p && document.activeElement !== p.div)
    {
        try { p.div.focus ({ preventScroll: true }); } catch (err) {}
    }
});

EM_JS (void, juce_web_copyToClipboard, (const char* text), {
    var s = UTF8ToString (text);
    Module.juceWeb.clipboard = s;
    try {
        if (navigator.clipboard && navigator.clipboard.writeText)
            navigator.clipboard.writeText (s).catch (function() {});
    } catch (err) {}
});

EM_JS (char*, juce_web_getClipboard, (), {
    var s = Module.juceWeb ? Module.juceWeb.clipboard : "";
    var n = lengthBytesUTF8 (s) + 1;
    var p = _malloc (n);
    stringToUTF8 (s, p, n);
    return p;
});

EM_JS (void, juce_web_alert, (const char* title, const char* message), {
    var t = UTF8ToString (title);
    var m = UTF8ToString (message);
    window.alert ((t.length > 0 ? t + String.fromCharCode (10, 10) : "") + m);
});

EM_JS (int, juce_web_confirm, (const char* title, const char* message), {
    var t = UTF8ToString (title);
    var m = UTF8ToString (message);
    return window.confirm ((t.length > 0 ? t + String.fromCharCode (10, 10) : "") + m) ? 1 : 0;
});

EM_JS (void, juce_web_setFullscreen, (int enable), {
    try {
        if (enable)
        {
            var el = document.documentElement;
            var req = el.requestFullscreen || el.webkitRequestFullscreen;
            if (req && ! (document.fullscreenElement || document.webkitFullscreenElement))
            {
                var r = req.call (el);
                if (r && r.catch) r.catch (function() {});
            }
        }
        else if (document.fullscreenElement || document.webkitFullscreenElement)
        {
            var exit = document.exitFullscreen || document.webkitExitFullscreen;
            if (exit) exit.call (document);
        }
    } catch (err) {}
});

EM_JS (int, juce_web_hasFocus, (), {
    return document.hasFocus() ? 1 : 0;
});

class EmscriptenComponentPeer;

namespace WebPeers
{
    static std::map<int, EmscriptenComponentPeer*> peers;
    static int nextId = 1;
    static int highestZ = 10;
    static int focusedPeerId = 0;
    static std::set<int> keysDown;
    static bool windowHasFocus = true;
    static String clipboardText;

    static EmscriptenComponentPeer* find (int id)
    {
        auto it = peers.find (id);
        return it != peers.end() ? it->second : nullptr;
    }

    static int normaliseKey (int keyCode)
    {
        if (keyCode >= 'A' && keyCode <= 'Z')
            return keyCode - 'A' + 'a';
        return keyCode;
    }

    static ModifierKeys keyFlagsToModifiers (int keyMods)
    {
        int flags = 0;

        if ((keyMods & 1) != 0) flags |= ModifierKeys::shiftModifier;
        if ((keyMods & 2) != 0) flags |= ModifierKeys::ctrlModifier;
        if ((keyMods & 4) != 0) flags |= ModifierKeys::altModifier;
        if ((keyMods & 8) != 0) flags |= ModifierKeys::ctrlModifier;

        return ModifierKeys (flags);
    }

    static void setKeyModifiers (int keyMods)
    {
        auto mouseFlags = ModifierKeys::currentModifiers.getRawFlags() & ModifierKeys::allMouseButtonModifiers;
        ModifierKeys::currentModifiers = keyFlagsToModifiers (keyMods).withFlags (mouseFlags);
    }

    static void setMouseButtons (int buttonFlags)
    {
        int flags = 0;

        if ((buttonFlags & 16) != 0) flags |= ModifierKeys::leftButtonModifier;
        if ((buttonFlags & 32) != 0) flags |= ModifierKeys::rightButtonModifier;
        if ((buttonFlags & 64) != 0) flags |= ModifierKeys::middleButtonModifier;

        ModifierKeys::currentModifiers = ModifierKeys::currentModifiers.withoutMouseButtons().withFlags (flags);
    }
}

class EmscriptenComponentPeer  : public ComponentPeer
{
public:
    EmscriptenComponentPeer (Component& comp, int windowStyleFlags)
        : ComponentPeer (comp, windowStyleFlags),
          peerId (WebPeers::nextId++),
          isAlwaysOnTop (comp.isAlwaysOnTop())
    {
        juce_web_setup();

        if (isAlwaysOnTop)
            ++numAlwaysOnTopPeers;

        zIndex = nextZIndex();
        juce_web_createPeer (peerId, zIndex);
        WebPeers::peers[peerId] = this;

        EmscriptenEventLoop::addFrameCallback (this, [this] (double) { performAnyPendingRepaintsNow(); });

        getNativeRealtimeModifiers = []() -> ModifierKeys { return ModifierKeys::currentModifiers; };
    }

    ~EmscriptenComponentPeer() override
    {
        EmscriptenEventLoop::removeFrameCallback (this);
        WebPeers::peers.erase (peerId);

        if (WebPeers::focusedPeerId == peerId)
            WebPeers::focusedPeerId = 0;

        juce_web_destroyPeer (peerId);

        if (isAlwaysOnTop)
            --numAlwaysOnTopPeers;
    }

    int getPeerId() const noexcept   { return peerId; }

    void* getNativeHandle() const override
    {
        return (void*) (pointer_sized_int) peerId;
    }

    void setVisible (bool shouldBeVisible) override
    {
        visible = shouldBeVisible;
        juce_web_setPeerVisible (peerId, shouldBeVisible ? 1 : 0);

        if (shouldBeVisible)
            repaint (bounds.withZeroOrigin());
    }

    void setTitle (const String&) override {}

    void setBounds (const Rectangle<int>& newBounds, bool isNowFullScreen) override
    {
        auto oldBounds = bounds;
        bounds = newBounds.withSize (jmax (1, newBounds.getWidth()), jmax (1, newBounds.getHeight()));
        fullScreen = isNowFullScreen;

        updateScale();
        applyBounds();

        if (oldBounds.getWidth() != bounds.getWidth() || oldBounds.getHeight() != bounds.getHeight())
        {
            backBuffer = Image();
            dirtyRegion.add (bounds.withZeroOrigin());
        }

        WeakReference<Component> deletionChecker (&component);
        handleMovedOrResized();

        if (deletionChecker != nullptr && oldBounds.getWidth() != bounds.getWidth())
            repaint (bounds.withZeroOrigin());
    }

    Rectangle<int> getBounds() const override   { return bounds; }

    Point<float> localToGlobal (Point<float> relativePosition) override
    {
        return relativePosition + bounds.getPosition().toFloat();
    }

    Point<float> globalToLocal (Point<float> screenPosition) override
    {
        return screenPosition - bounds.getPosition().toFloat();
    }

    using ComponentPeer::localToGlobal;
    using ComponentPeer::globalToLocal;

    void setMinimised (bool shouldBeMinimised) override
    {
        minimised = shouldBeMinimised;
        setVisible (! shouldBeMinimised);
    }

    bool isMinimised() const override   { return minimised; }

    void setFullScreen (bool shouldBeFullScreen) override
    {
        if (shouldBeFullScreen == fullScreen)
            return;

        if (shouldBeFullScreen)
        {
            lastNonFullscreenBounds = bounds;
            auto area = Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            setBounds (ScalingHelpers::scaledScreenPosToUnscaled (component, area), true);
        }
        else if (! lastNonFullscreenBounds.isEmpty())
        {
            setBounds (ScalingHelpers::scaledScreenPosToUnscaled (component, lastNonFullscreenBounds), false);
        }

        component.repaint();
    }

    bool isFullScreen() const override   { return fullScreen; }

    void setIcon (const Image&) override {}

    bool contains (Point<int> localPos, bool trueIfInAChildWindow) const override
    {
        if (! bounds.withZeroOrigin().contains (localPos))
            return false;

        for (int i = Desktop::getInstance().getNumComponents(); --i >= 0;)
        {
            auto* c = Desktop::getInstance().getComponent (i);

            if (c == &component)
                break;

            if (! c->isVisible())
                continue;

            if (auto* peer = c->getPeer())
                if (peer->contains (localPos + bounds.getPosition() - peer->getBounds().getPosition(), true))
                    return false;
        }

        ignoreUnused (trueIfInAChildWindow);
        return true;
    }

    BorderSize<int> getFrameSize() const override   { return {}; }

    bool setAlwaysOnTop (bool alwaysOnTop) override
    {
        if (alwaysOnTop != isAlwaysOnTop)
        {
            numAlwaysOnTopPeers += alwaysOnTop ? 1 : -1;
            isAlwaysOnTop = alwaysOnTop;
            zIndex = nextZIndex();
            juce_web_setPeerZ (peerId, zIndex);
        }

        return true;
    }

    void toFront (bool makeActive) override
    {
        zIndex = nextZIndex();
        juce_web_setPeerZ (peerId, zIndex);

        if (makeActive)
        {
            setVisible (true);
            grabFocus();
        }

        handleBroughtToFront();
    }

    void toBehind (ComponentPeer* other) override
    {
        if (auto* otherPeer = dynamic_cast<EmscriptenComponentPeer*> (other))
        {
            zIndex = otherPeer->zIndex - 1;
            juce_web_setPeerZ (peerId, zIndex);
        }
    }

    bool isFocused() const override
    {
        return WebPeers::focusedPeerId == peerId && WebPeers::windowHasFocus;
    }

    void grabFocus() override
    {
        if (WebPeers::focusedPeerId == peerId)
            return;

        auto* previous = WebPeers::find (WebPeers::focusedPeerId);
        WebPeers::focusedPeerId = peerId;

        if (previous != nullptr)
            previous->handleFocusLoss();

        handleFocusGain();
    }

    void textInputRequired (Point<int>, TextInputTarget&) override {}

    void repaint (const Rectangle<int>& area) override
    {
        dirtyRegion.add (area.getIntersection (bounds.withZeroOrigin()));
    }

    void performAnyPendingRepaintsNow() override
    {
        if (dirtyRegion.isEmpty() || ! visible || minimised)
            return;

        auto scale = currentScale;
        auto physicalW = jmax (1, roundToInt (bounds.getWidth() * scale));
        auto physicalH = jmax (1, roundToInt (bounds.getHeight() * scale));

        if (backBuffer.isNull() || backBuffer.getWidth() != physicalW || backBuffer.getHeight() != physicalH)
        {
            backBuffer = Image (Image::ARGB, physicalW, physicalH, true, SoftwareImageType());
            dirtyRegion = RectangleList<int> (bounds.withZeroOrigin());
        }

        RectangleList<int> physicalRegion;

        for (auto& r : dirtyRegion)
            physicalRegion.add ((r.toFloat() * (float) scale).getSmallestIntegerContainer()
                                  .getIntersection ({ physicalW, physicalH }));

        dirtyRegion.clear();

        auto totalArea = physicalRegion.getBounds();

        if (totalArea.isEmpty())
            return;

        for (auto& r : physicalRegion)
            backBuffer.clear (r);

        {
            auto context = component.getLookAndFeel().createGraphicsContext (backBuffer, {}, physicalRegion);
            context->addTransform (AffineTransform::scale ((float) scale));
            handlePaint (*context);
        }

        Image::BitmapData data (backBuffer, totalArea.getX(), totalArea.getY(),
                                totalArea.getWidth(), totalArea.getHeight(), Image::BitmapData::readOnly);

        auto w = totalArea.getWidth();
        auto h = totalArea.getHeight();
        rgba.resize ((size_t) (w * h * 4));

        auto* dest = rgba.data();
        auto opaque = component.isOpaque();

        for (int y = 0; y < h; ++y)
        {
            auto* src = data.getLinePointer (y);

            for (int x = 0; x < w; ++x)
            {
                auto* px = src + x * data.pixelStride;
                uint8 b = px[0], g = px[1], r = px[2], a = px[3];

                if (opaque)
                {
                    a = 255;
                }
                else if (a != 0 && a != 255)
                {
                    r = (uint8) jmin (255, (r * 255) / a);
                    g = (uint8) jmin (255, (g * 255) / a);
                    b = (uint8) jmin (255, (b * 255) / a);
                }

                dest[0] = r;
                dest[1] = g;
                dest[2] = b;
                dest[3] = a;
                dest += 4;
            }
        }

        juce_web_blit (peerId, rgba.data(), totalArea.getX(), totalArea.getY(), w, h);
    }

    void setAlpha (float newAlpha) override
    {
        juce_web_setPeerAlpha (peerId, newAlpha);
    }

    StringArray getAvailableRenderingEngines() override
    {
        return { "Software Renderer" };
    }

    double getPlatformScaleFactor() const noexcept override
    {
        return currentScale;
    }

    void handleScreenSizeChange() override
    {
        ComponentPeer::handleScreenSizeChange();
        updateScale();
        applyBounds();
        backBuffer = Image();
        dirtyRegion.add (bounds.withZeroOrigin());
    }

    void setCursorCss (const std::string& css)
    {
        if (css != currentCursor)
        {
            currentCursor = css;
            juce_web_setCursor (peerId, css.c_str());
        }
    }

    void handlePointer (int type, Point<float> globalPos, int buttonFlags, int keyMods, bool isTouch)
    {
        WebPeers::setKeyModifiers (keyMods);
        WebPeers::setMouseButtons (buttonFlags);

        auto local = globalToLocal (globalPos);
        auto time = Time::getMillisecondCounter();

        if (type == 3)
        {
            ModifierKeys::currentModifiers = ModifierKeys::currentModifiers.withoutMouseButtons();
            local = { -10000.0f, -10000.0f };
        }

        handleMouseEvent (isTouch ? MouseInputSource::InputSourceType::touch : MouseInputSource::InputSourceType::mouse,
                          local, ModifierKeys::currentModifiers,
                          isTouch ? 1.0f : MouseInputSource::invalidPressure,
                          MouseInputSource::invalidOrientation, time, {}, 0);
    }

    void handleTouch (int type, Point<float> globalPos, int buttonFlags, int keyMods, int touchIndex, bool othersDown)
    {
        WebPeers::setKeyModifiers (keyMods);
        WebPeers::setMouseButtons (buttonFlags);

        auto local = globalToLocal (globalPos);
        auto time = Time::getMillisecondCounter();
        auto mods = ModifierKeys::currentModifiers;

        if (type == 3)
        {
            mods = mods.withoutMouseButtons();
            local = { -10000.0f, -10000.0f };
        }

        handleMouseEvent (MouseInputSource::InputSourceType::touch, local, mods,
                          type == 2 || type == 3 ? 0.0f : 1.0f,
                          MouseInputSource::invalidOrientation, time, {}, touchIndex);

        if (othersDown)
            ModifierKeys::currentModifiers = ModifierKeys::currentModifiers.withoutMouseButtons().withFlags (ModifierKeys::leftButtonModifier);
        else if (type == 2 || type == 3)
            ModifierKeys::currentModifiers = ModifierKeys::currentModifiers.withoutMouseButtons();
    }

    void handleWheel (Point<float> globalPos, float dx, float dy, int keyMods, bool smooth)
    {
        WebPeers::setKeyModifiers (keyMods);

        MouseWheelDetails wheel;
        wheel.deltaX = dx;
        wheel.deltaY = dy;
        wheel.isReversed = false;
        wheel.isSmooth = smooth;
        wheel.isInertial = false;

        handleMouseWheel (MouseInputSource::InputSourceType::mouse, globalToLocal (globalPos),
                          Time::getMillisecondCounter(), wheel, 0);
    }

    void handleDrop (Point<float> globalPos, const StringArray& files, const String& text)
    {
        ComponentPeer::DragInfo info;
        info.files = files;
        info.text = text;
        info.position = globalToLocal (globalPos).roundToInt();

        WeakReference<Component> deletionChecker (&component);
        handleDragMove (info);

        if (deletionChecker != nullptr)
            handleDragDrop (info);
    }

private:
    static int nextZIndex()
    {
        return ++WebPeers::highestZ;
    }

    void updateScale()
    {
        auto newScale = Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale
                          / Desktop::getInstance().getGlobalScaleFactor();

        if (newScale <= 0.0)
            newScale = 1.0;

        if (! approximatelyEqual (newScale, currentScale))
        {
            currentScale = newScale;
            backBuffer = Image();
            dirtyRegion.add (bounds.withZeroOrigin());
            scaleFactorListeners.call ([&] (ScaleFactorListener& l) { l.nativeScaleFactorChanged (currentScale); });
        }
    }

    void applyBounds()
    {
        juce_web_setPeerBounds (peerId, bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                                jmax (1, roundToInt (bounds.getWidth() * currentScale)),
                                jmax (1, roundToInt (bounds.getHeight() * currentScale)));
    }

    const int peerId;
    Rectangle<int> bounds, lastNonFullscreenBounds;
    RectangleList<int> dirtyRegion;
    Image backBuffer;
    std::vector<uint8> rgba;
    std::string currentCursor = "default";
    double currentScale = 1.0;
    int zIndex = 0;
    bool fullScreen = false, minimised = false, visible = false, isAlwaysOnTop = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmscriptenComponentPeer)
};

}

using namespace juce;

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_pointer (int id, int type, float x, float y, int buttonFlags, int keyMods, int isTouch)
{
    if (auto* peer = WebPeers::find (id))
        peer->handlePointer (type, { x, y }, buttonFlags, keyMods, isTouch != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_touch (int id, int type, float x, float y, int buttonFlags, int keyMods, int touchIndex, int othersDown)
{
    if (auto* peer = WebPeers::find (id))
        peer->handleTouch (type, { x, y }, buttonFlags, keyMods, touchIndex, othersDown != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_wheel (int id, float x, float y, float dx, float dy, int keyMods, int smooth)
{
    if (auto* peer = WebPeers::find (id))
        peer->handleWheel ({ x, y }, dx, dy, keyMods, smooth != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_activate (int id)
{
    WebPeers::windowHasFocus = true;

    if (auto* peer = WebPeers::find (id))
    {
        auto& comp = peer->getComponent();

        if (comp.getWantsKeyboardFocus() || (peer->getStyleFlags() & ComponentPeer::windowIsTemporary) == 0)
            peer->grabFocus();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_modifiers (int keyMods)
{
    WebPeers::setKeyModifiers (keyMods);

    if (auto* peer = WebPeers::find (WebPeers::focusedPeerId))
        peer->handleModifierKeysChange();
}

static EmscriptenComponentPeer* findKeyTargetPeer()
{
    if (auto* peer = WebPeers::find (WebPeers::focusedPeerId))
        return peer;

    for (int i = Desktop::getInstance().getNumComponents(); --i >= 0;)
        if (auto* c = Desktop::getInstance().getComponent (i))
            if (c->isVisible())
                if (auto* peer = dynamic_cast<EmscriptenComponentPeer*> (c->getPeer()))
                    return peer;

    return nullptr;
}

extern "C" EMSCRIPTEN_KEEPALIVE int juce_web_key (int isDown, int keyCode, int textChar, int keyMods, int isRepeat)
{
    WebPeers::setKeyModifiers (keyMods);

    auto normalised = WebPeers::normaliseKey (keyCode);
    bool stateChanged = false;

    if (isDown)
        stateChanged = WebPeers::keysDown.insert (normalised).second;
    else
        stateChanged = WebPeers::keysDown.erase (normalised) > 0;

    auto* peer = findKeyTargetPeer();

    if (peer == nullptr)
        return 0;

    WeakReference<Component> deletionChecker (&peer->getComponent());
    bool handled = false;

    if (stateChanged || ! isDown)
        handled = peer->handleKeyUpOrDown (isDown != 0);

    if (isDown && deletionChecker != nullptr && keyCode != 0)
    {
        auto character = (juce_wchar) textChar;

        if (character < 0x20 && character != 0x09 && character != 0x0d && character != 0x08 && character != 0x1b)
            character = 0;

        handled = peer->handleKeyPress (KeyPress (keyCode, WebPeers::keyFlagsToModifiers (keyMods), character)) || handled;
    }

    ignoreUnused (isRepeat);
    return handled ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_window_focus (int hasFocus)
{
    WebPeers::windowHasFocus = hasFocus != 0;

    if (! hasFocus)
    {
        auto hadKeys = ! WebPeers::keysDown.empty();
        WebPeers::keysDown.clear();
        ModifierKeys::currentModifiers = ModifierKeys();

        if (auto* peer = WebPeers::find (WebPeers::focusedPeerId))
        {
            if (hadKeys)
                peer->handleKeyUpOrDown (false);
        }
    }
    else if (auto* peer = WebPeers::find (WebPeers::focusedPeerId))
    {
        if (Component::getCurrentlyFocusedComponent() == nullptr)
            peer->handleFocusGain();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_resized()
{
    const_cast<Displays&> (Desktop::getInstance().getDisplays()).refresh();

    for (auto& p : WebPeers::peers)
        p.second->handleScreenSizeChange();
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_pointer_lock (int)
{
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_fullscreen_exited()
{
    if (Desktop::getInstance().getKioskModeComponent() != nullptr)
        Desktop::getInstance().setKioskModeComponent (nullptr);
}

extern "C" EMSCRIPTEN_KEEPALIVE void juce_web_drop (int id, float x, float y, const char* data, int isText)
{
    if (auto* peer = WebPeers::find (id))
    {
        auto content = String::fromUTF8 (data);

        if (isText)
            peer->handleDrop ({ x, y }, {}, content);
        else
            peer->handleDrop ({ x, y }, StringArray::fromLines (content), {});
    }
}

namespace juce
{

ComponentPeer* Component::createNewPeer (int styleFlags, void*)
{
    return new EmscriptenComponentPeer (*this, styleFlags);
}

JUCE_API bool JUCE_CALLTYPE Process::isForegroundProcess()    { return juce_web_hasFocus() != 0; }
JUCE_API void JUCE_CALLTYPE Process::makeForegroundProcess()  {}
JUCE_API void JUCE_CALLTYPE Process::hide()                   {}

void Desktop::setKioskComponent (Component* comp, bool enableOrDisable, bool)
{
    juce_web_setFullscreen (enableOrDisable ? 1 : 0);

    if (enableOrDisable)
        comp->setBounds (getDisplays().getPrimaryDisplay()->totalArea);
}

void Displays::findDisplays (float masterScale)
{
    juce_web_setup();

    Display d;
    d.isMain = true;
    d.scale = juce_web_devicePixelRatio() * masterScale;
    d.dpi = 96.0 * d.scale;
    d.totalArea = { juce_web_viewportWidth(), juce_web_viewportHeight() };
    d.userArea = d.totalArea;
    d.topLeftPhysical = {};

    displays.clear();
    displays.add (d);
}

bool Desktop::canUseSemiTransparentWindows() noexcept   { return true; }

void Desktop::setScreenSaverEnabled (bool)  {}
bool Desktop::isScreenSaverEnabled()        { return true; }

double Desktop::getDefaultMasterScale()                             { return 1.0; }
Desktop::DisplayOrientation Desktop::getCurrentOrientation() const  { return upright; }
void Desktop::allowedOrientationsChanged()                          {}

bool MouseInputSource::SourceList::addSource()
{
    if (sources.size() < 11)
    {
        addSource (sources.size() == 0 ? 0 : sources.size() - 1,
                   sources.size() == 0 ? MouseInputSource::InputSourceType::mouse
                                       : MouseInputSource::InputSourceType::touch);
        return true;
    }

    return false;
}

bool MouseInputSource::SourceList::canUseTouch()
{
    return true;
}

Point<float> MouseInputSource::getCurrentRawMousePosition()
{
    return { juce_web_mouseX(), juce_web_mouseY() };
}

void MouseInputSource::setRawMousePosition (Point<float> newPosition)
{
    juce_web_setRawMouse (newPosition.x, newPosition.y);
}

struct WebCursor
{
    std::string css;
};

static std::string cursorCssForType (MouseCursor::StandardCursorType type)
{
    switch (type)
    {
        case MouseCursor::NoCursor:                     return "none";
        case MouseCursor::WaitCursor:                   return "wait";
        case MouseCursor::IBeamCursor:                  return "text";
        case MouseCursor::CrosshairCursor:              return "crosshair";
        case MouseCursor::CopyingCursor:                return "copy";
        case MouseCursor::PointingHandCursor:           return "pointer";
        case MouseCursor::DraggingHandCursor:           return "grab";
        case MouseCursor::LeftRightResizeCursor:        return "ew-resize";
        case MouseCursor::UpDownResizeCursor:           return "ns-resize";
        case MouseCursor::UpDownLeftRightResizeCursor:  return "move";
        case MouseCursor::TopEdgeResizeCursor:          return "n-resize";
        case MouseCursor::BottomEdgeResizeCursor:       return "s-resize";
        case MouseCursor::LeftEdgeResizeCursor:         return "w-resize";
        case MouseCursor::RightEdgeResizeCursor:        return "e-resize";
        case MouseCursor::TopLeftCornerResizeCursor:    return "nw-resize";
        case MouseCursor::TopRightCornerResizeCursor:   return "ne-resize";
        case MouseCursor::BottomLeftCornerResizeCursor: return "sw-resize";
        case MouseCursor::BottomRightCornerResizeCursor:return "se-resize";
        case MouseCursor::ParentCursor:
        case MouseCursor::NormalCursor:
        case MouseCursor::NumStandardCursorTypes:
        default:                                        return "default";
    }
}

void* CustomMouseCursorInfo::create() const
{
    auto* cursor = new WebCursor();

    MemoryOutputStream png;
    PNGImageFormat format;

    if (image.isValid() && format.writeImageToStream (image, png))
    {
        cursor->css = "url(data:image/png;base64," + Base64::toBase64 (png.getData(), png.getDataSize()).toStdString()
                        + ") " + std::to_string (hotspot.x) + " " + std::to_string (hotspot.y) + ", default";
    }
    else
    {
        cursor->css = "default";
    }

    return cursor;
}

void MouseCursor::deleteMouseCursor (void* cursorHandle, bool)
{
    delete static_cast<WebCursor*> (cursorHandle);
}

void* MouseCursor::createStandardMouseCursor (MouseCursor::StandardCursorType type)
{
    auto* cursor = new WebCursor();
    cursor->css = cursorCssForType (type);
    return cursor;
}

void MouseCursor::showInWindow (ComponentPeer* peer) const
{
    if (auto* webPeer = dynamic_cast<EmscriptenComponentPeer*> (peer))
    {
        auto* handle = static_cast<WebCursor*> (getHandle());
        webPeer->setCursorCss (handle != nullptr ? handle->css : std::string ("default"));
    }
}

bool DragAndDropContainer::performExternalDragDropOfFiles (const StringArray&, bool, Component*, std::function<void()>)
{
    return false;
}

bool DragAndDropContainer::performExternalDragDropOfText (const String&, Component*, std::function<void()>)
{
    return false;
}

void SystemClipboard::copyTextToClipboard (const String& clipText)
{
    WebPeers::clipboardText = clipText;
    juce_web_copyToClipboard (clipText.toRawUTF8());
}

String SystemClipboard::getTextFromClipboard()
{
    auto* text = juce_web_getClipboard();
    auto result = String::fromUTF8 (text);
    free (text);
    return result;
}

bool KeyPress::isKeyCurrentlyDown (int keyCode)
{
    return WebPeers::keysDown.count (WebPeers::normaliseKey (keyCode)) > 0;
}

void LookAndFeel::playAlertSound()
{
}

void JUCE_CALLTYPE NativeMessageBox::showMessageBoxAsync (AlertWindow::AlertIconType, const String& title,
                                                          const String& message, Component*,
                                                          ModalComponentManager::Callback* callback)
{
    std::unique_ptr<ModalComponentManager::Callback> cb (callback);
    String t (title), m (message);

    MessageManager::callAsync ([t, m]
    {
        juce_web_alert (t.toRawUTF8(), m.toRawUTF8());
    });

    if (cb != nullptr)
        cb->modalStateFinished (0);
}

bool JUCE_CALLTYPE NativeMessageBox::showOkCancelBox (AlertWindow::AlertIconType, const String& title,
                                                      const String& message, Component*,
                                                      ModalComponentManager::Callback* callback)
{
    std::unique_ptr<ModalComponentManager::Callback> cb (callback);
    auto result = juce_web_confirm (title.toRawUTF8(), message.toRawUTF8());

    if (cb != nullptr)
        cb->modalStateFinished (result);

    return result != 0;
}

int JUCE_CALLTYPE NativeMessageBox::showYesNoCancelBox (AlertWindow::AlertIconType, const String& title,
                                                        const String& message, Component*,
                                                        ModalComponentManager::Callback* callback)
{
    std::unique_ptr<ModalComponentManager::Callback> cb (callback);
    auto result = juce_web_confirm (title.toRawUTF8(), message.toRawUTF8()) ? 1 : 0;

    if (cb != nullptr)
        cb->modalStateFinished (result);

    return result;
}

int JUCE_CALLTYPE NativeMessageBox::showYesNoBox (AlertWindow::AlertIconType, const String& title,
                                                  const String& message, Component*,
                                                  ModalComponentManager::Callback* callback)
{
    std::unique_ptr<ModalComponentManager::Callback> cb (callback);
    auto result = juce_web_confirm (title.toRawUTF8(), message.toRawUTF8());

    if (cb != nullptr)
        cb->modalStateFinished (result);

    return result;
}

Image juce_createIconForFile (const File&)
{
    return {};
}

}
