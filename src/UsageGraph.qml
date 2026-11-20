import QtQuick

// A minute of samples drawn right to left, newest at the right edge. A fixed
// maximum (percentages) keeps the scale still; zero scales to the busiest
// sample on screen, for rates.
Canvas {
    id: graph
    property var values: []
    property var secondValues: []
    property real maximum: 0
    property int length: 60
    property color color: "#FFD60A"
    property color secondColor: "#8a8a92"

    onValuesChanged: requestPaint()
    onSecondValuesChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function scale() {
        if (maximum > 0)
            return maximum
        let top = 1
        for (const v of values) top = Math.max(top, v)
        for (const v of secondValues) top = Math.max(top, v)
        return top * 1.15
    }

    function trace(ctx, series, stroke, fill, top) {
        if (series.length < 2)
            return
        const step = width / (length - 1)
        const x0 = width - (series.length - 1) * step
        ctx.beginPath()
        ctx.moveTo(x0, height)
        for (let i = 0; i < series.length; ++i)
            ctx.lineTo(x0 + i * step, height - Math.min(1, series[i] / top) * (height - 2))
        ctx.lineTo(width, height)
        ctx.closePath()
        if (fill) {
            ctx.fillStyle = Qt.rgba(stroke.r, stroke.g, stroke.b, 0.18)
            ctx.fill()
        }
        ctx.beginPath()
        for (let i = 0; i < series.length; ++i) {
            const x = x0 + i * step, y = height - Math.min(1, series[i] / top) * (height - 2)
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.strokeStyle = stroke
        ctx.lineWidth = 1.5
        ctx.stroke()
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        // Quarter lines give the eye something to measure against.
        ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.06)
        ctx.lineWidth = 1
        for (let q = 1; q < 4; ++q) {
            const y = Math.round(height * q / 4) + 0.5
            ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
        }
        const top = scale()
        trace(ctx, secondValues, secondColor, false, top)
        trace(ctx, values, color, true, top)
    }
}
