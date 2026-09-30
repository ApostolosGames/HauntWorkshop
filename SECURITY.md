# Security Policy

## Scope

This repository documents how to build and publish Steam Workshop maps for **HAUNT**. Please report security problems with:

- HAUNT's Workshop map loading, for example a crafted `.pak`/`.utoc`/`.ucas` set that crashes the game, runs unexpected code, or reads or writes files outside the Workshop item
- anything in this repository (the guide, the SteamCMD template, or the FBX templates) that could put map creators or players at risk

Ordinary map-loading errors, such as `Could not find a map` or `Expected exactly one .pak`, are not security issues. Use the [Map loading problem](https://github.com/ApostolosGames/HauntWorkshop/issues/new/choose) issue form for those.

## Reporting a vulnerability

**Do not open a public issue for a security problem.**

Report it privately through GitHub:

1. Go to the repository's [**Security** tab](https://github.com/ApostolosGames/HauntWorkshop/security).
2. Select **Report a vulnerability**.
3. Describe the problem, how to reproduce it, and which HAUNT build you tested. If the report involves a Workshop item, include its Workshop ID, but do not attach a malicious pak to a public page.

Only the maintainers can see the report. We aim to acknowledge reports within 7 days and will keep you updated while we investigate. Please give us reasonable time to release a fix before you disclose the problem publicly.

## Supported versions

Only the current public release of HAUNT and the current `main` branch of this repository are supported.
