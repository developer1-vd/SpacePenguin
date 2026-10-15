# For Nix without flakes:
#   nix-build            ->  ./result/bin/SpacePenguin
#   nix-env -f . -i      ->  install into your profile
{
  pkgs ? import <nixpkgs> { },
}:

pkgs.callPackage ./package.nix { }
