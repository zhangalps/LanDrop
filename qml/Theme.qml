import QtQuick

QtObject {
    id: themeObj
    property bool isDark: false

    readonly property color pageBg:          isDark ? "#111317" : "#F5F6F8"
    readonly property color surfaceBg:       isDark ? "#1B1E24" : "#FFFFFF"
    readonly property color chatBg:          isDark ? "#111317" : "#F5F6F8"
    readonly property color chatBarBg:       isDark ? "#1B1E24" : "#FFFFFF"
    readonly property color outgoingBubble:  isDark ? "#1E3A5F" : "#E8F1FF"
    readonly property color outgoingText:    isDark ? "#E2E8F0" : "#1E293B"
    readonly property color incomingBubble:  isDark ? "#1E222B" : "#FFFFFF"
    readonly property color bubbleBorder:    isDark ? "#2A303C" : "#E8ECF2"
    readonly property color divider:         isDark ? "#262B34" : "#ECEEF2"
    readonly property color accent:          "#3B82F6"
    readonly property color accentHover:     "#2563EB"
    readonly property color accentSoft:      isDark ? "#1E2D45" : "#EFF6FF"
    readonly property color onlineDot:       "#10B981"
    readonly property color pendingDot:      "#F59E0B"
    readonly property color textPrimary:     isDark ? "#F1F5F9" : "#1E293B"
    readonly property color textBody:        isDark ? "#CBD5E1" : "#334155"
    readonly property color textMuted:       isDark ? "#64748B" : "#94A3B8"
    readonly property color inputBg:         isDark ? "#13161C" : "#F3F5F8"
    readonly property color badgeBg:         isDark ? "#262C38" : "#F1F5F9"
    readonly property color dangerText:      "#EF4444"

    function avatarInitial(name) {
        if (!name || name.trim().length === 0)
            return "L"
        var trimmed = name.trim()
        var parts = trimmed.split(/[\s\-_]+/).filter(function(p) { return p.length > 0 })
        if (parts.length >= 2)
            return (parts[0].charAt(0) + parts[1].charAt(0)).toUpperCase()
        if (/^[A-Za-z0-9]+$/.test(trimmed) && trimmed.length >= 2)
            return (trimmed.charAt(0) + trimmed.charAt(trimmed.length - 1)).toUpperCase()
        return trimmed.charAt(0).toUpperCase()
    }

    function _avatarIndex(seed) {
        var s = seed ? String(seed) : "LanDrop"
        var h = 2166136261
        for (var i = 0; i < s.length; ++i) {
            h ^= s.charCodeAt(i)
            h = Math.imul(h, 16777619)
        }
        return (h >>> 0) % 12
    }

    function avatarBg(seed) {
        var lightBgs = [
            "#DBEAFE", "#D1FAE5", "#EDE9FE", "#FEF3C7", "#FFE4E6", "#E0F2FE",
            "#CCFBF1", "#E0E7FF", "#FFEDD5", "#DCFCE7", "#FAE8FF", "#CFFAFE"
        ]
        var darkBgs = [
            "#1E3352", "#14352A", "#2D2246", "#3D2E14", "#3F1D28", "#163244",
            "#133835", "#24284E", "#422714", "#173822", "#3A1C42", "#143640"
        ]
        var idx = _avatarIndex(seed)
        return isDark ? darkBgs[idx] : lightBgs[idx]
    }

    function avatarFg(seed) {
        var lightFgs = [
            "#2563EB", "#059669", "#7C3AED", "#D97706", "#E11D48", "#0284C7",
            "#0D9488", "#4F46E5", "#EA580C", "#16A34A", "#C026D3", "#0891B2"
        ]
        var darkFgs = [
            "#93C5FD", "#6EE7B7", "#C4B5FD", "#FCD34D", "#FDA4AF", "#7DD3FC",
            "#5EEAD4", "#A5B4FC", "#FDBA74", "#86EFAC", "#F0ABFC", "#67E8F9"
        ]
        var idx = _avatarIndex(seed)
        return isDark ? darkFgs[idx] : lightFgs[idx]
    }
}
