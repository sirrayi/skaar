# Unit tests for the engine-free game logic (game/shared): `ctest` or
# <build>/tests/skaar_tests. scripts/smoke.sh runs them after the safe build.

enable_testing()

set(SKAAR_GAME_DIR ${CMAKE_SOURCE_DIR}/game)

file(GLOB SKAAR_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp)
add_executable(skaar_tests ${SKAAR_TEST_SOURCES}
    ${SKAAR_GAME_DIR}/shared/scenario.cpp
    ${SKAAR_GAME_DIR}/shared/runlog.cpp
    ${SKAAR_GAME_DIR}/shared/insights.cpp
    ${SKAAR_GAME_DIR}/shared/trainer_sim.cpp)
target_include_directories(skaar_tests PRIVATE ${SOURCE_DIR} ${SKAAR_GAME_DIR} ${NLOHMANN_JSON_DIR})
target_compile_definitions(skaar_tests PRIVATE SKAAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
skaar_strict_target(skaar_tests)
set_target_properties(skaar_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

add_test(NAME skaar_tests COMMAND skaar_tests)
