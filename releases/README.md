# Hardware-verified, machine-bound images

| File | Bytes | SHA-256 |
|---|---:|---|
| usb-v10/UNLOCK.EFI | 73216 | cae0fe2d5e965d5fb79d6541de568ad53e04fd794a8f7106a3985254821d3c71 |
| auto-v11/AUTO.EFI | 74240 | a5b5dc610f6423c7c436658c2943593286dbd7d6c623c0e218f369b43166c051 |

Both images have been run successfully on the single machine in the root
README and rebuilt byte-for-byte from the included shared source. They still
require five separately prepared firmware inputs. They are not general 2503
binaries and the automatic image will only accept the pinned internal ESP.

The manifests are original build-time snapshots. `HardwareTested=false` in those
snapshots is historical, not the final test result. Subsequent hardware and
performance evidence is in `../evidence/measurements`; package rebuild evidence
is in `../evidence/package-validation`. Do not edit old manifests to imply that
the original offline build had already performed hardware validation.
