{
  lib,
  stdenv,
  cmake,
  pkg-config,
  hyprtoolkit,
  libdrm,
  lua5_4,
  version ? "git",
  doCheck ? false,
}:
let
  inherit (lib.strings) optionalString;
in
stdenv.mkDerivation {
  pname = "hyprtoolkit-lua" + optionalString doCheck "-with-tests";
  inherit version doCheck;

  src = lib.cleanSourceWith {
    src = ../.;
    filter =
      path: type:
      let
        name = baseNameOf path;
        isBuildDirectory = type == "directory" && builtins.match "build(-.*)?" name != null;
        isResult = builtins.match "result(-.*)?" name != null;
      in
      !isBuildDirectory && !isResult;
  };

  nativeBuildInputs = [
    cmake
    pkg-config
    lua5_4
  ];

  propagatedBuildInputs = [
    hyprtoolkit
    lua5_4
  ];

  buildInputs = hyprtoolkit.buildInputs ++ [ libdrm ];

  strictDeps = true;

  cmakeBuildType = if doCheck then "Debug" else "RelWithDebInfo";

  meta = {
    homepage = "https://github.com/hyprwm/hyprtoolkit-lua";
    description = "Lua 5.4 bindings for Hyprtoolkit";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
  };
}
