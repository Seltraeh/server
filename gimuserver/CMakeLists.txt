file(GLOB_RECURSE SRC "*.cpp" "*.hpp")
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${SRC})
add_library(gimuserver STATIC ${SRC})

# The generated header (packets/all.hpp) is produced by pkgen_generate,
# declared at the project root. Everything in gimuserver transitively
# depends on it via App.hpp, so pin the build-order edge here.
add_dependencies(gimuserver pkgen_generate)

target_link_libraries(gimuserver PUBLIC
    # third-party libraries
    Drogon::Drogon
    cryptopp::cryptopp
    unofficial::sqlite3::sqlite3
    glaze::glaze
    # project
    pkgen_cpp
)

target_include_directories(gimuserver
    PUBLIC
        ../
    PRIVATE
        .
)

target_precompile_headers(gimuserver PRIVATE App.hpp)

if (MSVC)
    target_compile_options(gimuserver PUBLIC "/constexpr:steps8388608")
    # ServerCache.cpp instantiates one glaze LoadJson<T> per cached MST
    # table; past ~50 tables the object file exceeds the default COFF
    # section limit (C1128).
    target_compile_options(gimuserver PRIVATE "/bigobj")
endif()

# MSVC 19.51 emits C4737 for these coroutine-heavy handlers under /O2,
# even without LTCG. Keep the workaround local; all other Release objects
# remain optimized. Recheck when upgrading the compiler.
if (MSVC)
    set_property(SOURCE gme/handlers/Mission.cpp gme/handlers/Present.cpp gme/handlers/UserInfo.cpp
        APPEND PROPERTY COMPILE_OPTIONS "$<$<NOT:$<CONFIG:Debug>>:/Od>")
    # MSVC otherwise restores /O2 from the shared PCH (C4653), ignoring /Od.
    set_source_files_properties(gme/handlers/Mission.cpp gme/handlers/Present.cpp gme/handlers/UserInfo.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()
