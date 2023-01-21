{ pkgs ? import <nixpkgs> {} }:

# pyocd rtt --target nrf52840 -M attach

with pkgs;

mkShell {
  hardeningDisable = [ "all" ];
  nativeBuildInputs = [
    cmake
    ninja

    openocd
    python3Packages.west

    gn1924
    # mcuboot requirements
    python3Packages.cryptography
    python3Packages.intelhex
    python3Packages.click
    python3Packages.cbor2
    # Used by imgtool to sign:
    (python3.withPackages (p: with p; [
      cffi
      pyocd
    ]))
    libressl
    pkg-config

    # Menu config
    python3Packages.tkinter

    python3Packages.pyelftools

    ccache
    gcc-arm-embedded
  ];

  buildInputs = with pkgs; [

  ];

  ZEPHYR_BASE=builtins.getEnv "HOME" + "/src/ncs/zephyr";
  ZEPHYR_TOOLCHAIN_VARIANT="gnuarmemb";
  GNUARMEMB_TOOLCHAIN_PATH="${pkgs.gcc-arm-embedded}";
}
