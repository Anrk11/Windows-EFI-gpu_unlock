# Attribution and licensing

This package is a 2026-10-31 captured-machine GA106/2503 research extension. New handoff, core adapter, automatic activation, confirmation, packaging and public tools are provided under GPL-2.0-only. Existing source notices are preserved. This is not an upstream-certified general-purpose 2503 release.

- Direct EFI/Windows port base: https://github.com/impacifist/Windows-GPU-UNLOCK at `1f8776ee9c6ee32f5920adaa4baabc9181379bd4`. Included `source/internal-boot` and related code retain original notices. The original upstream third-party notice and changelog are preserved under `source/`.
- Core lineage: https://github.com/Zagitta/gpu-unlock at `0ebdb1c6308b6a2072be5bf04513472942541956` (upstream port notice), GPL-2.0-only. See `source/internal-boot/unlock/vendor`.
- Earlier port lineage: https://github.com/rough2515-hub/rtx3080-unlock-public at `a08254d269af30ace9e57237abd3d8e6c261e8f5`.
- EDK2 header subset: https://github.com/tianocore/edk2 at `c1415771ac592ad8509351c46cd3cda4f38d7d52`; BSD-2-Clause-Patent. See `source/deps/edk2/License.txt` and `LICENSES/EDK2-BSD-2-Clause-Patent.txt`.
- Firmware preparation scripts and provenance: https://github.com/JustAResearcher/gpu-unlock-ada at `4e7fefcc4923f9e083d5324dc75a7d2f906af77f`, GPL-2.0-only. `firmware/upstream` preserves the selected files byte-for-byte. The original firmware notice records additional payload attribution, including WildFlash1st/cmp90hx-unlock `76940ae583281daa2b883518a2e89b56c11d3a46` and TOTORONG/AD103_win_unlocker `51df9c17ec89eb8486df52cc9079dde4bd1f9974`.

Source extensions relative to the direct base include `source/research/2503-handoff`, the GU_RESEARCH_2503 profile branches in `machine-profile.h`, and the RAM transaction test hook. Unrelated local changes to the boot probe were not exported. Public tools and documentation are new packaging work; GPU runtime sources were not rewritten for publication.

NVIDIA firmware is not included. Downloaded archives, extracted firmware and generated payloads retain their applicable terms and notices; GPL licensing of host code does not relicense them. The pinned preparation tool preserves NVIDIA-LICENSE in the local output. LICENSES also retains the original NVIDIA-MIT, CMP40HX-MIT and NVIDIA Driver notices. No claim of blanket firmware redistribution permission is made.
