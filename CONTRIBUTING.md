# Contributing

Thanks for helping improve the HAUNT Workshop map guide. This repository holds the guide ([`README.md`](README.md)), the SteamCMD upload template, and the reference FBX templates. It does not contain HAUNT's source code or game content.

## Ways to contribute

- **Report an error in the guide.** If a step, value, or setting doesn't match what HAUNT actually does, open a [Guide correction](https://github.com/ApostolosGames/HauntWorkshop/issues/new/choose) issue.
- **Get help with a map that won't load.** Open a [Map loading problem](https://github.com/ApostolosGames/HauntWorkshop/issues/new/choose) issue and include HAUNT's error message and your pak listing.
- **Suggest an improvement.** Missing topics, clearer explanations, or new reference templates are welcome as a [Request](https://github.com/ApostolosGames/HauntWorkshop/issues/new/choose).
- **Report a security problem privately.** Follow the [security policy](SECURITY.md) instead of opening an issue.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change.
3. Check that any values you add (coordinates, sizes, settings, surface types) match the current HAUNT release. Say in the pull request how you verified them, for example by testing a private Workshop upload.
4. Fill in the pull request template and open the pull request against `main`.

A maintainer will review it. Changes to gameplay values (landmark positions, spawn rules, lighting behavior) need confirmation from the HAUNT team before they are merged.

## Guidelines

- Write for map creators who know Unreal Engine but not HAUNT's internals. Prefer short steps and concrete values.
- Keep the README's existing structure and anchor links working. If you rename a heading, update every link to it.
- Do not commit cooked paks, `.utoc`/`.ucas` files, HAUNT game files, or other large binaries. Reference templates must be small FBX blockouts, not game assets.
- Only contribute content you have the right to share. By contributing, you agree that your contribution is licensed under the repository's [MIT License](LICENSE).

## Code of conduct

Everyone taking part in this project is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
