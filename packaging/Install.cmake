# Release-only, portable standalone layout. Never install the live deploy config/save.
# Scan the PE dependency graph: TARGET_RUNTIME_DLLS alone misses libraries
# imported as UNKNOWN targets (OpenSSL, JSONCPP, zlib, Brotli).
install(CODE [[
    if(POLICY CMP0207)
        cmake_policy(SET CMP0207 NEW)
    endif()
]] CONFIGURATIONS Release)
install(TARGETS gimuserverw RUNTIME_DEPENDENCY_SET standalone_runtime
    RUNTIME DESTINATION . CONFIGURATIONS Release)
install(RUNTIME_DEPENDENCY_SET standalone_runtime
    DIRECTORIES "$<TARGET_FILE_DIR:gimuserverw>"
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    POST_EXCLUDE_REGEXES [=[.*[/\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\].*]=]
    RUNTIME DESTINATION . CONFIGURATIONS Release)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION . CONFIGURATIONS Release)
install(FILES
    "${CMAKE_SOURCE_DIR}/packaging/config.json"
    "${CMAKE_SOURCE_DIR}/packaging/Start Server.bat"
    "${CMAKE_SOURCE_DIR}/packaging/Check-Package.ps1"
    "${CMAKE_SOURCE_DIR}/packaging/Enable Client Loopback.ps1"
    "${CMAKE_SOURCE_DIR}/packaging/README.txt"
    "${CMAKE_SOURCE_DIR}/LICENSE"
    DESTINATION . CONFIGURATIONS Release)
foreach(folder IN ITEMS archive mst system)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/deploy/${folder}/"
        DESTINATION "${folder}" CONFIGURATIONS Release
        FILES_MATCHING PATTERN "*.json")
endforeach()

# Carry notices with the compiled libraries/header runtimes shipped in the ZIP.
file(GLOB dependency_notices "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/*/copyright")
foreach(notice IN LISTS dependency_notices)
    get_filename_component(notice_dir "${notice}" DIRECTORY)
    get_filename_component(dependency_name "${notice_dir}" NAME)
    install(FILES "${notice}" DESTINATION "licenses/${dependency_name}"
        RENAME copyright.txt CONFIGURATIONS Release)
endforeach()
install(FILES "${CMAKE_SOURCE_DIR}/packet-generator/LICENSE"
    DESTINATION "licenses/packet-generator" CONFIGURATIONS Release)
install(FILES "${CMAKE_SOURCE_DIR}/packaging/date-LICENSE.txt"
    DESTINATION "licenses/date" CONFIGURATIONS Release)
