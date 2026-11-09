<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: EUPL-1.2
-->

# Contributing

Enable the repository's pre-commit license check once per clone:

```sh
git config --local core.hooksPath .githooks
```

The hook checks newly added staged files for the SEANERGYS copyright notice and
`EUPL-1.2` identifier. Files that cannot contain comments need a sibling
`.license` file with both SPDX notices. The hook can be bypassed, so it is a
contributor-side check rather than server-side enforcement.
