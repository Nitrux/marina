set(marina_SRCS
    main.cpp
    controllers/dockmodel.cpp
    controllers/dockmodel.h)

qt_add_executable(${PROJECT_NAME}
    MANUAL_FINALIZATION
    ${marina_SRCS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${_cfg}" _cfg_upper)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${_cfg_upper} "${CMAKE_BINARY_DIR}/bin")
    endforeach()
endif()

qt_add_qml_module(${PROJECT_NAME}
    URI app.marina
    VERSION 1.0
    NO_PLUGIN
    NO_IMPORT_SCAN
    RESOURCE_PREFIX /
    QML_FILES main.qml)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        MauiKit4
        KF6::CoreAddons
        KF6::I18n
        LayerShellQt::Interface
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

qt_finalize_target(${PROJECT_NAME})
