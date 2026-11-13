# Publishing SpacePenguin on Flathub

`io.github.developer1_vd.SpacePenguin.yml` here is the manifest Flathub builds
SpacePenguin from. It is written by hand and builds a tagged release from this
repository on GitHub. Flathub verifies the `io.github.developer1_vd` app ID through
the developer1-vd GitHub account, so no domain is needed.

## Before submitting

1. Publish a release whose tag matches the manifest (`v1.0.0-rc.1` now).
2. Put that tag's commit in the manifest: `git rev-parse v1.0.0-rc.1^{commit}`.
3. Build and lint it as Flathub will (the commands are at the top of the
   manifest). The linter must report nothing.

## Submitting (once)

1. Fork <https://github.com/flathub/flathub> with the option to copy all branches.
2. In the fork, start a branch from `new-pr`, add
   `io.github.developer1_vd.SpacePenguin.yml` at the top level, and push it.
3. Open a pull request from that branch to `flathub/flathub`'s **`new-pr`**
   branch (not `master`), titled `Add io.github.developer1_vd.SpacePenguin`.
4. Comment `bot, build` on the pull request for a test build, and answer the
   reviewers. Once it's accepted, Flathub makes a repository
   `flathub/io.github.developer1_vd.SpacePenguin` and gives developer1-vd access.

## Each release after that

Flathub's bot watches this repository's tags (`x-checker-data` in the manifest)
and opens a pull request with the new tag and commit in
`flathub/io.github.developer1_vd.SpacePenguin`. Merge it to publish the update.
Changes to the manifest itself go in that repository too; keep this copy the same.
