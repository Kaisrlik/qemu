{
  description = "esc-qemu";
  inputs.nixpkgs.url = "nixpkgs/nixos-unstable-small";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      packages.x86_64-linux.default = pkgs.callPackage ./qemu.nix {
        numaSupport = true;
        seccompSupport = false;
        alsaSupport = true;
        pulseSupport = true;
        pipewireSupport = false;
        sdlSupport = false;
        jackSupport = false;
        gtkSupport = false;
        vncSupport = true;
        smartcardSupport = false;
        ncursesSupport = false;
        usbredirSupport = false;
        xenSupport = false;
        cephSupport = false;
        glusterfsSupport = false;
        openGLSupport = false;
        rutabagaSupport = false;
        virglSupport = false;
        libiscsiSupport = false;
        smbdSupport = false;
        tpmSupport = false;
        uringSupport = false;
        canokeySupport = false;
        capstoneSupport = false;
        enableDocs = false;
        hostCpuOnly = true;
        hostCpuTargets = [ "x86_64-softmmu" ];
        nixosTestRunner = false;
        toolsOnly = false;
        xml2Support = true;
      };

      devShells.x86_64-linux.default = pkgs.mkShell {
        packages = with pkgs; [
          zlib
          pkg-config
          glib
          pixman  vde2  alsa-lib  texinfo  flex
          bison  lzo  snappy  libaio  libtasn1  gnutls  nettle  curl  dtc  ninja
          meson attr  libcap  libcap_ng  socat  libslirp numactl
          spice spice-protocol
          OVMF.fd
          libxml2
          python313Packages.distlib
        ];
      };
    };
}
