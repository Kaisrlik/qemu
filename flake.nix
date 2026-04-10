{
  description = "esc-qemu";
  inputs.nixpkgs.url = "nixpkgs/nixos-unstable-small";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      packages.x86_64-linux.default = pkgs.callPackage ./qemu.nix {
        ncursesSupport = false;
        enableDocs = false;
        hostCpuOnly = true;
        hostCpuTargets = [ "riscv32-softmmu" ];
        nixosTestRunner = false;
        guestAgentSupport = false;
      };

      devShells.x86_64-linux.default = pkgs.mkShell {
        packages = with pkgs; [
          zlib
          pkg-config
          glib
          pixman flex
          bison  lzo  libaio  libtasn1  gnutls  nettle  curl  dtc  ninja
          meson attr  libcap  libcap_ng  socat  numactl
          OVMF.fd
          (pkgs.python3.withPackages (ps: with ps; [
            distlib
            meson
            pycotap
            qemu-qmp
            setuptools
            pip
            wheel
          ]))
        ];
      };
    };
}
