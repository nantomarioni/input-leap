import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// A SpinBox that shows plain digits (no locale grouping) at a fixed width.
SpinBox {
    Layout.preferredWidth: 130
    editable: true
    textFromValue: function(value, locale) { return value.toString() }
    valueFromText: function(text, locale) { var n = parseInt(text); return isNaN(n) ? 0 : n }
}
