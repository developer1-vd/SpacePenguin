import QtQuick

// The desktop app's painted icon set, redrawn on a 24-unit grid so the mobile
// apps need no image plugins and every glyph follows the theme's colour.
Canvas {
    id: icon
    property string name
    property color color: "black"
    property bool filled: false
    implicitWidth: 24
    implicitHeight: 24
    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onFilledChanged: requestPaint()

    onPaint: {
        const g = getContext("2d");
        g.reset();
        g.scale(width / 24, height / 24);
        g.strokeStyle = color;
        g.fillStyle = color;
        g.lineWidth = 1.9;
        g.lineCap = "round";
        g.lineJoin = "round";
        const line = (x1, y1, x2, y2) => { g.beginPath(); g.moveTo(x1, y1); g.lineTo(x2, y2); g.stroke(); };
        const poly = (pts, close) => {
            g.beginPath(); g.moveTo(pts[0], pts[1]);
            for (let i = 2; i < pts.length; i += 2) g.lineTo(pts[i], pts[i + 1]);
            if (close) g.closePath();
            g.stroke();
        };
        switch (name) {
        case "back": line(19, 12, 5.5, 12); poly([11, 6, 5, 12, 11, 18]); break;
        case "forward": line(5, 12, 18.5, 12); poly([13, 6, 19, 12, 13, 18]); break;
        case "reload":
            g.beginPath(); g.arc(12, 12, 7, -Math.PI * 0.35, Math.PI * 1.45); g.stroke();
            poly([15.5, 3.8, 17.3, 6.7, 14.3, 8.2]); break;
        case "close": line(7, 7, 17, 17); line(17, 7, 7, 17); break;
        case "check": poly([5, 12.5, 10, 17.5, 19.5, 7]); break;
        case "plus": line(12, 5, 12, 19); line(5, 12, 19, 12); break;
        case "menu": [6, 12, 18].forEach(y => { g.beginPath(); g.arc(12, y, 1.6, 0, Math.PI * 2); g.fill(); }); break;
        case "tabs":
            g.beginPath(); g.roundedRect(4.5, 6.5, 13, 13, 3, 3); g.stroke();
            poly([8, 3.5, 17.5, 3.5, 20.5, 6.5, 20.5, 16]); break;
        case "search":
            g.beginPath(); g.arc(10.5, 10.5, 5.5, 0, Math.PI * 2); g.stroke(); line(14.5, 14.5, 19.5, 19.5); break;
        case "star": {
            g.beginPath();
            for (let i = 0; i < 10; ++i) {
                const r = i % 2 === 0 ? 8.5 : 3.8, a = (-90 + i * 36) * Math.PI / 180;
                const x = 12 + r * Math.cos(a), y = 12.6 + r * Math.sin(a);
                if (i === 0) g.moveTo(x, y); else g.lineTo(x, y);
            }
            g.closePath();
            if (filled) g.fill(); else g.stroke();
            break;
        }
        case "bookmarks": poly([7, 4, 17, 4, 17, 20, 12, 16.5, 7, 20], true); break;
        case "history":
            g.beginPath(); g.arc(12, 12, 8, 0, Math.PI * 2); g.stroke(); poly([12, 7.5, 12, 12, 15, 14]); break;
        case "settings":
            g.beginPath(); g.arc(12, 12, 3, 0, Math.PI * 2); g.stroke();
            for (let i = 0; i < 8; ++i) {
                const a = i * Math.PI / 4;
                line(12 + 6 * Math.cos(a), 12 + 6 * Math.sin(a), 12 + 8.5 * Math.cos(a), 12 + 8.5 * Math.sin(a));
            }
            g.beginPath(); g.arc(12, 12, 6, 0, Math.PI * 2); g.stroke(); break;
        case "shield": poly([12, 3.5, 19, 6.5, 19, 11.5, 12, 20.5, 5, 11.5, 5, 6.5], true); break;
        case "share": poly([8, 9, 12, 5, 16, 9]); line(12, 5, 12, 15); poly([6, 13, 6, 19, 18, 19, 18, 13]); break;
        case "home": poly([4, 11.5, 12, 4.5, 20, 11.5]); poly([6.5, 9.5, 6.5, 19.5, 17.5, 19.5, 17.5, 9.5]); break;
        case "trash": line(5, 7, 19, 7); poly([7, 7, 8, 19.5, 16, 19.5, 17, 7]); poly([9.5, 7, 10, 4.5, 14, 4.5, 14.5, 7]); break;
        case "info":
            g.beginPath(); g.arc(12, 12, 8, 0, Math.PI * 2); g.stroke(); line(12, 11, 12, 16);
            g.beginPath(); g.arc(12, 8, 1, 0, Math.PI * 2); g.fill(); break;
        }
    }
}
