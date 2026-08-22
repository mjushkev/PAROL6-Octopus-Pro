# Immutable third-party source lock

These references are inputs, not proof that their hardware assumptions apply
to Matt J's robot. Derived work must retain the relevant upstream licenses.

| Component | Immutable revision | Git tree | License SHA-256 | Intended use |
| --- | --- | --- | --- | --- |
| Official PAROL6 checkout | `77597de127a844990965189f0e6062e2551a2842` | Parent repository | Parent repository | Reviewed reference |
| Waldo Commander | `d5acbe1bea86cf1f207b8e912b8e36f9d7dbaf91` | `b822454fc5dd3483016166d75111b9618f1eff06` | `e1b4dc6f479b2401e8d1acf04b7cc41e39b9deb6c9aed6a3633f9ce5a2be2abf` | Phase 6 fork baseline |
| PAROL6 Python API | `829c2c73051c18d9cbf2e4cb07508a1557f63294` | `4903f4208583e237e01cfa276af3d8244b07ce0c` | `230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809` | Phase 5 fork baseline |
| waldoctl | `9ceab01e9b43495f4115cda90d26563220a1466a` | `e906c4485d5020fe00934cda19a0f6c9df10db6d` | `310965db3e5f2201818dc4db412df3623c775fe3ac775fbf47bef720fcdfa215` | Phase 5 client contract baseline |
| MKS Servo42C | `makerbase-mks/MKS-SERVO42C` | `31471153111fc991fb6f4e6cab2690912b2f79a5` | Reference only; installed firmware must be measured |
| BTT Octopus Pro | `bigtreetech/BIGTREETECH-OCTOPUS-Pro` | `60a01f412959b62c349ba00da15b45232b7d90c5` | Schematic/reference baseline; HV-01 required |
| TMCStepper | PlatformIO registry `0.7.3` | `teemuatlut/TMCStepper` | MIT | Pinned build-only dependency for the temporary J6 diagnostic |

The upstream URLs and license filenames are recorded in
`UPSTREAM_MANIFEST.json`. `scripts/fetch-upstreams.ps1` verifies checked-out revisions and refuses a
mismatch. Runtime/build dependencies will be added with hashes before they are
redistributed. No third-party binary is yet part of a release artifact.
