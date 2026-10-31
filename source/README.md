# Corresponding source tree

This directory preserves the exact v11 C/header/build/test inputs recorded in
`../releases/auto-v11/manifest.json`, plus required EDK2 headers, original helper
scripts and license notices. It is a selected source tree, not a full clone of
the upstream repository. The public build entry is `../tools/build.sh`.

The shared source also reproduces the published v10 USB EFI byte-for-byte when
the automatic branch is disabled. The v10 historical manifest predates five
conditional source/test changes, so its old source hashes are retained as
history rather than claimed to describe the new shared tree. Both rebuild
results are recorded in `../evidence/package-validation`.

The GPU runtime code was not changed during publication. Original local scripts
that depend on owner captures/caches remain for provenance; they are not a
general installer. The public owner installer in `../tools/install_owner.ps1`
adapts paths while retaining the hardware and deployment guards.

Original copyright and license notices are retained. New 2503 source work was
created/modified 2026-10-30 and 2026-10-31; publication tooling and documentation
were added 2026-10-31. See the package root THIRD_PARTY_NOTICES.md for lineage.
