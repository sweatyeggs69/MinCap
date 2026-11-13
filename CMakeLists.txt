cmake_minimum_required(VERSION 3.24)
# Windows: static MSVC runtime so MinCap.exe runs without installing the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(mincap LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

option(MINCAP_FETCH_SDL "Download and build SDL3 if it is not installed" ON)
option(MINCAP_BUILD_TESTS "Build headless tests" ON)
option(MINCAP_BUILD_TOOLS "Build analysis tools (mincap_disasm, needs Capstone)" ON)
option(MINCAP_MACOS_BUNDLE "Build MinCap.app (and the `dmg` target) on macOS" OFF)
option(MINCAP_VENDOR_SDL "Always build SDL3 from source (static) instead of using a system install" OFF)
option(MINCAP_PACKAGE "Add packaging targets (Windows zip, Linux AppDir install)" OFF)

# App icon renderer: the MC mark (Kyle's artwork) on a rounded tile; used for the macOS .icns, Windows .ico
# and the Linux icon. Reads the logo with stb_image (public domain, pinned).
include(FetchContent)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 4e9b368bfc071e97206ea5c6adba68dd0cbf8897)
FetchContent_MakeAvailable(stb)
add_executable(mincap_icon tools/mincap_icon/main.cpp)
target_include_directories(mincap_icon SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_compile_definitions(mincap_icon PRIVATE MINCAP_ICON_LOGO="${PROJECT_SOURCE_DIR}/assets/icon/mincap-mark.png")
set_property(TARGET mincap_icon APPEND PROPERTY OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/assets/icon/mincap-mark.png")

# Core: ROM loading and verification. No SDL dependency, so it can be tested headless.
add_library(mincap_core STATIC
    src/rom.cpp
    src/sha1.cpp
)
target_include_directories(mincap_core PUBLIC src)
if(MSVC)
    target_compile_options(mincap_core PRIVATE /W4)
else()
    target_compile_options(mincap_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Save import/export and backups (no SDL).
add_library(mincap_saves STATIC src/app/save_import.cpp src/app/fsutil.cpp)
target_include_directories(mincap_saves PUBLIC src)
if(WIN32)
    target_link_libraries(mincap_saves PUBLIC shell32 ole32 uuid)
endif()
if(NOT MSVC)
    target_compile_options(mincap_saves PRIVATE -Wall -Wextra)
endif()

# Save state slots (no SDL). Uses the system zlib when present (macOS/Linux); otherwise states
# are stored uncompressed.
add_library(mincap_states STATIC src/app/states.cpp)
target_include_directories(mincap_states PUBLIC src)
target_link_libraries(mincap_states PUBLIC mincap_gba mincap_saves)
target_compile_definitions(mincap_states PRIVATE MINCAP_VERSION="${PROJECT_VERSION}")
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(mincap_states PRIVATE ZLIB::ZLIB)
    target_compile_definitions(mincap_states PRIVATE MINCAP_HAVE_ZLIB=1)
endif()
if(NOT MSVC)
    target_compile_options(mincap_states PRIVATE -Wall -Wextra)
endif()

# Updates from GitHub Releases: versions, release JSON, SHA-256, zip unpacking. No SDL.
add_library(mincap_update_core STATIC src/update/sha256.cpp src/update/update_core.cpp)
target_include_directories(mincap_update_core PUBLIC src)
if(ZLIB_FOUND)
    target_link_libraries(mincap_update_core PRIVATE ZLIB::ZLIB)
    target_compile_definitions(mincap_update_core PRIVATE MINCAP_HAVE_ZLIB=1)
endif()
if(MSVC)
    target_compile_options(mincap_update_core PRIVATE /W4)
else()
    target_compile_options(mincap_update_core PRIVATE -Wall -Wextra)
endif()

# Display post-processing (scaling layout, GBA colors, frame blending, overlays). No SDL.
add_library(mincap_display STATIC src/options/display.cpp)
target_include_directories(mincap_display PUBLIC src)
if(MSVC)
    target_compile_options(mincap_display PRIVATE /W4)
else()
    target_compile_options(mincap_display PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GBA runtime (interpreter, PPU, APU, BIOS HLE). No SDL dependency.
add_library(mincap_gba STATIC
    src/gba/apu.cpp
    src/gba/bios.cpp
    src/gba/bus.cpp
    src/gba/cpu.cpp
    src/gba/ppu.cpp
    src/gba/state.cpp
    src/gba/system.cpp
    src/gba/wide_level.cpp
)
target_include_directories(mincap_gba PUBLIC src)
if(MSVC)
    target_compile_options(mincap_gba PRIVATE /W4)
else()
    target_compile_options(mincap_gba PRIVATE -Wall -Wextra)
endif()

# SDL3: use a system install if present (brew install sdl3, pacman -S sdl3, vcpkg), else fetch it.
if(NOT MINCAP_VENDOR_SDL)
    find_package(SDL3 CONFIG QUIET)
endif()
if(NOT SDL3_FOUND AND MINCAP_FETCH_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.26
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

if(TARGET SDL3::SDL3)
    # Dear ImGui (core + SDL3 platform + SDL_Renderer3 backends), pinned.
    include(FetchContent)
    FetchContent_Declare(imgui_src
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(imgui_src)
    add_library(imgui STATIC
        ${imgui_src_SOURCE_DIR}/imgui.cpp
        ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_src_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    target_include_directories(imgui PUBLIC ${imgui_src_SOURCE_DIR} ${imgui_src_SOURCE_DIR}/backends)
    target_link_libraries(imgui PUBLIC SDL3::SDL3)

    # RetroAchievements: rcheevos (MIT) for the rc_client runtime, and stb_image (public domain)
    # to decode PNG badges. Both pinned; built from source as plain C/C++ files.
    FetchContent_Declare(rcheevos
        GIT_REPOSITORY https://github.com/RetroAchievements/rcheevos.git
        GIT_TAG v12.5.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(rcheevos)
    set(rc ${rcheevos_SOURCE_DIR}/src)
    add_library(rcheevos STATIC
        ${rc}/rc_client.c ${rc}/rc_compat.c ${rc}/rc_util.c ${rc}/rc_version.c
        ${rc}/rapi/rc_api_common.c ${rc}/rapi/rc_api_info.c ${rc}/rapi/rc_api_runtime.c ${rc}/rapi/rc_api_user.c
        ${rc}/rcheevos/alloc.c ${rc}/rcheevos/condition.c ${rc}/rcheevos/condset.c ${rc}/rcheevos/consoleinfo.c
        ${rc}/rcheevos/format.c ${rc}/rcheevos/lboard.c ${rc}/rcheevos/memref.c ${rc}/rcheevos/operand.c
        ${rc}/rcheevos/rc_validate.c ${rc}/rcheevos/richpresence.c ${rc}/rcheevos/runtime.c
        ${rc}/rcheevos/runtime_progress.c ${rc}/rcheevos/trigger.c ${rc}/rcheevos/value.c
        ${rc}/rhash/md5.c)
    target_include_directories(rcheevos PUBLIC ${rcheevos_SOURCE_DIR}/include ${rc})
    target_include_directories(rcheevos SYSTEM PUBLIC ${stb_SOURCE_DIR})
    set_target_properties(rcheevos PROPERTIES C_STANDARD 99)

    # Options panel (owned by the options work; display.* / input.* settings).
    add_library(mincap_options STATIC
        src/options/options_panel.cpp
        src/options/presenter.cpp
        src/options/input.cpp
    )
    target_include_directories(mincap_options PUBLIC src)
    target_link_libraries(mincap_options PUBLIC imgui mincap_display mincap_app mincap_states SDL3::SDL3)

    # App layer: settings, launcher UI. Uses SDL (pref path, dialogs).
    add_library(mincap_app STATIC
        src/app/config.cpp
    )
    target_include_directories(mincap_app PUBLIC src)
    target_link_libraries(mincap_app PUBLIC SDL3::SDL3)

    # Launcher: first-run ROM setup, themed menu, shortcuts, game session. The DM Sans font
    # (SIL OFL) and a few Lucide icons (ISC), both in assets/fonts, are compiled in.
    set(mincap_font_cpp "")
    foreach(pair "dmsans_regular;DMSans-Regular.ttf" "dmsans_semibold;DMSans-SemiBold.ttf"
                 "dmsans_bold;DMSans-Bold.ttf" "lucide_icons;lucide-icons.ttf")
        list(GET pair 0 name)
        list(GET pair 1 file)
        set(out "${CMAKE_CURRENT_BINARY_DIR}/font_${name}.cpp")
        add_custom_command(OUTPUT "${out}"
            COMMAND "${CMAKE_COMMAND}" -DINPUT=${PROJECT_SOURCE_DIR}/assets/fonts/${file}
                    -DOUTPUT=${out} -DSYMBOL=mincap_font_${name} -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
            DEPENDS assets/fonts/${file} cmake/embed.cmake
            COMMENT "Embedding ${file}")
        list(APPEND mincap_font_cpp "${out}")
    endforeach()
    # The MinCap wordmark on the launcher card (assets/logo/mincap-logo.png, original artwork).
    # Always embedded: an optional EXISTS check was only evaluated at configure time, so build
    # folders configured before the file was added never picked it up.
    set(out "${CMAKE_CURRENT_BINARY_DIR}/image_logo.cpp")
    add_custom_command(OUTPUT "${out}"
        COMMAND "${CMAKE_COMMAND}" -DINPUT=${PROJECT_SOURCE_DIR}/assets/logo/mincap-logo.png -DOUTPUT=${out}
                -DSYMBOL=mincap_image_logo -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
        DEPENDS assets/logo/mincap-logo.png cmake/embed.cmake
        COMMENT "Embedding the MinCap logo")
    list(APPEND mincap_font_cpp "${out}")
    # The app icon (512x512, rendered by mincap_icon), written into the shortcuts the launcher
    # creates so they carry the MinCap icon even from a plain build/mincap.
    set(icon_png "${CMAKE_CURRENT_BINARY_DIR}/mincap-icon-512.png")
    set(out "${CMAKE_CURRENT_BINARY_DIR}/image_icon.cpp")
    add_custom_command(OUTPUT "${out}"
        COMMAND mincap_icon --png 512 "${icon_png}"
        COMMAND "${CMAKE_COMMAND}" -DINPUT=${icon_png} -DOUTPUT=${out}
                -DSYMBOL=mincap_image_icon -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
        DEPENDS mincap_icon cmake/embed.cmake
        COMMENT "Embedding the MinCap app icon")
    list(APPEND mincap_font_cpp "${out}")
    # License texts compiled in (shown in F1 > About and by --licenses).
    foreach(pair "license;LICENSE" "notices;THIRD_PARTY_NOTICES.md" "dmsans_ofl;assets/fonts/DMSans-OFL.txt")
        list(GET pair 0 name)
        list(GET pair 1 file)
        set(out "${CMAKE_CURRENT_BINARY_DIR}/text_${name}.cpp")
        add_custom_command(OUTPUT "${out}"
            COMMAND "${CMAKE_COMMAND}" -DINPUT=${PROJECT_SOURCE_DIR}/${file} -DOUTPUT=${out}
                    -DSYMBOL=mincap_text_${name} -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
            DEPENDS ${file} cmake/embed.cmake
            COMMENT "Embedding ${file}")
        list(APPEND mincap_font_cpp "${out}")
    endforeach()
    add_library(mincap_launcher STATIC
        src/app/achievements.cpp
        src/app/credentials.cpp
        src/app/http.cpp
        src/app/app.cpp
        src/app/platform.cpp
        src/app/theme.cpp
        src/app/theme_colors.cpp
        src/app/licenses.cpp
        src/app/toast.cpp
        ${mincap_font_cpp}
    )
    target_include_directories(mincap_launcher PUBLIC src)
    target_link_libraries(mincap_launcher PUBLIC mincap_options mincap_gba mincap_core mincap_saves mincap_states imgui SDL3::SDL3)
    target_link_libraries(mincap_launcher PRIVATE rcheevos)
    target_compile_definitions(mincap_launcher PRIVATE MINCAP_VERSION="${PROJECT_VERSION}")
    if(APPLE)
        target_link_libraries(mincap_launcher PRIVATE "-framework Security" "-framework CoreFoundation")
    elseif(WIN32)
        target_link_libraries(mincap_launcher PRIVATE winhttp advapi32)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(mincap_launcher PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
    endif()
    if(NOT MSVC)
        target_compile_options(mincap_launcher PRIVATE -Wall -Wextra)
    endif()

    # In-app updater: downloads (WinHTTP / curl), install per OS, and its settings tab.
    add_library(mincap_updater STATIC src/update/http.cpp src/update/updater.cpp src/update/update_ui.cpp)
    target_include_directories(mincap_updater PUBLIC src)
    target_link_libraries(mincap_updater PUBLIC mincap_update_core mincap_app imgui SDL3::SDL3)
    target_compile_definitions(mincap_updater PUBLIC MINCAP_VERSION="${PROJECT_VERSION}")
    if(WIN32)
        target_link_libraries(mincap_updater PRIVATE winhttp)
    endif()
    if(NOT MSVC)
        target_compile_options(mincap_updater PRIVATE -Wall -Wextra)
    endif()
    target_link_libraries(mincap_launcher PUBLIC mincap_updater)

    add_executable(mincap src/main.cpp)
    if(WIN32)
        # GUI program (no console window), MinCap.exe with the app icon and version info.
        set(mincap_ico "${CMAKE_CURRENT_BINARY_DIR}/mincap.ico")
        add_custom_command(OUTPUT "${mincap_ico}" COMMAND mincap_icon --ico "${mincap_ico}" DEPENDS mincap_icon
                           COMMENT "Rendering mincap.ico")
        configure_file(packaging/windows/mincap.rc.in "${CMAKE_CURRENT_BINARY_DIR}/mincap.rc" @ONLY)
        target_sources(mincap PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/mincap.rc" "${mincap_ico}")
        set_target_properties(mincap PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME MinCap)
        target_link_libraries(mincap PRIVATE ole32 shell32 uuid)
    endif()
    target_link_libraries(mincap PRIVATE mincap_launcher mincap_core mincap_gba mincap_options imgui SDL3::SDL3)
    if(APPLE AND MINCAP_MACOS_BUNDLE)
        include(cmake/MacBundle.cmake)
        mincap_configure_bundle(mincap)
    elseif(APPLE)
        # Plain (unbundled) build/mincap: embed an Info.plist so the menu bar says MinCap, not mincap.
        set(mincap_plist "${CMAKE_CURRENT_BINARY_DIR}/mincap-embedded-Info.plist")
        file(WRITE "${mincap_plist}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\"><dict>\n"
            "<key>CFBundleName</key><string>MinCap</string>\n"
            "<key>CFBundleDisplayName</key><string>MinCap</string>\n"
            "<key>CFBundleShortVersionString</key><string>${PROJECT_VERSION}</string>\n"
            "</dict></plist>\n")
        target_link_options(mincap PRIVATE "LINKER:-sectcreate,__TEXT,__info_plist,${mincap_plist}")
        set_property(TARGET mincap APPEND PROPERTY LINK_DEPENDS "${mincap_plist}")
    endif()
    if(MINCAP_PACKAGE AND WIN32)
        # Release file: the single self-contained MinCap.exe (static runtime, SDL, zlib; licenses
        # embedded, see F1 > About and --licenses), copied under its release name.
        set(mincap_release_exe "${CMAKE_CURRENT_BINARY_DIR}/MinCap-${PROJECT_VERSION}-Windows-x64.exe")
        add_custom_target(package_exe
            COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:mincap>" "${mincap_release_exe}"
            DEPENDS mincap
            COMMENT "Building ${mincap_release_exe}"
            VERBATIM)
    endif()
    if(MINCAP_PACKAGE AND UNIX AND NOT APPLE)
        # Portable Linux binary: static libstdc++/libgcc; SDL is static and loads X11/Wayland/
        # ALSA/PulseAudio/PipeWire at runtime. Installed into an AppDir by the workflow.
        target_link_options(mincap PRIVATE -static-libstdc++ -static-libgcc)
        set(mincap_png "${CMAKE_CURRENT_BINARY_DIR}/mincap.png")
        add_custom_command(OUTPUT "${mincap_png}" COMMAND mincap_icon --png 256 "${mincap_png}" DEPENDS mincap_icon
                           COMMENT "Rendering mincap.png")
        add_custom_target(mincap_linux_icon ALL DEPENDS "${mincap_png}")
        install(TARGETS mincap RUNTIME DESTINATION bin)
        install(FILES packaging/linux/mincap.desktop DESTINATION share/applications)
        install(FILES "${mincap_png}" DESTINATION share/icons/hicolor/256x256/apps)
        install(FILES LICENSE THIRD_PARTY_NOTICES.md packaging/README-Linux.txt DESTINATION share/doc/mincap)
        install(FILES assets/fonts/DMSans-OFL.txt assets/fonts/Lucide-LICENSE.txt DESTINATION share/doc/mincap)
    endif()
else()
    message(WARNING "SDL3 not found and MINCAP_FETCH_SDL is OFF: only building mincap_core")
endif()

if(MINCAP_BUILD_TESTS)
    enable_testing()
    add_executable(mincap_romcheck tests/romcheck.cpp)
    target_link_libraries(mincap_romcheck PRIVATE mincap_core)
    add_test(NAME sha1_selftest COMMAND mincap_romcheck --selftest)
    add_executable(mincap_display_test tests/display_test.cpp)
    target_link_libraries(mincap_display_test PRIVATE mincap_display)
    add_test(NAME display_options COMMAND mincap_display_test)
    add_executable(mincap_save_test tests/save_import_test.cpp)
    target_link_libraries(mincap_save_test PRIVATE mincap_saves)
    add_test(NAME save_import COMMAND mincap_save_test)
    add_executable(mincap_states_test tests/states_test.cpp)
    target_link_libraries(mincap_states_test PRIVATE mincap_states)
    add_test(NAME save_state_slots COMMAND mincap_states_test)
    add_executable(mincap_rewind_test tests/rewind_test.cpp)
    target_include_directories(mincap_rewind_test PRIVATE src)
    add_test(NAME rewind_history COMMAND mincap_rewind_test)
    add_executable(mincap_cheats_test tests/cheats_test.cpp)
    target_include_directories(mincap_cheats_test PRIVATE src)
    add_test(NAME cheats COMMAND mincap_cheats_test)
    add_executable(mincap_update_test tests/update_test.cpp)
    target_link_libraries(mincap_update_test PRIVATE mincap_update_core)
    target_compile_definitions(mincap_update_test PRIVATE MINCAP_TEST_DATA="${PROJECT_SOURCE_DIR}/tests/data")
    add_test(NAME updates COMMAND mincap_update_test)
    add_executable(mincap_state_test tests/state_test.cpp)
    target_link_libraries(mincap_state_test PRIVATE mincap_gba mincap_core)
    # Uses the user's own ROM when it sits in the source folder (it is never committed).
    target_compile_definitions(mincap_state_test PRIVATE
        MINCAP_TEST_ROM="${PROJECT_SOURCE_DIR}/Minish Cap.gba"
        MINCAP_TEST_BIOS="${PROJECT_SOURCE_DIR}/gba_bios.bin")
    add_test(NAME save_state_determinism COMMAND mincap_state_test)
    set_tests_properties(save_state_determinism PROPERTIES SKIP_RETURN_CODE 77)
    if(TARGET mincap_launcher)
        # RetroAchievements glue against canned server replies (offline).
        add_executable(mincap_achievements_test tests/achievements_test.cpp)
        target_link_libraries(mincap_achievements_test PRIVATE mincap_launcher rcheevos)
        add_test(NAME achievements COMMAND mincap_achievements_test)
        # Controller bindings driven through a virtual SDL gamepad.
        add_executable(mincap_input_test tests/input_test.cpp)
        target_link_libraries(mincap_input_test PRIVATE mincap_launcher)
        add_test(NAME input COMMAND mincap_input_test)
    endif()
endif()

# Analysis tools. Their output is derived from the ROM and goes to generated/ (gitignored).
if(MINCAP_BUILD_TOOLS)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CAPSTONE IMPORTED_TARGET capstone>=5)
    endif()
    if(TARGET PkgConfig::CAPSTONE)
        add_executable(mincap_disasm tools/mincap_disasm/main.cpp)
        target_link_libraries(mincap_disasm PRIVATE mincap_core PkgConfig::CAPSTONE)
        if(NOT MSVC)
            target_compile_options(mincap_disasm PRIVATE -Wall -Wextra)
        endif()
    else()
        message(WARNING "Capstone not found via pkg-config: skipping mincap_disasm (brew install capstone pkgconf)")
    endif()
endif()
