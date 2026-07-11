pragma Singleton

import QtQuick

QtObject {
    // Global accessibility switch. Components also expose a local
    // motionEnabled property so either policy can disable animation.
    property bool reducedMotion: false

    readonly property string fontDisplay: "Segoe UI Variable Display"
    readonly property string fontBody: "Segoe UI Variable Text"
    readonly property string fontMono: "Cascadia Mono"

    readonly property int radiusControl: 8
    readonly property int radiusCard: 12
    readonly property int radiusPill: 20
    readonly property int navWidth: 260
    readonly property int navCompactWidth: 76
    readonly property int topBarHeight: 56
    readonly property int controlHeight: 38
    readonly property int fieldHeight: 38
    readonly property int rowHeight: 40

    readonly property int spacing4: 4
    readonly property int spacing8: 8
    readonly property int spacing12: 12
    readonly property int spacing16: 16
    readonly property int spacing20: 20
    readonly property int spacing24: 24
    readonly property int spacing28: 28
    readonly property int spacing32: 32
    readonly property int spacing40: 40

    readonly property int motionShort: 150
    readonly property int motionMedium: 200

    // Material 3 "copper" scheme from the WimForge Material 3 design canvas.
    function primary(dark) { return dark ? "#FFB59E" : "#9A4527" }
    function onPrimary(dark) { return dark ? "#571E0A" : "#FFFFFF" }
    function primaryContainer(dark) { return dark ? "#753523" : "#FFDBD0" }
    function onPrimaryContainer(dark) { return dark ? "#FFDBD0" : "#3B0900" }

    function secondary(dark) { return dark ? "#E7BDAF" : "#77574C" }
    function onSecondary(dark) { return dark ? "#442A20" : "#FFFFFF" }
    function secondaryContainer(dark) { return dark ? "#5D4035" : "#F4DDD3" }
    function onSecondaryContainer(dark) { return dark ? "#FFDBD0" : "#2C150C" }

    function tertiary(dark) { return dark ? "#DDC48C" : "#6C5D2F" }
    function onTertiary(dark) { return dark ? "#3B2F05" : "#FFFFFF" }
    function tertiaryContainer(dark) { return dark ? "#544619" : "#F6E1A6" }
    function onTertiaryContainer(dark) { return dark ? "#FAE1A6" : "#221B00" }

    function success(dark) { return dark ? "#97D5A0" : "#2F6B43" }
    function onSuccess(dark) { return dark ? "#05391B" : "#FFFFFF" }
    function successContainer(dark) { return dark ? "#1E5230" : "#B4F1BF" }
    function onSuccessContainer(dark) { return dark ? "#B4F1BF" : "#00210D" }

    function error(dark) { return dark ? "#FFB4AB" : "#BA1A1A" }
    function onError(dark) { return dark ? "#690005" : "#FFFFFF" }
    function errorContainer(dark) { return dark ? "#93000A" : "#FFDAD6" }
    function onErrorContainer(dark) { return dark ? "#FFDAD6" : "#410002" }

    function surface(dark) { return dark ? "#17100C" : "#FFF8F6" }
    function surfaceDim(dark) { return dark ? "#140D0A" : "#FFF0EA" }
    function surfaceLowest(dark) { return dark ? "#211814" : "#FFFFFF" }
    function surfaceLow(dark) { return dark ? "#231A16" : "#FAEEE8" }
    function surfaceContainer(dark) { return dark ? "#291F1A" : "#F4E7E1" }
    function surfaceHigh(dark) { return dark ? "#342925" : "#EFE1DA" }
    function surfaceHighest(dark) { return dark ? "#3F332E" : "#E9DBD4" }
    function onSurface(dark) { return dark ? "#F1DFD9" : "#231A16" }
    function onSurfaceVariant(dark) { return dark ? "#D5C0B8" : "#53433E" }
    function outline(dark) { return dark ? "#A08D85" : "#85736C" }
    function outlineVariant(dark) { return dark ? "#4E413B" : "#D8C2BA" }

    function navSurface(dark) { return dark ? "#231A16" : "#FAEEE8" }
    function navOn(dark) { return dark ? "#F1DFD9" : "#231A16" }
    function navHover(dark) { return dark ? "#342925" : "#EFE1DA" }

    function surfaceForLevel(level, dark) {
        if (level === "low") return surfaceLow(dark)
        if (level === "container") return surfaceContainer(dark)
        if (level === "high") return surfaceHigh(dark)
        if (level === "highest") return surfaceHighest(dark)
        return surfaceLowest(dark)
    }

    function toneContainer(tone, dark) {
        if (tone === "primary") return primaryContainer(dark)
        if (tone === "info") return secondaryContainer(dark)
        if (tone === "warning") return tertiaryContainer(dark)
        if (tone === "success") return successContainer(dark)
        if (tone === "error" || tone === "destructive") return errorContainer(dark)
        return surfaceHigh(dark)
    }

    function toneForeground(tone, dark) {
        if (tone === "primary") return onPrimaryContainer(dark)
        if (tone === "info") return onSecondaryContainer(dark)
        if (tone === "warning") return onTertiaryContainer(dark)
        if (tone === "success") return onSuccessContainer(dark)
        if (tone === "error" || tone === "destructive") return onErrorContainer(dark)
        return onSurfaceVariant(dark)
    }

    function toneStrong(tone, dark) {
        if (tone === "primary") return primary(dark)
        if (tone === "info") return secondary(dark)
        if (tone === "warning") return tertiary(dark)
        if (tone === "success") return success(dark)
        if (tone === "error" || tone === "destructive") return error(dark)
        return onSurfaceVariant(dark)
    }

    function motionDuration(nominal, enabled) {
        return reducedMotion || enabled === false ? 0 : nominal
    }
}
