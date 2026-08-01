{
  description = "Cardinal — FHS build environment (CardinalWasmDSP + native GUI)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # PawPaw + downstream autotools projects (glib, libFLAC, ...)
        # hardcode absolute paths like /usr/bin/file, and PawPaw's downloaded
        # emsdk binaries (LLVM/node/python) need a normal glibc dynamic
        # linker. buildFHSEnv constructs a user-namespace sandbox with a
        # real /usr/bin, /lib, /bin populated from nixpkgs so all of that
        # behaves as it would on Ubuntu.
        #
        # We use this both in the CI workflow (.github/workflows/build-wasmdsp.yml)
        # and locally for the CardinalWasmDSP subproject *and* for the native
        # Cardinal GUI builds (`make jack`, `make native`).
        fhsEnv = pkgs.buildFHSEnv {
          name = "cardinal-fhs";

          targetPkgs = pkgs: with pkgs; [
            # Cardinal CI's original apt deps
            brotli
            gperf
            meson
            ninja

            # Autotools / build systems
            autoconf
            automake
            libtool
            cmake
            gnumake
            pkg-config
            m4
            gettext
            texinfo
            help2man

            # Host compiler — pin to gcc 13 because gcc 15 defaults to C23
            # where `bool` is reserved, and PawPaw builds ancient bundled
            # glib (inside pkg-config-0.28) that uses `bool` as an identifier.
            gcc13
            binutils

            # Fetch + unpack
            curl
            wget
            gnutar
            gzip
            bzip2
            xz
            unzip
            zip
            patch

            # Scripting / build glue
            python3
            perl
            nodejs_22
            file
            which
            findutils
            gnused
            gawk
            gnugrep
            coreutils

            # Runtime libs the emsdk-downloaded LLVM/node/python link against
            stdenv.cc.cc.lib
            zlib
            openssl
            ncurses
            libxml2
            libffi
            expat
            util-linux
            glibc

            git

            # ----------------------------------------------------------------
            # Native Cardinal GUI (`make jack` / `make native`) dependencies.
            # Mirrors the Debian/Ubuntu apt list in .github/workflows/build.yml:
            #   libasound2-dev libdbus-1-dev libgl1-mesa-dev liblo-dev
            #   libsdl2-dev libx11-dev libxcursor-dev libxext-dev libxrandr-dev
            #
            # buildFHSEnv only symlinks each package's default `out` output
            # into /usr, so we also list `.dev` for anything multi-output —
            # otherwise headers and .pc files aren't visible in /usr/include
            # and /usr/lib/pkgconfig.
            # ----------------------------------------------------------------
            # OpenGL (provides /usr/include/GL/gl.h via libglvnd + mesa headers).
            libglvnd libglvnd.dev
            libGLU libGLU.dev
            mesa
            # X11 client libs Cardinal / DPF / GLFW link against.
            libx11 libx11.dev
            libxext libxext.dev
            libxrandr libxrandr.dev
            libxcursor libxcursor.dev
            libxinerama libxinerama.dev
            libxi libxi.dev
            libxfixes libxfixes.dev
            libxrender libxrender.dev
            libpthread-stubs
            libxau libxau.dev
            libxdmcp libxdmcp.dev
            xorgproto
            # Audio / MIDI backends.
            alsa-lib alsa-lib.dev
            libjack2 libjack2.dev
            libpulseaudio libpulseaudio.dev
            # Cardinal-specific runtime deps.
            liblo
            dbus dbus.dev
            freetype freetype.dev
            fontconfig fontconfig.dev
            SDL2 SDL2.dev
          ];

          # Nix-built pkg-config bakes its default search path to its own
          # store lib/share dirs, so it doesn't see the FHS's .pc files.
          # Point it at /usr/{lib,share}/pkgconfig the way Debian does.
          profile = ''
            export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
          '';

          runScript = pkgs.writeShellScript "cardinal-fhs-run" ''
            if [ $# -eq 0 ]; then
              exec bash -l
            else
              exec "$@"
            fi
          '';
        };
      in {
        packages.default = fhsEnv;
        apps.default = {
          type = "app";
          program = "${fhsEnv}/bin/cardinal-fhs";
        };
        devShells.default = pkgs.mkShell {
          packages = [ fhsEnv ];
          shellHook = ''
            echo "Enter the FHS sandbox with: nix run"
            echo "One-shot in sandbox:      nix run . -- <cmd> [args...]"
          '';
        };
      });
}
