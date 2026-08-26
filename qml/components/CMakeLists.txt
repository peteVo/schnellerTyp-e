# SPDX-License-Identifier: MIT
#
# The shared QML components live in their own module so that Theme can be a
# proper QML singleton and so the settings window imports them by name rather
# than by relative path.
#
# The module is defined from this directory on purpose: qt_add_qml_module keeps
# the directory structure of QML_FILES, so declaring them here is what makes the
# type names come out as `Card`, `ToggleSwitch`, … rather than nested paths.

qt_add_library(schnellertype_components STATIC)

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(schnellertype_components
    URI SchnellerTypE.Components
    VERSION 1.0
    # Must mirror the URI so qmllint and qmlls can find the module.
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/SchnellerTypE/Components"
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Theme.qml
        AccentButton.qml
        BadgeTile.qml
        Card.qml
        ChoiceRow.qml
        StatusRow.qml
        ToggleSwitch.qml
)

target_link_libraries(schnellertype_components PRIVATE Qt6::Quick Qt6::QuickControls2)
