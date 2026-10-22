# Skaar host tools (C++20) and the maps they generate.

enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NLOHMANN_JSON_DIR ${SOURCE_DIR}/thirdparty/nlohmann-json-3.11.3)

add_executable(mapc ${CMAKE_SOURCE_DIR}/tools/mapc/mapc.cpp)
target_include_directories(mapc PRIVATE ${NLOHMANN_JSON_DIR})
skaar_strict_target(mapc)
set_target_properties(mapc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)

# data/maps-src/<name>.json -> <out>/<BASEGAME>/generated.pk3dir/maps/<name>.bsp
# (a .pk3dir is a directory the engine mounts like a pk3)
if(CMAKE_CONFIGURATION_TYPES)
    set(SKAAR_GENERATED_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${BASEGAME}/generated.pk3dir)
else()
    set(SKAAR_GENERATED_DIR ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${BASEGAME}/generated.pk3dir)
endif()

file(GLOB SKAAR_MAP_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/data/maps-src/*.json)
set(SKAAR_MAP_OUTPUTS)
foreach(MAP_SOURCE ${SKAAR_MAP_SOURCES})
    get_filename_component(MAP_NAME ${MAP_SOURCE} NAME_WE)
    set(MAP_OUTPUT ${SKAAR_GENERATED_DIR}/maps/${MAP_NAME}.bsp)
    add_custom_command(
        OUTPUT ${MAP_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SKAAR_GENERATED_DIR}/maps
        COMMAND mapc ${MAP_SOURCE} ${MAP_OUTPUT}
        DEPENDS mapc ${MAP_SOURCE}
        COMMENT "mapc ${MAP_NAME}"
        VERBATIM)
    list(APPEND SKAAR_MAP_OUTPUTS ${MAP_OUTPUT})
endforeach()

add_custom_target(skaar_maps ALL DEPENDS ${SKAAR_MAP_OUTPUTS})
