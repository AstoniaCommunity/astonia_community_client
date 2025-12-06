// build/tools/macos_launcher.c
// Tiny Mach-O launcher for Astonia.app
//
// Behavior:
//   - Locates the .app bundle at runtime
//   - cd's into Contents/Resources
//   - execs ./bin/moac, forwarding all command-line args

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <mach-o/dyld.h>

int main(int argc, char **argv) {
    char exe_path[PATH_MAX];
    uint32_t size = sizeof(exe_path);

    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        fprintf(stderr, "astonia launcher: _NSGetExecutablePath buffer too small\n");
        return 1;
    }

    // exe_path -> .../Astonia.app/Contents/MacOS/astonia
    // Get .../Astonia.app/Contents/MacOS
    char *macos_dir = dirname(exe_path);
    if (macos_dir == NULL) {
        fprintf(stderr, "astonia launcher: dirname() failed\n");
        return 1;
    }

    // chdir to Contents/MacOS
    if (chdir(macos_dir) != 0) {
        perror("astonia launcher: chdir to MacOS failed");
        return 1;
    }

    // Then go up to Contents and into Resources:
    //   Contents/MacOS -> Contents/Resources
    if (chdir("../Resources") != 0) {
        perror("astonia launcher: chdir to ../Resources failed");
        return 1;
    }

    // Now CWD = .../Astonia.app/Contents/Resources
    // We want to exec ./bin/moac from here.
    //
    // Reuse argv in-place:
    //   argv[0] becomes "./bin/moac"
    //   argv[1..] are forwarded unchanged
    argv[0] = "./bin/moac";

    execv(argv[0], argv);

    // If we reach here, exec failed
    perror("astonia launcher: execv ./bin/moac failed");
    return 1;
}