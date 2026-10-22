# macOS notes

Platform-specific issues and their fixes, so the Ubuntu setup can be compared later.

## Universal binaries forced by upstream
`cmake/platforms/macos.cmake` hard-set `CMAKE_OSX_ARCHITECTURES` to `arm64;x86_64`, which ignored `-D` on the command line.
Fix: only set it if the user didn't. Dev builds pass `arm64`. (Not relevant on Linux.)

## "Abnormal Exit" modal dialog blocks relaunch
ioq3 writes `<gamedir>/<product>.pid` at startup and removes it on clean exit. If the process is killed
(Ctrl+C, the dev-watch relaunch, a crash), the next start pops an NSAlert and waits for a click. Stdout stays
silent, so it looks like a hang. Fix: Skaar cvar `com_abnormalExitDialog 0` (in `code/sys/sys_main.c`), set by the dev scripts.
Linux has the same code path but `Sys_Dialog` falls back to zenity/kdialog/xmessage, or prints to the console.

## Piped stdout is block-buffered
When the client's stdout goes to a pipe or file, output can sit in the buffer and is lost if the process is killed.
Use `+set logfile 2` (qconsole.log, flushed after every write) as the reliable log. The dev scripts do this.

## OpenGL 4.1 is the ceiling
`GL_VERSION: 4.1 Metal - 91.7` on the M2 Pro. The OpenGL2 renderer runs fine. The upstream build produces no OpenGL
deprecation warnings (ioq3 loads GL functions through SDL rather than including `<OpenGL/gl.h>` APIs directly).
Only 8 `-Wunknown-pragmas` warnings (MSVC pragmas) and the linker note below remain.

## Linker warning
`ld: warning: reducing alignment of section __DATA,__common from 0x8000 to 0x4000` appears when linking the client and server.
It's harmless (comes from a large aligned global in upstream code). We leave it alone.
