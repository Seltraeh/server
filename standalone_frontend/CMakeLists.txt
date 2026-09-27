option(GIMU_ENABLE_DEBUG_CLI "Include the optional developer console" ON)
file(GLOB SRC CONFIGURE_DEPENDS "*.cpp" "*.hpp")
if (NOT GIMU_ENABLE_DEBUG_CLI)
    list(FILTER SRC EXCLUDE REGEX "/DebugCli\\.(cpp|hpp)$")
endif()
add_executable(gimuserverw ${SRC})
target_link_libraries(gimuserverw PRIVATE gimuserver)
set_property(SOURCE ${SRC} APPEND PROPERTY OBJECT_DEPENDS
    "${PKGEN_OUT_HEADER};${PKGEN_ARCHIVE_OUT_HEADER}")

# Shared developer console; portable release presets disable it explicitly.
if (GIMU_ENABLE_DEBUG_CLI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DebugCli.cpp")
    target_compile_definitions(gimuserverw PRIVATE BF_DEBUG_CLI)
    message(STATUS "standalone_frontend: debug CLI enabled (DebugCli.cpp present)")
endif()

# F5 must work after a direct IDE configure, before Build-Dev.ps1 has run.
# Preserve local configuration and saves across all subsequent configurations.
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/deploy/config.json")
    configure_file("${CMAKE_SOURCE_DIR}/packaging/config.json"
        "${CMAKE_SOURCE_DIR}/deploy/config.json" COPYONLY)
endif()

# In Debug builds, bake the absolute path to deploy/config.json into the
# binary so F5 from VS (or any launcher) always finds the right config
# regardless of working directory.  Release builds keep the default
# config.json beside the executable so the package works from any cwd.
target_compile_definitions(gimuserverw PRIVATE
    $<$<CONFIG:Debug>:GIMU_DEFAULT_CONFIG_PATH="${CMAKE_SOURCE_DIR}/deploy/config.json">
)

if (WIN32)
    include("${CMAKE_SOURCE_DIR}/packaging/Install.cmake")
endif()
