{
  description = "Cardinal — FHS build environment for CardinalWasmDSP";

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
        # and locally when developing the CardinalWasmDSP subproject.
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
          ];

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
