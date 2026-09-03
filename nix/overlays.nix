{
  lib,
  inputs,
  self,
}:
let
  mkDate =
    longDate:
    (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);

  version = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in
{
  default = self.overlays.hyprtoolkit-lua;

  hyprtoolkit-lua-with-deps = lib.composeManyExtensions [
    inputs.aquamarine.overlays.default
    inputs.hyprgraphics.overlays.default
    inputs.hyprlang.overlays.default
    inputs.hyprutils.overlays.default
    inputs.hyprwayland-scanner.overlays.default
    inputs.hyprtoolkit.overlays.default
    self.overlays.hyprtoolkit-lua
  ];

  hyprtoolkit-lua = final: prev: {
    hyprtoolkit-lua = prev.callPackage ./default.nix {
      stdenv = prev.gcc16Stdenv;
      version =
        version
        + "+date="
        + (mkDate (inputs.self.lastModifiedDate or "19700101"))
        + "_"
        + (inputs.self.shortRev or "dirty");
    };

    hyprtoolkit-lua-with-tests = final.hyprtoolkit-lua.override {
      doCheck = true;
    };
  };
}
