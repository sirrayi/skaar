# Build-time script: symlink every top-level entry of data/<BASEGAME> into the
# build output's <BASEGAME> dir, so the engine reads source data directly and
# data edits never need a rebuild. Run via `cmake -P` from the skaar_data target.
#   -DDATA_DIR=<repo>/data/base -DOUT_DIR=<build>/<config>/base

file(MAKE_DIRECTORY ${OUT_DIR})
file(GLOB ENTRIES RELATIVE ${DATA_DIR} ${DATA_DIR}/*)

foreach(ENTRY ${ENTRIES})
    set(LINK ${OUT_DIR}/${ENTRY})
    if(IS_SYMLINK ${LINK})
        file(READ_SYMLINK ${LINK} CURRENT)
        if(CURRENT STREQUAL ${DATA_DIR}/${ENTRY})
            continue()
        endif()
        file(REMOVE ${LINK})
    elseif(EXISTS ${LINK})
        message(WARNING "skaar_data: ${LINK} exists and is not a symlink, leaving it")
        continue()
    endif()
    file(CREATE_LINK ${DATA_DIR}/${ENTRY} ${LINK} SYMBOLIC)
endforeach()
