{ lib, stdenv, fetchurl, fetchpatch, python3Packages, zlib, pkg-config, glib, buildPackages
, pixman, vde2, alsa-lib, texinfo, flex
, bison, lzo, snappy, libaio, libtasn1, gnutls, nettle, curl, dtc, ninja, meson
, makeWrapper, removeReferencesTo
, attr, libcap, libcap_ng, socat, libslirp
, guestAgentSupport ? (with stdenv.hostPlatform; isLinux || isNetBSD || isOpenBSD || isSunOS || isWindows) && !toolsOnly
, numaSupport ? stdenv.isLinux && !stdenv.isAarch32 && !toolsOnly, numactl
, seccompSupport ? stdenv.isLinux && !toolsOnly, libseccomp
, alsaSupport ? lib.hasSuffix "linux" stdenv.hostPlatform.system && !nixosTestRunner && !toolsOnly
, pulseSupport ? !stdenv.isDarwin && !nixosTestRunner && !toolsOnly, libpulseaudio
, pipewireSupport ? !stdenv.isDarwin && !nixosTestRunner && !toolsOnly, pipewire
, sdlSupport ? !stdenv.isDarwin && !nixosTestRunner && !toolsOnly, SDL2, SDL2_image
, jackSupport ? !stdenv.isDarwin && !nixosTestRunner && !toolsOnly, libjack2
, gtkSupport ? !stdenv.isDarwin && !xenSupport && !nixosTestRunner && !toolsOnly, gtk3, gettext, vte, wrapGAppsHook3
, vncSupport ? !nixosTestRunner && !toolsOnly, libjpeg, libpng
, smartcardSupport ? !nixosTestRunner && !toolsOnly, libcacard
, spiceSupport ? true && !nixosTestRunner && !toolsOnly, spice, spice-protocol
, ncursesSupport ? !nixosTestRunner && !toolsOnly, ncurses
, usbredirSupport ? spiceSupport, usbredir
, xenSupport ? false, xen
, cephSupport ? false, ceph
, glusterfsSupport ? false, glusterfs, libuuid
, openGLSupport ? sdlSupport, mesa, libepoxy, libdrm
, rutabagaSupport ? openGLSupport && !toolsOnly && lib.meta.availableOn stdenv.hostPlatform rutabaga_gfx, rutabaga_gfx
, virglSupport ? openGLSupport, virglrenderer
, libiscsiSupport ? !toolsOnly, libiscsi
, smbdSupport ? false, samba
, tpmSupport ? !toolsOnly
, uringSupport ? stdenv.isLinux, liburing
, canokeySupport ? false, canokey-qemu
, capstoneSupport ? !toolsOnly, capstone
, enableDocs ? true
, hostCpuOnly ? false
, hostCpuTargets ? (if toolsOnly
                    then [ ]
                    else if hostCpuOnly
                    then (lib.optional stdenv.isx86_64 "i386-softmmu"
                          ++ ["${stdenv.hostPlatform.qemuArch}-softmmu"])
                    else null)
, nixosTestRunner ? false
, toolsOnly ? false
, gitUpdater
, qemu-utils # for tests attribute
, xml2Support ? false, libxml2
, OVMF
}:

let
  hexagonSupport = hostCpuTargets == null || lib.elem "hexagon" hostCpuTargets;

  # TODO: sources should be loaded based on current meson configuration, this is
  # far simplier now
  srcs = {
    qemu = ./.;
    keycodemapdb = fetchGit {
      name = "keycodemapdb";
      url = "https://gitlab.com/qemu-project/keycodemapdb.git";
      rev = "f5772a62ec52591ff6870b7e8ef32482371f22c6";
    };
    berkeley-softfloat-3 = fetchGit {
      name = "berkeley-softfloat-3";
      url = "https://gitlab.com/qemu-project/berkeley-softfloat-3.git";
      rev = "b64af41c3276f97f0e181920400ee056b9c88037";
    };
    berkeley-testfloat-3 = fetchGit {
      name = "berkeley-testfloat-3";
      url = "https://gitlab.com/qemu-project/berkeley-testfloat-3.git";
      rev = "e7af9751d9f9fd3b47911f51a5cfd08af256a9ab";
    };
  };
in

stdenv.mkDerivation (finalAttrs: {
  pname = "qemu"
    + lib.optionalString xenSupport "-xen"
    + lib.optionalString hostCpuOnly "-host-cpu-only"
    + lib.optionalString nixosTestRunner "-for-vm-tests"
    + lib.optionalString toolsOnly "-utils";
  version = "8.2.3";

  sourceRoot = ".";
  unpackPhase = ''
    cp -R ${srcs.qemu}/* .
    chmod -R u+w .

    mkdir -p subprojects/keycodemapdb
    cp -R "${srcs.keycodemapdb}"/* subprojects/keycodemapdb

    mkdir -p subprojects/berkeley-softfloat-3
    cp -R "${srcs.berkeley-softfloat-3}"/* subprojects/berkeley-softfloat-3
    cp -R subprojects/packagefiles/berkeley-softfloat-3/* subprojects/berkeley-softfloat-3/

    mkdir -p subprojects/berkeley-testfloat-3
    cp -R "${srcs.berkeley-testfloat-3}"/* subprojects/berkeley-testfloat-3
    cp -R subprojects/packagefiles/berkeley-testfloat-3/* subprojects/berkeley-testfloat-3/

    chmod -R u+w subprojects/
  '';


  depsBuildBuild = [ buildPackages.stdenv.cc ]
    ++ lib.optionals hexagonSupport [ pkg-config ];

  nativeBuildInputs = [
    makeWrapper removeReferencesTo
    pkg-config flex bison dtc meson ninja

    # Don't change this to python3 and python3.pkgs.*, breaks cross-compilation
    python3Packages.python python3Packages.sphinx python3Packages.sphinx-rtd-theme
  ]
    ++ lib.optionals gtkSupport [ wrapGAppsHook3 ]
    ++ lib.optionals hexagonSupport [ glib ];

  buildInputs = [ zlib glib pixman
    vde2 texinfo lzo snappy libtasn1
    gnutls nettle curl libslirp
  ]
    ++ lib.optionals ncursesSupport [ ncurses ]
    ++ lib.optionals seccompSupport [ libseccomp ]
    ++ lib.optionals numaSupport [ numactl ]
    ++ lib.optionals alsaSupport [ alsa-lib ]
    ++ lib.optionals pulseSupport [ libpulseaudio ]
    ++ lib.optionals pipewireSupport [ pipewire ]
    ++ lib.optionals sdlSupport [ SDL2 SDL2_image ]
    ++ lib.optionals jackSupport [ libjack2 ]
    ++ lib.optionals gtkSupport [ gtk3 gettext vte ]
    ++ lib.optionals vncSupport [ libjpeg libpng ]
    ++ lib.optionals smartcardSupport [ libcacard ]
    ++ lib.optionals spiceSupport [ spice-protocol spice ]
    ++ lib.optionals usbredirSupport [ usbredir ]
    ++ lib.optionals stdenv.isLinux [ libaio libcap_ng libcap attr ]
    ++ lib.optionals xenSupport [ xen ]
    ++ lib.optionals cephSupport [ ceph ]
    ++ lib.optionals glusterfsSupport [ glusterfs libuuid ]
    ++ lib.optionals openGLSupport [ mesa libepoxy libdrm ]
    ++ lib.optionals rutabagaSupport [ rutabaga_gfx ]
    ++ lib.optionals virglSupport [ virglrenderer ]
    ++ lib.optionals libiscsiSupport [ libiscsi ]
    ++ lib.optionals smbdSupport [ samba ]
    ++ lib.optionals uringSupport [ liburing ]
    ++ lib.optionals canokeySupport [ canokey-qemu ]
    ++ lib.optionals capstoneSupport [ capstone ]
    ++ lib.optionals xml2Support [ libxml2 ];

  dontUseMesonConfigure = true; # meson's configurePhase isn't compatible with qemu build

  outputs = [ "out" ] ++ lib.optional guestAgentSupport "ga";
  # On aarch64-linux we would shoot over the Hydra's 2G output limit.
  separateDebugInfo = !(stdenv.isAarch64 && stdenv.isLinux);

  postPatch = ''
    # Otherwise tries to ensure /var/run exists.
    sed -i "/install_emptydir(get_option('localstatedir') \/ 'run')/d" \
        qga/meson.build
  '';

  preConfigure = ''
    unset CPP # intereferes with dependency calculation
    # this script isn't marked as executable b/c it's indirectly used by meson. Needed to patch its shebang
    chmod +x ./scripts/shaderinclude.py
    patchShebangs .
    # avoid conflicts with libc++ include for <version>
    mv VERSION QEMU_VERSION
    substituteInPlace configure \
      --replace '$source_path/VERSION' '$source_path/QEMU_VERSION'
    substituteInPlace meson.build \
      --replace "'VERSION'" "'QEMU_VERSION'"
  '';

  configureFlags = [
    "--disable-strip" # We'll strip ourselves after separating debug info.
    (lib.enableFeature enableDocs "docs")
    "--enable-tools"
    "--localstatedir=/var"
    "--sysconfdir=/etc"
    "--cross-prefix=${stdenv.cc.targetPrefix}"
    (lib.enableFeature guestAgentSupport "guest-agent")
  ] ++ lib.optional numaSupport "--enable-numa"
    ++ lib.optional seccompSupport "--enable-seccomp"
    ++ lib.optional smartcardSupport "--enable-smartcard"
    ++ lib.optional spiceSupport "--enable-spice"
    ++ lib.optional usbredirSupport "--enable-usb-redir"
    ++ lib.optional (hostCpuTargets != null) "--target-list=${lib.concatStringsSep "," hostCpuTargets}"
    ++ lib.optionals stdenv.isDarwin [ "--enable-cocoa" "--enable-hvf" ]
    ++ lib.optional stdenv.isLinux "--enable-linux-aio"
    ++ lib.optional gtkSupport "--enable-gtk"
    ++ lib.optional xenSupport "--enable-xen"
    ++ lib.optional cephSupport "--enable-rbd"
    ++ lib.optional glusterfsSupport "--enable-glusterfs"
    ++ lib.optional openGLSupport "--enable-opengl"
    ++ lib.optional virglSupport "--enable-virglrenderer"
    ++ lib.optional tpmSupport "--enable-tpm"
    ++ lib.optional libiscsiSupport "--enable-libiscsi"
    ++ lib.optional smbdSupport "--smbd=${samba}/bin/smbd"
    ++ lib.optional uringSupport "--enable-linux-io-uring"
    ++ lib.optional canokeySupport "--enable-canokey"
    ++ lib.optional capstoneSupport "--enable-capstone";

  dontWrapGApps = true;

  # QEMU attaches entitlements with codesign and strip removes those,
  # voiding the entitlements and making it non-operational.
  # The alternative is to re-sign with entitlements after stripping:
  # * https://github.com/qemu/qemu/blob/v6.1.0/scripts/entitlement.sh#L25
  dontStrip = stdenv.isDarwin;

  postFixup = ''
    # the .desktop is both invalid and pointless
    rm -f $out/share/applications/qemu.desktop
    wrapProgram $out/bin/qemu-system-x86_64 \
      --add-flags "-bios ${OVMF.fd}/FV/OVMF.fd"
  '' + lib.optionalString guestAgentSupport ''
    # move qemu-ga (guest agent) to separate output
    mkdir -p $ga/bin
    mv $out/bin/qemu-ga $ga/bin/
    ln -s $ga/bin/qemu-ga $out/bin
    remove-references-to -t $out $ga/bin/qemu-ga
  '' + lib.optionalString gtkSupport ''
    # wrap GTK Binaries
    for f in $out/bin/qemu-system-*; do
      wrapGApp $f
    done
  '';
  preBuild = "cd build";

  # Add a ‘qemu-kvm’ wrapper for compatibility/convenience.
  postInstall = lib.optionalString (!toolsOnly) ''
    ln -s $out/bin/qemu-system-${stdenv.hostPlatform.qemuArch} $out/bin/qemu-kvm
    install -m 755 ${srcs.qemu}/escape-run.sh $out/bin/escape-run.sh
  '';

  passthru = {
    qemu-system-i386 = "bin/qemu-system-i386";
    tests = lib.optionalAttrs (!toolsOnly) {
      qemu-tests = finalAttrs.finalPackage.overrideAttrs (_: { doCheck = true; });
      qemu-utils-builds = qemu-utils;
    };
    updateScript = gitUpdater {
      # No nicer place to find latest release.
      url = "https://gitlab.com/qemu-project/qemu.git";
      rev-prefix = "v";
      ignoredVersions = "(alpha|beta|rc).*";
    };
  };

  # Builds in ~3h with 2 cores, and ~20m with a big-parallel builder.
  requiredSystemFeatures = [ "big-parallel" ];

  meta = with lib; {
    homepage = "https://www.qemu.org/";
    description = "A generic and open source machine emulator and virtualizer";
    license = licenses.gpl2Plus;
    maintainers = with maintainers; [ eelco qyliss ];
    platforms = platforms.unix;
  }
  # toolsOnly: Does not have qemu-kvm and there's no main support tool
  // lib.optionalAttrs (!toolsOnly) {
    mainProgram = "qemu-kvm";
  };
})
