# pcp-desktop — Pure C++/Qt6 Matrix client for Linux desktop + PineTab 2/PinePhone.
# Sister to pcp-android-next. Reuses the pcp_native core (libolm + libcurl).
#
# Build:
#   cmake --preset pinetab2        # or: cmake --preset desktop
#   cmake --build build -j4
cmake_minimum_required(VERSION 3.22)

project(pcp-desktop
    VERSION 0.1.0
    DESCRIPTION "Pure C++/Qt6 Matrix client — sister to pcp-android-next"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# --- Options ---
option(PCP_MOBILE "Fullscreen frameless UI for PinePhone/PineTab 2 (auto-detected at runtime)" OFF)
option(PCP_LTO    "Enable LTO for pcp_native (slow link, smaller binary)" OFF)
option(PCP_BUILD_TESTS "Build tests" ON)

# --- Qt6 ---
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# --- libcurl (HTTP client with TLS + SOCKS5/Tor proxy) ---
find_package(CURL REQUIRED COMPONENTS HTTPS)

# --- Canonical core: pcp-chat/pcp-core (FetchContent) ---
# One Qt-free core for ALL frontends (desktop, CLI, …). Never copy
# src/core into this repo again — edit pcp-core and bump the tag.
include(FetchContent)
FetchContent_Declare(
    pcp-core
    GIT_REPOSITORY https://github.com/pcp-chat/pcp-core.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
# The core builds its own unit tests — skip them when fetched. The core
# shares the SAME option name, so restore the desktop's own flag right after
# (the desktop keeps its integration tests: Qt/UI + the live-Synapse E2EE).
set(PCP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# Download the prebuilt core for this commit (fast builds). Falls back to
# compiling from source automatically when the artifact isn't published yet.
set(PCP_CORE_USE_ARTIFACT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(pcp-core)
set(PCP_BUILD_TESTS ON CACHE BOOL "" FORCE)

# --- UI widget layer (no deps on other UI libs) ---
add_library(pcp_ui_widgets STATIC
    src/ui/shared/image_loader.cpp
    src/ui/shared/proxy_settings.cpp
    src/ui/shared/room_activity_log.cpp
    src/ui/shared/theme.cpp
    src/ui/timeline/timeline_model.cpp
    src/ui/timeline/timeline_delegate.cpp
    src/ui/timeline/timeline_handlers.cpp
    src/ui/timeline/timeline_layout.cpp
    src/ui/timeline/timeline_painter.cpp
    src/ui/room/room_store.cpp
    src/ui/room/room_data_loader.cpp
    src/ui/room/event_parser.cpp
    src/ui/room_list_model.cpp
    src/ui/room_list_delegate.cpp
    src/ui/chat/chat_logger.cpp
    src/ui/chat/chat_view.cpp
    src/ui/chat/message_edit.cpp
    src/ui/chat/emoji_picker.cpp
)
target_include_directories(pcp_ui_widgets PUBLIC src)
target_link_libraries(pcp_ui_widgets PUBLIC pcp_core pcp_ui_dialogs Qt6::Widgets)
# --- UI dialogs layer (no deps on other UI libs) ---
add_library(pcp_ui_dialogs STATIC
    src/ui/dialogs/login_dialog.cpp
    src/ui/dialogs/color_settings_dialog.cpp
    src/ui/dialogs/image_viewer_dialog.cpp
    src/ui/dialogs/room_directory_dialog.cpp
    src/ui/dialogs/room_settings_dialog.cpp
    src/ui/dialogs/room_activity_log_dialog.cpp
    src/ui/dialogs/room_switcher_dialog.cpp
    src/ui/dialogs/threads_dialog.cpp
    src/ui/dialogs/shortcuts_dialog.cpp
    src/ui/dialogs/sas_verification_dialog.cpp
    src/ui/dialogs/prefs_dialog.cpp
    src/ui/dialogs/network_log_dialog.cpp
    src/ui/dialogs/log_viewer_dialog.cpp
    src/ui/profile_dialog.cpp
    src/ui/profile/room_members_dialog.cpp
    src/ui/profile/user_profile_dialog.cpp
    src/ui/notifications.cpp
)
target_include_directories(pcp_ui_dialogs PUBLIC src)
target_link_libraries(pcp_ui_dialogs PUBLIC pcp_core pcp_ui_widgets Qt6::Widgets)

# --- UI layout layer (depends on widgets) ---
add_library(pcp_ui_layout STATIC
    src/ui/ui_layout_builder.cpp
)
target_include_directories(pcp_ui_layout PUBLIC src)
target_link_libraries(pcp_ui_layout PUBLIC pcp_ui_widgets Qt6::Widgets)

# --- UI handlers layer (depends on widgets + dialogs) ---
add_library(pcp_ui_handlers STATIC
    src/ui/handlers/auth_handler.cpp
    src/ui/handlers/room_handler.cpp
    src/ui/handlers/thread_handler.cpp
    src/ui/handlers/room_context_menu.cpp
    src/ui/handlers/room_key_helper.cpp
    src/ui/handlers/toolbar_handler.cpp
    src/ui/handlers/sync_response_handler.cpp
    src/ui/handlers/attachment_handler.cpp
    src/ui/handlers/account_switcher.cpp
    src/ui/handlers/session_bootstrap.cpp
    src/ui/handlers/slash_command_handler.cpp
    src/ui/handlers/verification_handler.cpp
)
target_include_directories(pcp_ui_handlers PUBLIC src)
target_link_libraries(pcp_ui_handlers PUBLIC
    pcp_ui_widgets
    pcp_ui_dialogs
    Qt6::Widgets
)

# --- Desktop app ---
add_executable(pcp-desktop
    src/main.cpp
    src/ui/main_window.cpp
    resources/resources.qrc
)
target_link_libraries(pcp-desktop PRIVATE
    pcp_ui_handlers
    pcp_ui_layout
    pcp_ui_dialogs
    pcp_ui_widgets
    Qt6::Widgets
)
target_link_options(pcp-desktop PRIVATE -rdynamic)

target_compile_definitions(pcp-desktop PRIVATE
    PCP_DESKTOP=1
)

if(PCP_MOBILE)
    target_compile_definitions(pcp-desktop PRIVATE PCP_MOBILE=1)
endif()

# --- Tests ---
if(PCP_BUILD_TESTS)
    enable_testing()

    add_executable(test_sync_applier tests/test_sync_applier.cpp)
    target_link_libraries(test_sync_applier PRIVATE pcp_core)
    add_executable(test_media_crypto tests/test_media_crypto.cpp)
    target_link_libraries(test_media_crypto PRIVATE pcp_core)
    add_executable(test_phase1 tests/test_phase1.cpp)
    target_link_libraries(test_phase1 PRIVATE pcp_core)
    target_compile_options(test_phase1 PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME sync_applier COMMAND test_sync_applier)
    add_test(NAME media_crypto COMMAND test_media_crypto)
    add_test(NAME phase1 COMMAND test_phase1)

    add_executable(test_phase3 tests/bench_sync_parse.cpp)
    target_link_libraries(test_phase3 PRIVATE pcp_core)
    target_compile_options(test_phase3 PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    # Not registered with ctest — needs network for --record, fixture for bench

    add_executable(test_olm_inbound tests/test_olm_inbound.cpp)
    target_link_libraries(test_olm_inbound PRIVATE pcp_core)
    target_compile_options(test_olm_inbound PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME olm_inbound COMMAND test_olm_inbound)

    add_executable(test_megolm_inbound tests/test_megolm_inbound.cpp)
    target_link_libraries(test_megolm_inbound PRIVATE pcp_core)
    target_compile_options(test_megolm_inbound PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME megolm_inbound COMMAND test_megolm_inbound)

    add_executable(test_e2ee_account tests/test_e2ee_account.cpp)
    target_link_libraries(test_e2ee_account PRIVATE pcp_core)
    target_compile_options(test_e2ee_account PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME e2ee_account COMMAND test_e2ee_account)

    add_executable(test_e2ee_otk_count tests/test_e2ee_otk_count.cpp)
    target_link_libraries(test_e2ee_otk_count PRIVATE pcp_core)
    target_compile_options(test_e2ee_otk_count PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME e2ee_otk_count COMMAND test_e2ee_otk_count)

    add_executable(test_e2ee_sas tests/test_e2ee_sas.cpp)
    target_link_libraries(test_e2ee_sas PRIVATE pcp_core)
    target_compile_options(test_e2ee_sas PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME e2ee_sas COMMAND test_e2ee_sas)

    add_executable(test_e2ee_verify_protocol tests/test_e2ee_verify_protocol.cpp)
    target_link_libraries(test_e2ee_verify_protocol PRIVATE pcp_core)
    target_compile_options(test_e2ee_verify_protocol PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME e2ee_verify_protocol COMMAND test_e2ee_verify_protocol)

    add_executable(test_e2ee_store tests/test_e2ee_store.cpp)
    target_link_libraries(test_e2ee_store PRIVATE pcp_core)
    target_compile_options(test_e2ee_store PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME e2ee_store COMMAND test_e2ee_store)

    # Live-Synapse E2EE integration test (CI: synapse-e2ee workflow).
    # Skips gracefully when no server is reachable (no SYNAPSE_URL set).
    add_executable(test_synapse_e2ee tests/test_synapse_e2ee.cpp)
    target_link_libraries(test_synapse_e2ee PRIVATE pcp_core)
    target_compile_options(test_synapse_e2ee PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    add_test(NAME synapse_e2ee COMMAND test_synapse_e2ee)
    set_tests_properties(synapse_e2ee PROPERTIES TIMEOUT 120)

    add_executable(test_phase4 tests/test_phase4.cpp)
    add_test(NAME phase4 COMMAND test_phase4)

    # GUI test: requires Qt (automatic via ENABLE_AUTOMOC)
    add_executable(test_gui_phase4 tests/test_gui_phase4.cpp)
    target_link_libraries(test_gui_phase4 PRIVATE Qt6::Widgets)
    set_target_properties(test_gui_phase4 PROPERTIES AUTOMOC ON)
    add_test(NAME gui_phase4 COMMAND test_gui_phase4)
    set_tests_properties(gui_phase4 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test_visual tests/test_visual.cpp)
    target_link_libraries(test_visual PRIVATE
        pcp_ui_handlers
        pcp_ui_widgets
        pcp_core
        Qt6::Widgets
        Qt6::Test
    )
    target_compile_options(test_visual PRIVATE
        -include "${pcp-core_SOURCE_DIR}/native_shim/pcp_compat.h")
    target_include_directories(test_visual PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(test_visual PROPERTIES AUTOMOC ON)
    add_test(NAME visual COMMAND test_visual)
    set_tests_properties(visual PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # --- Ignore test fixtures from git ---
    set_tests_properties(phase1 PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# --- Install (basic, for packaging later) ---
install(TARGETS pcp-desktop RUNTIME DESTINATION bin)
