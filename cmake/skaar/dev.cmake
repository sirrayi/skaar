# Skaar developer build options. scripts/build.sh sets these; plain CMake
# builds get a release configuration (DEV_ASSERT compiled out).

option(SKAAR_DEV "Developer build: DEV_ASSERT, dev_crash, live session logs" OFF)
set(SKAAR_SANITIZE "" CACHE STRING "Sanitizers to build with, e.g. address,undefined (empty = none)")

if(SKAAR_DEV)
    add_compile_definitions(SKAAR_DEV)
endif()

if(SKAAR_SANITIZE)
    add_compile_options(-fsanitize=${SKAAR_SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${SKAAR_SANITIZE})
endif()

# Warnings for code we own: new files under code/, plus game/, tools/, tests/.
# Upstream ioq3 sources keep their own flags (see cmake/compilers/gnu.cmake).
if(MSVC)
    set(SKAAR_STRICT_FLAGS /W4 /WX)
else()
    set(SKAAR_STRICT_FLAGS -Wall -Wextra -Werror)
endif()

function(skaar_strict_sources)
    set_source_files_properties(${ARGV} PROPERTIES COMPILE_OPTIONS "${SKAAR_STRICT_FLAGS}")
endfunction()

function(skaar_strict_target TARGET)
    target_compile_options(${TARGET} PRIVATE ${SKAAR_STRICT_FLAGS})
endfunction()

skaar_strict_sources(
    ${SOURCE_DIR}/qcommon/dev.c
    ${SOURCE_DIR}/qcommon/cfg_migrate.c
    ${SOURCE_DIR}/qcommon/dev_assert.c
    ${SOURCE_DIR}/sys/sys_crash.c
    ${SOURCE_DIR}/client/cl_dev.c
    ${SOURCE_DIR}/sys/sys_osx_rawmouse.m
)
