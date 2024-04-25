with import <nixpkgs> {};
stdenv.mkDerivation {
  name = "rust-env";
  nativeBuildInputs = [
    rustc
    cargo
    rust-analyzer
    pkg-config
    stdenv.cc.cc.lib
    llvmPackages.clang
    llvmPackages.libclang
    cmake
    rustPlatform.bindgenHook
    pkgs.darwin.apple_sdk_11_0.frameworks.AppKit
  ];
  buildInputs = [
    libiconv
    binutils
    stdenv.cc.cc.lib
    (opencv.override  {
       enableGtk3 = true;
       enableUnfree = true;
    })
    llvmPackages.libclang
  ];
}
