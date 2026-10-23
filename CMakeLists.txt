cmake_minimum_required(VERSION 3.20)

project(LocalToolsSuite VERSION 4.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 6 MOC / UIC / RCC code generation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Compiler Warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find Qt 6
find_package(Qt6 6.0 REQUIRED COMPONENTS Widgets Gui Core Concurrent Svg Multimedia MultimediaWidgets LinguistTools)

# Find PDFium package in third_party/pdfium
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party/pdfium")
find_package(PDFium REQUIRED PATHS "${CMAKE_SOURCE_DIR}/third_party/pdfium")

message(STATUS "Qt6 Version: ${Qt6_VERSION}")
message(STATUS "PDFium Version: ${PDFium_VERSION}")

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT Qt6_VERSION VERSION_EQUAL "6.11.1")
    message(FATAL_ERROR
        "Store Release builds require the audited Qt 6.11.1 Community SDK; found ${Qt6_VERSION}.")
endif()

# Qt Resource File
set(RESOURCE_SOURCES
    resources/resources.qrc
)

# Source files grouped by layer
set(CORE_SOURCES
    src/core/error.h
    src/core/error.cpp
    src/core/result.h
    src/core/tool_operation.h
    src/core/tool_registry.h
    src/core/tool_registry.cpp
    src/core/job_queue.h
    src/core/job_queue.cpp
    src/core/state/workspace_state.h
    src/core/state/workspace_state.cpp
    src/core/backend/pdfium_backend.h
    src/core/backend/pdfium_backend.cpp
    src/core/backends/image_backend.h
    src/core/backends/image_backend.cpp
    src/core/backends/document_backend.h
    src/core/backends/document_backend.cpp
    src/core/backends/native_vision_backend.h
    src/core/backends/native_vision_backend.cpp
    src/core/backends/file_utils_backend.h
    src/core/backends/file_utils_backend.cpp
    src/core/backends/quick_tools_backend.h
    src/core/backends/quick_tools_backend.cpp
    src/core/backends/ffmpeg_process.h
    src/core/backends/ffmpeg_process.cpp
    third_party/qrcodegen/qrcodegen.cpp
    third_party/quirc/quirc.c
    third_party/quirc/decode.c
    third_party/quirc/identify.c
    third_party/quirc/version_db.c
    src/core/backends/video_backend.h
    src/core/backends/video_backend.cpp
    src/core/backends/audio_backend.h
    src/core/backends/audio_backend.cpp
    src/core/operations/convert_audio.h
    src/core/operations/convert_audio.cpp
    src/core/operations/compress_audio.h
    src/core/operations/compress_audio.cpp
    src/core/operations/trim_audio.h
    src/core/operations/trim_audio.cpp
    src/core/operations/merge_pdf.h
    src/core/operations/merge_pdf.cpp
    src/core/operations/split_pdf.h
    src/core/operations/split_pdf.cpp
    src/core/operations/extract_pages.h
    src/core/operations/extract_pages.cpp
    src/core/operations/delete_pages.h
    src/core/operations/delete_pages.cpp
    src/core/operations/reorder_pages.h
    src/core/operations/reorder_pages.cpp
    src/core/operations/rotate_pages.h
    src/core/operations/rotate_pages.cpp
    src/core/operations/images_to_pdf.h
    src/core/operations/images_to_pdf.cpp
    src/core/operations/pdf_to_images.h
    src/core/operations/pdf_to_images.cpp
    src/core/operations/pdf_to_text.h
    src/core/operations/pdf_to_text.cpp
    src/core/operations/compress_pdf.h
    src/core/operations/compress_pdf.cpp
    src/core/operations/watermark_pdf.h
    src/core/operations/watermark_pdf.cpp
)

set(SERVICE_SOURCES
    src/services/logging_service.h
    src/services/logging_service.cpp
    src/services/file_service.h
    src/services/file_service.cpp
    src/services/settings_service.h
    src/services/settings_service.cpp
    src/services/localization_service.h
    src/services/localization_service.cpp
)

set(WORKER_SOURCES
    src/workers/pdf_worker.h
    src/workers/pdf_worker.cpp
)

set(UI_SOURCES
    src/ui/theme.h
    src/ui/theme.cpp
    src/ui/common/icon_utils.h
    src/ui/common/icon_utils.cpp
    src/ui/common/async_task.h
    src/ui/common/output_file_dialog.h
    src/ui/common/output_file_dialog.cpp
    src/ui/common/sidebar.h
    src/ui/common/sidebar.cpp
    src/ui/common/page_header.h
    src/ui/common/page_header.cpp
    src/ui/common/search_field.h
    src/ui/common/search_field.cpp
    src/ui/common/tool_card.h
    src/ui/common/tool_card.cpp
    src/ui/common/drop_zone.h
    src/ui/common/drop_zone.cpp
    src/ui/common/file_card.h
    src/ui/common/file_card.cpp
    src/ui/common/status_banner.h
    src/ui/common/status_banner.cpp
    src/ui/common/pill_slider.h
    src/ui/common/pill_slider.cpp
    src/ui/common/video_range_slider.h
    src/ui/common/video_range_slider.cpp
    src/ui/common/thumbnail_card.h
    src/ui/common/thumbnail_card.cpp
    src/ui/common/thumbnail_grid.h
    src/ui/common/thumbnail_grid.cpp
    src/ui/common/page_workspace.h
    src/ui/common/page_workspace.cpp
    src/ui/pages/home_page.h
    src/ui/pages/home_page.cpp
    src/ui/pages/organize_page.h
    src/ui/pages/organize_page.cpp
    src/ui/pages/result_screen.h
    src/ui/pages/result_screen.cpp
    src/ui/pages/convert_page.h
    src/ui/pages/convert_page.cpp
    src/ui/pages/optimize_page.h
    src/ui/pages/optimize_page.cpp
    src/ui/pages/metadata_page.h
    src/ui/pages/metadata_page.cpp
    src/ui/pages/images_page.h
    src/ui/pages/images_page.cpp
    src/ui/pages/documents_page.h
    src/ui/pages/documents_page.cpp
    src/ui/pages/files_page.h
    src/ui/pages/files_page.cpp
    src/ui/pages/quick_tools_page.h
    src/ui/pages/quick_tools_page.cpp
    src/ui/pages/video_page.h
    src/ui/pages/video_page.cpp
    src/ui/pages/audio_page.h
    src/ui/pages/audio_page.cpp
    src/ui/pages/settings_page.h
    src/ui/pages/settings_page.cpp
    src/ui/main_window.h
    src/ui/main_window.cpp
)

if(WIN32)
    set(APP_RC_SOURCE resources/app.rc)
endif()

# Main Application Target
add_executable(LocalTools WIN32
    src/main.cpp
    src/app/application.h
    src/app/application.cpp
    ${CORE_SOURCES}
    ${SERVICE_SOURCES}
    ${WORKER_SOURCES}
    ${UI_SOURCES}
    ${RESOURCE_SOURCES}
    ${APP_RC_SOURCE}
)

set(TS_FILES resources/translations/tr_TR.ts)

qt_add_translations(LocalTools
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/translations"
)

target_include_directories(LocalTools PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

target_link_libraries(LocalTools PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::Concurrent
    Qt6::Svg
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    pdfium
)

if(WIN32)
    target_link_libraries(LocalTools PRIVATE dwmapi)
endif()

# Automated Smoke Test Target
add_executable(smoke_test
    tests/smoke_test.cpp
    ${CORE_SOURCES}
    ${SERVICE_SOURCES}
    ${WORKER_SOURCES}
)

qt_add_translations(smoke_test
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/translations"
)

target_include_directories(smoke_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smoke_test PRIVATE
    Qt6::Gui
    Qt6::Core
    pdfium
)

# UI regression: verifies that the application-wide combo popup setup keeps
# every visible row at full height (including the last row) and only scrolls
# when the item count exceeds the visible-row cap.
add_executable(combo_popup_test
    tests/combo_popup_test.cpp
    src/ui/theme.cpp
    src/services/settings_service.cpp
)

target_include_directories(combo_popup_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(combo_popup_test PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
)

add_executable(settings_service_test
    tests/settings_service_test.cpp
    src/services/settings_service.cpp
)

add_executable(output_safety_test
    tests/output_safety_test.cpp
    src/services/file_service.cpp
    src/services/settings_service.cpp
    src/ui/common/output_file_dialog.cpp
)

add_executable(logging_privacy_test
    tests/logging_privacy_test.cpp
    src/core/error.cpp
    src/services/logging_service.cpp
)

add_executable(job_queue_test
    tests/job_queue_test.cpp
    src/core/error.cpp
    src/core/job_queue.cpp
)

add_executable(ffmpeg_process_test
    tests/ffmpeg_process_test.cpp
    src/core/error.cpp
    src/core/backends/ffmpeg_process.cpp
)

add_executable(translation_catalog_test
    tests/translation_catalog_test.cpp
)

add_executable(release_performance_test
    tests/release_performance_test.cpp
    src/core/error.cpp
    src/core/backend/pdfium_backend.cpp
    src/core/backends/file_utils_backend.cpp
    src/core/backends/image_backend.cpp
    src/core/operations/merge_pdf.cpp
    src/services/file_service.cpp
)

add_executable(windows_media_backend_test
    tests/windows_media_backend_test.cpp
)

target_link_libraries(windows_media_backend_test PRIVATE
    Qt6::Core
    Qt6::Multimedia
)

target_include_directories(release_performance_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(release_performance_test PRIVATE
    Qt6::Core
    Qt6::Gui
    pdfium
)

if(WIN32)
    target_link_libraries(release_performance_test PRIVATE psapi)
endif()

add_executable(ui_accessibility_test
    tests/ui_accessibility_test.cpp
    ${CORE_SOURCES}
    ${SERVICE_SOURCES}
    ${WORKER_SOURCES}
    ${UI_SOURCES}
    ${RESOURCE_SOURCES}
)

# Explicitly built release-delivery helper. It renders real application pages
# with neutral local fixtures and never enters the production install/MSIX.
add_executable(store_screenshot_tool EXCLUDE_FROM_ALL
    tests/store_screenshot_tool.cpp
    ${CORE_SOURCES}
    ${SERVICE_SOURCES}
    ${WORKER_SOURCES}
    ${UI_SOURCES}
    ${RESOURCE_SOURCES}
)

qt_add_translations(store_screenshot_tool
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/translations"
)

target_include_directories(store_screenshot_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

target_link_libraries(store_screenshot_tool PRIVATE
    Qt6::Widgets Qt6::Gui Qt6::Core Qt6::Concurrent Qt6::Svg
    Qt6::Multimedia Qt6::MultimediaWidgets pdfium
)

if(WIN32)
    target_link_libraries(store_screenshot_tool PRIVATE dwmapi)
endif()

qt_add_translations(ui_accessibility_test
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/translations"
)

target_include_directories(ui_accessibility_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

target_link_libraries(ui_accessibility_test PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::Concurrent
    Qt6::Svg
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    pdfium
)

if(WIN32)
    target_link_libraries(ui_accessibility_test PRIVATE dwmapi)
endif()

target_link_libraries(translation_catalog_test PRIVATE
    Qt6::Core
)

target_include_directories(ffmpeg_process_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ffmpeg_process_test PRIVATE
    Qt6::Core
)

target_include_directories(job_queue_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(job_queue_test PRIVATE
    Qt6::Core
)

target_include_directories(logging_privacy_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(logging_privacy_test PRIVATE
    Qt6::Core
)

target_include_directories(output_safety_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(output_safety_test PRIVATE
    Qt6::Widgets
    Qt6::Core
)

target_include_directories(settings_service_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(settings_service_test PRIVATE
    Qt6::Core
)

if(WIN32)
    target_link_libraries(combo_popup_test PRIVATE dwmapi)
endif()

# Release certification must execute the same assertion-based checks as Debug.
# CMake normally defines NDEBUG for Release, which would otherwise turn much of
# the integration suite into a no-op while still returning success.
set(LOCALTOOLS_TEST_TARGETS
    smoke_test
    combo_popup_test
    settings_service_test
    output_safety_test
    logging_privacy_test
    job_queue_test
    ffmpeg_process_test
    translation_catalog_test
    release_performance_test
    windows_media_backend_test
    ui_accessibility_test
)
foreach(testTarget IN LISTS LOCALTOOLS_TEST_TARGETS)
    if(MSVC)
        target_compile_options(${testTarget} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${testTarget} PRIVATE -UNDEBUG)
    endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME smoke_test COMMAND smoke_test "${CMAKE_SOURCE_DIR}/test.pdf")
    set_tests_properties(smoke_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 180
    )

    add_test(NAME combo_popup_test COMMAND combo_popup_test)
    set_tests_properties(combo_popup_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 30
    )

    add_test(NAME settings_service_test COMMAND settings_service_test)
    set_tests_properties(settings_service_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 30
    )

    add_test(NAME output_safety_test COMMAND output_safety_test)
    set_tests_properties(output_safety_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 30
    )

    add_test(NAME logging_privacy_test COMMAND logging_privacy_test)
    set_tests_properties(logging_privacy_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 30
    )

    add_test(NAME job_queue_test COMMAND job_queue_test)
    set_tests_properties(job_queue_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 10
    )

    add_test(NAME ffmpeg_process_test
        COMMAND ffmpeg_process_test "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/bin/ffmpeg.exe")
    set_tests_properties(ffmpeg_process_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 30
    )


    add_test(NAME translation_catalog_test
        COMMAND translation_catalog_test "${CMAKE_SOURCE_DIR}/resources/translations/tr_TR.ts")
    set_tests_properties(translation_catalog_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 10
    )

    add_test(NAME release_performance_test
        COMMAND release_performance_test
            "${CMAKE_SOURCE_DIR}/test.pdf"
            "${CMAKE_BINARY_DIR}/performance-results.json")
    set_tests_properties(release_performance_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 120
    )

    add_test(NAME windows_media_backend_test
        COMMAND windows_media_backend_test
            "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/bin/ffmpeg.exe")
    set_tests_properties(windows_media_backend_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 30
        ENVIRONMENT "QT_MEDIA_BACKEND=windows"
    )


    foreach(scale IN ITEMS 100 125 150 200)
        math(EXPR scalePercent "${scale}")
        math(EXPR scaleWhole "${scalePercent} / 100")
        math(EXPR scaleFraction "${scalePercent} % 100")
        set(scaleValue "${scaleWhole}.${scaleFraction}")
        foreach(theme IN ITEMS system light dark)
            foreach(language IN ITEMS en tr)
                set(testName "ui_${scale}_${theme}_${language}")
                add_test(NAME ${testName} COMMAND ui_accessibility_test)
                set_tests_properties(${testName} PROPERTIES
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                    TIMEOUT 30
                    ENVIRONMENT "QT_SCALE_FACTOR=${scaleValue};LOCALTOOLS_TEST_THEME=${theme};LOCALTOOLS_TEST_LANGUAGE=${language}"
                )
            endforeach()
        endforeach()
    endforeach()
endif()

# Post-build deployment.  A copied executable must be runnable from its build
# directory too; linking Qt only makes DLLs available while Qt's bin directory
# is on PATH.  windeployqt also deploys Multimedia's platform/media plugins.
if(WIN32)
    get_filename_component(QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
    set(QT_WINDEPLOYQT "${QT_BIN_DIR}/windeployqt.exe")
    set(LOCALTOOLS_FFMPEG_DIR "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/bin")
    set(LOCALTOOLS_FFMPEG_LICENSE "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/LICENSE.txt")
    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND
       (NOT EXISTS "${LOCALTOOLS_FFMPEG_DIR}/ffmpeg.exe" OR
        NOT EXISTS "${LOCALTOOLS_FFMPEG_DIR}/ffprobe.exe"))
        message(FATAL_ERROR
            "Release builds require the verified LGPL FFmpeg runtime. Run tools/fetch_ffmpeg.ps1 first.")
    endif()
    if(NOT EXISTS "${QT_WINDEPLOYQT}")
        message(FATAL_ERROR "windeployqt.exe was not found at ${QT_WINDEPLOYQT}")
    endif()

    add_custom_command(TARGET LocalTools POST_BUILD
        COMMAND "${QT_WINDEPLOYQT}"
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            --dir "$<TARGET_FILE_DIR:LocalTools>"
            "$<TARGET_FILE:LocalTools>"
        COMMENT "Deploying Qt runtime and Multimedia plugins to LocalTools output directory..."
    )

    if(EXISTS "${LOCALTOOLS_FFMPEG_DIR}/ffmpeg.exe" AND
       EXISTS "${LOCALTOOLS_FFMPEG_DIR}/ffprobe.exe")
        add_custom_command(TARGET LocalTools POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LOCALTOOLS_FFMPEG_DIR}/ffmpeg.exe" "$<TARGET_FILE_DIR:LocalTools>/ffmpeg.exe"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LOCALTOOLS_FFMPEG_DIR}/ffprobe.exe" "$<TARGET_FILE_DIR:LocalTools>/ffprobe.exe"
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:LocalTools>/licenses"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LOCALTOOLS_FFMPEG_LICENSE}" "$<TARGET_FILE_DIR:LocalTools>/licenses/FFmpeg-LICENSE.txt"
            COMMENT "Deploying verified LGPL FFmpeg runtime"
        )
    endif()
endif()

# Copy the project runtime assets to target binary directories.
add_custom_command(TARGET LocalTools POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:pdfium>
        $<TARGET_FILE_DIR:LocalTools>
    COMMENT "Deploying pdfium.dll to the LocalTools output directory..."
)

add_custom_command(TARGET smoke_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:pdfium>
        $<TARGET_FILE_DIR:smoke_test>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/test.pdf"
        $<TARGET_FILE_DIR:smoke_test>
    COMMENT "Deploying pdfium.dll and test.pdf to smoke_test output directory..."
)

add_custom_command(TARGET ui_accessibility_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:pdfium>
        $<TARGET_FILE_DIR:ui_accessibility_test>
    COMMENT "Deploying pdfium.dll to ui_accessibility_test output directory..."
)

add_custom_command(TARGET store_screenshot_tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/third_party/pdfium/bin/pdfium.dll"
        $<TARGET_FILE_DIR:store_screenshot_tool>
    COMMENT "Deploying pdfium.dll to store_screenshot_tool output directory..."
)

add_custom_command(TARGET release_performance_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:pdfium>
        $<TARGET_FILE_DIR:release_performance_test>
    COMMENT "Deploying pdfium.dll to release_performance_test output directory..."
)

# Production staging is intentionally allowlisted. Development output folders
# may contain test executables and fixtures; only these install rules feed the
# Store packaging pipeline.
install(TARGETS LocalTools RUNTIME DESTINATION .)
install(FILES $<TARGET_FILE:pdfium> DESTINATION .)

if(WIN32)
    install(PROGRAMS
        "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/bin/ffmpeg.exe"
        "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/bin/ffprobe.exe"
        DESTINATION .
    )
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/legal/"
    DESTINATION licenses
    FILES_MATCHING
        PATTERN "*.txt"
        PATTERN "*.md"
)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/pdfium/licenses/"
    DESTINATION licenses/PDFium-components
    FILES_MATCHING PATTERN "*"
)
install(FILES
    "${CMAKE_SOURCE_DIR}/third_party/pdfium/LICENSE"
    DESTINATION licenses
    RENAME "PDFium-LICENSE.txt"
)
install(FILES
    "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/LICENSE.txt"
    DESTINATION licenses
    RENAME "FFmpeg-LGPL-3.0.txt"
)
