{
  description = "Simple Research Data Pipeline";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    scas = {
      url = "github:markuskowa/scas";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, scas }: let
      forAllSystems = function:
        nixpkgs.lib.genAttrs [
          "x86_64-linux"
          "aarch64-linux"
      ] (system: function nixpkgs.legacyPackages.${system} system);
  in {

    packages = forAllSystems (pkgs: system: {
      srdp = pkgs.lib.callPackageWith (pkgs // { scas = scas.outputs.packages.${system}.default; }) ./package.nix {};
      default = self.outputs.packages.${system}.srdp;
    });

    devShells = forAllSystems (pkgs: system: {
      default = self.outputs.packages.${system}.srdp;
    });
  };
}
