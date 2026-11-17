# winget

The manifests for each release live in a folder named after its version, written
by `tools/update-package-manifests.sh <version>` from the published release.

winget only installs packages from its community repository, so each version is
sent there as a pull request:

1. Fork [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs).
2. Copy the version folder to `manifests/s/SpacePenguin/SpacePenguin/<version>/`.
3. Check it on Windows: `winget validate --manifest <folder>` and
   `winget install --manifest <folder>`.
4. Open the pull request. Microsoft's bots test the installer before it is merged.

Alternatively, [wingetcreate](https://github.com/microsoft/winget-create) does
steps 1–4: `wingetcreate submit <folder>`.

Once merged, anyone can install with:

```powershell
winget install SpacePenguin.SpacePenguin
```
