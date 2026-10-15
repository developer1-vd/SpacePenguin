{
  description = "SpacePenguin, a secure, lightweight, and usable browser written in Qt";

  # The channel tarball needs no GitHub API calls, so it is not rate limited.
  inputs.nixpkgs.url = "https://channels.nixos.org/nixos-unstable/nixexprs.tar.xz";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      # nix build      ->  ./result/bin/SpacePenguin
      packages = forAllSystems (pkgs: rec {
        spacepenguin = pkgs.callPackage ./package.nix { };
        default = spacepenguin;
      });

      # nix run github:developer1-vd/SpacePenguin
      apps = forAllSystems (pkgs: {
        default = {
          type = "app";
          program = nixpkgs.lib.getExe self.packages.${pkgs.stdenv.hostPlatform.system}.default;
          meta.description = "Run SpacePenguin";
        };
      });

      # nix develop    ->  cmake -B build && cmake --build build
      devShells = forAllSystems (pkgs: {
        default = import ./shell.nix { inherit pkgs; };
      });

      # nixpkgs.overlays = [ spacepenguin.overlays.default ];  ->  pkgs.spacepenguin
      overlays.default = final: _prev: {
        spacepenguin = final.callPackage ./package.nix { };
      };

      # NixOS: imports = [ spacepenguin.nixosModules.default ];
      #        programs.spacepenguin.enable = true;
      nixosModules.default =
        {
          config,
          lib,
          pkgs,
          ...
        }:
        let
          cfg = config.programs.spacepenguin;
        in
        {
          options.programs.spacepenguin = {
            enable = lib.mkEnableOption "SpacePenguin, a lightweight Qt web browser";
            package = lib.mkOption {
              type = lib.types.package;
              default = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
              defaultText = lib.literalExpression "spacepenguin.packages.\${system}.default";
              description = "The SpacePenguin package to install.";
            };
            defaultBrowser = lib.mkOption {
              type = lib.types.bool;
              default = false;
              description = "Make SpacePenguin the default handler for web links and HTML files.";
            };
          };

          config = lib.mkIf cfg.enable {
            environment.systemPackages = [ cfg.package ];
            xdg.mime.defaultApplications = lib.mkIf cfg.defaultBrowser {
              "text/html" = "org.spacepenguin.SpacePenguin.desktop";
              "x-scheme-handler/http" = "org.spacepenguin.SpacePenguin.desktop";
              "x-scheme-handler/https" = "org.spacepenguin.SpacePenguin.desktop";
            };
          };
        };

      # nix flake check builds the package and runs its tests.
      checks = forAllSystems (pkgs: {
        spacepenguin = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
