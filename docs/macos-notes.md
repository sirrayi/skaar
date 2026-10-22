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

## Command Line Tools older than the OS: lldb and ASan break (macOS 27 + CLT 17)
With macOS 27.0.1 and the Command Line Tools from Xcode 16 (Apple clang 17, lldb-1700):
- `lldb` crashes with SIGBUS in `ObjectFileMachO::ParseSymtab` as soon as it creates a target, even for `/bin/echo`.
- AddressSanitizer binaries deadlock at startup (`__asan::InitializeShadowMemory` → `dyld_shared_cache_iterate_text_swift` → malloc).
  An empty `int main(){}` built with `-fsanitize=address` never exits.
- UBSan and the compiler itself work fine.

Fix: install the matching tools (`softwareupdate --list` shows "Command Line Tools for Xcode 27.0"):
```sh
softwareupdate --install "Command Line Tools for Xcode 27.0-27.0"
```
Until then the scripts degrade on their own and say so:
- `dev.sh` probes lldb and runs without a debugger. The engine's crash handler still writes `logs/crash-*.txt` with a backtrace.
- `build.sh safe` probes ASan and builds with UBSan only.
After updating, delete `build-safe/` (or just run `build.sh safe`, which re-probes) to get ASan back.

## Writing to a pipe path that doesn't exist yet creates a file
`echo cmd > .home/base/dev.pipe` before the game has created the FIFO makes a regular file. The engine then can't create the
pipe and silently runs without one. The scripts only write when `[ -p path ]` is true.
