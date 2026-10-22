# Product identity for Skaar (a fork of ioquake3).
# Keep these in sync with the STANDALONE block in code/qcommon/q_shared.h.
set(PROJECT_NAME skaar)
set(PROJECT_VERSION 0.1)

set(SERVER_NAME skaarded)
set(CLIENT_NAME skaar)

set(BASEGAME base)

set(CGAME_MODULE cgame)
set(GAME_MODULE qagame)
set(UI_MODULE ui)

# TODO: replace with Skaar logo icons (misc/skaar/) once the source image is in the repo
set(WINDOWS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/windows/quake3.ico)

set(MACOS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/macos/quake3_flat.icns)
set(MACOS_BUNDLE_ID org.skaar.${CLIENT_NAME})

set(COPYRIGHT "Skaar, based on ioquake3. Quake III Arena source code Copyright (C) 1999-2005 id Software, Inc. GPLv2.")

set(CONTACT_EMAIL "")
set(PROTOCOL_HANDLER_SCHEME skaar)
