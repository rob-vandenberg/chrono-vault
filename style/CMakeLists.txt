# SPDX-License-Identifier: AGPL-3.0-only
#
# Chrono Vault, a password manager.
# Copyright (C) 2026 Rob Vandenberg
#
# --- Version ------------------------------------------------------------
# style/CMakeLists.txt 0.1.41
#
# --- Version history ----------------------------------------------------
# v0.1.41: Switch.qml (D-051)
# v0.0.39: ScrollBar.qml (D-049)
# v0.0.10: Initial release (D-023)

# Chrono Vault's own Qt Quick Controls style: Fusion's controls with their fixed
# sizes times the UI scaling, Fusion itself for every other control (D-023).
qt_add_library(chrono_vault_style STATIC)
qt_add_qml_module(chrono_vault_style
    URI ChronoVault.Style
    # A version Fusion has too: the style imports Fusion with its own version
    # ("auto"), which makes Fusion the fallback for every other control.
    VERSION 6.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ChronoVault/Style"
    IMPORTS QtQuick.Controls.Fusion/auto
    QML_FILES
        BusyIndicator.qml
        Button.qml
        CheckBox.qml
        ComboBox.qml
        DialogButtonBox.qml
        Frame.qml
        ItemDelegate.qml
        Menu.qml
        MenuBarItem.qml
        MenuItem.qml
        Pane.qml
        PopupPanel.qml
        RadioButton.qml
        ScrollBar.qml
        ScrollView.qml
        ScaledCheckIndicator.qml
        Slider.qml
        SpinBox.qml
        Switch.qml
        TextArea.qml
        TextField.qml
        ToolButton.qml
        ToolTip.qml
    SOURCES
        scaling.cpp
        scaling.h
)
target_include_directories(chrono_vault_style PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(chrono_vault_style PUBLIC Qt6::Qml PRIVATE Qt6::Quick Qt6::QuickControls2)
chrono_vault_strict_warnings_for_sources(scaling.cpp)
