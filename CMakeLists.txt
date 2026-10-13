cmake_minimum_required(VERSION 3.24)

project(ChromaDesk
    VERSION 0.4.2
    DESCRIPTION "A cross-platform, non-destructive desktop photo editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CHROMADESK_ENABLE_RAW "Enable camera RAW decoding when LibRaw is available" ON)
option(CHROMADESK_BUILD_TESTS "Build the ChromaDesk test suite" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent Network)
qt_standard_project_setup(REQUIRES 6.5)

if(APPLE)
    enable_language(OBJCXX)
    set(CHROMADESK_APP_ICON resources/ChromaDesk.icns)
    set_source_files_properties(${CHROMADESK_APP_ICON}
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set(CHROMADESK_MASK_PLATFORM_SOURCE src/core/AppleVisionMaskEngine.mm)
    set_source_files_properties(src/core/AppleVisionMaskEngine.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
else()
    set(CHROMADESK_MASK_PLATFORM_SOURCE src/core/AppleVisionMaskEngine.cpp)
endif()

set(CHROMADESK_SOURCES
    src/main.cpp
    ${CHROMADESK_APP_ICON}
    src/core/Catalog.cpp
    src/core/Catalog.h
    src/core/AppleVisionMaskEngine.h
    ${CHROMADESK_MASK_PLATFORM_SOURCE}
    src/core/EditState.cpp
    src/core/EditState.h
    src/core/ImageDocument.cpp
    src/core/ImageDocument.h
    src/core/ImageLoader.cpp
    src/core/ImageLoader.h
    src/core/ImageProcessor.cpp
    src/core/ImageProcessor.h
    src/core/LayerStack.cpp
    src/core/LayerStack.h
    src/core/PassportSheet.cpp
    src/core/PassportSheet.h
    src/core/LayerMask.cpp
    src/core/LayerMask.h
    src/core/PresetRepository.cpp
    src/core/PresetRepository.h
    src/core/RetouchOperation.cpp
    src/core/RetouchOperation.h
    src/core/SmartMaskEngine.cpp
    src/core/SmartMaskEngine.h
    src/core/TextOverlay.cpp
    src/core/TextOverlay.h
    src/core/WatermarkOverlay.cpp
    src/core/WatermarkOverlay.h
    src/ui/AdjustmentControl.cpp
    src/ui/AdjustmentControl.h
    src/ui/Branding.cpp
    src/ui/Branding.h
    src/ui/ExportDialog.cpp
    src/ui/ExportDialog.h
    src/ui/GenerativeFillClient.cpp
    src/ui/GenerativeFillClient.h
    src/ui/HistogramWidget.cpp
    src/ui/HistogramWidget.h
    src/ui/ImageCanvas.cpp
    src/ui/ImageCanvas.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/TextOverlayDialog.cpp
    src/ui/TextOverlayDialog.h
    src/ui/ThemeManager.cpp
    src/ui/ThemeManager.h
    src/ui/WatermarkDialog.cpp
    src/ui/WatermarkDialog.h
    resources/resources.qrc
)

qt_add_executable(ChromaDesk WIN32 MACOSX_BUNDLE ${CHROMADESK_SOURCES})

target_include_directories(ChromaDesk PRIVATE src)
target_link_libraries(ChromaDesk PRIVATE Qt6::Widgets Qt6::Concurrent Qt6::Network)
if(APPLE)
    target_link_libraries(ChromaDesk PRIVATE
        "-framework Foundation"
        "-framework Vision"
        "-framework CoreVideo"
        "-framework CoreGraphics"
        "-framework ImageIO")
endif()
target_compile_definitions(ChromaDesk PRIVATE
    CHROMADESK_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

if(MSVC)
    target_compile_options(ChromaDesk PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ChromaDesk PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CHROMADESK_ENABLE_RAW)
    find_path(LIBRAW_INCLUDE_DIR NAMES libraw/libraw.h)
    find_library(LIBRAW_LIBRARY NAMES raw_r raw libraw_r libraw)
    if(LIBRAW_INCLUDE_DIR AND LIBRAW_LIBRARY)
        target_include_directories(ChromaDesk PRIVATE "${LIBRAW_INCLUDE_DIR}")
        target_link_libraries(ChromaDesk PRIVATE "${LIBRAW_LIBRARY}")
        target_compile_definitions(ChromaDesk PRIVATE CHROMADESK_HAS_LIBRAW=1)
        message(STATUS "ChromaDesk: LibRaw support enabled")
    else()
        message(STATUS "ChromaDesk: LibRaw not found; standard image formats remain available")
    endif()
endif()

set_target_properties(ChromaDesk PROPERTIES
    OUTPUT_NAME "ChromaDesk"
    MACOSX_BUNDLE_BUNDLE_NAME "ChromaDesk"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.chromadesk.editor"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "ChromaDesk.icns"
)

include(GNUInstallDirs)
install(TARGETS ChromaDesk
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(QT_VERSION VERSION_GREATER_EQUAL 6.5)
    qt_generate_deploy_app_script(
        TARGET ChromaDesk
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT "${deploy_script}")
endif()

if(CHROMADESK_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.5 QUIET COMPONENTS Test)
    if(TARGET Qt6::Test)
        qt_add_executable(ChromaDeskTests
            tests/test_processing.cpp
            src/core/EditState.cpp
            src/core/EditState.h
            src/core/AppleVisionMaskEngine.h
            ${CHROMADESK_MASK_PLATFORM_SOURCE}
            src/core/ImageProcessor.cpp
            src/core/ImageProcessor.h
            src/core/LayerStack.cpp
            src/core/LayerStack.h
            src/core/PassportSheet.cpp
            src/core/PassportSheet.h
            src/core/PresetRepository.cpp
            src/core/PresetRepository.h
            src/core/RetouchOperation.cpp
            src/core/RetouchOperation.h
            src/core/LayerMask.cpp
            src/core/LayerMask.h
            src/core/SmartMaskEngine.cpp
            src/core/SmartMaskEngine.h
            src/core/TextOverlay.cpp
            src/core/TextOverlay.h
            src/core/WatermarkOverlay.cpp
            src/core/WatermarkOverlay.h
        )
        target_include_directories(ChromaDeskTests PRIVATE src)
        target_link_libraries(ChromaDeskTests PRIVATE Qt6::Gui Qt6::Test)
        if(APPLE)
            target_link_libraries(ChromaDeskTests PRIVATE
                "-framework Foundation"
                "-framework Vision"
                "-framework CoreVideo"
                "-framework CoreGraphics"
                "-framework ImageIO")
        endif()
        target_compile_definitions(ChromaDeskTests PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
        add_test(NAME ChromaDeskTests COMMAND ChromaDeskTests)
    endif()
endif()

set(CPACK_PACKAGE_NAME "ChromaDesk")
set(CPACK_PACKAGE_VENDOR "ChromaDesk")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Non-destructive photo editor for Windows and macOS")
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
endif()
include(CPack)
