# Ship model fixes (boat.ini)

These are corrected `boat.ini` files for the ships in `bin\Models`. The models
themselves are not in this repository because they are too large, so only the
`boat.ini` files are kept here.

## How to install

Double-click `Apply-ModelFixes.bat`. It copies every file from
`ModelFixes\Models\...` into `bin\Models\...`.

- **Backups:** before replacing a `boat.ini`, it saves the old one as
  `boat.ini.bak`.
- **Another Models folder:** drag that folder onto `Apply-ModelFixes.bat`.
- **Undo:** run `powershell -ExecutionPolicy Bypass -File Apply-ModelFixes.ps1 -Restore`.

## What changed and why

Only three keys were changed: `ScaleFactor`, `YCorrection` and `FileName`.
Every other line is untouched.

- **`ScaleFactor`:** many ships were drawn far too big. For example, the BDC-180
  was 724 m long and the cruise ship was 662 m. Views, wheel, radar screen and
  lights are all in model units, so they follow the new size by themselves.
- **`YCorrection`:** this sets how deep the hull sits in the water. With the new
  water, which no longer shows inside the hull, a wrong value made ships look
  sunk.
- **Shared hulls:** ships that use the same 3D model now have the same values.
  That covers all CQPM, ISPM and ITPM SAR boats, and all ALOTF and "Chal-"
  trawlers.
- **`FileName`:** two own ships wrote their model file name in the wrong
  upper/lower case. Windows ignores case, so they already loaded there; the fix
  matters only on Linux and macOS.
- **`Depth=`:** the simulator does not read this key, so it was left as it is.

**How to read the "Before" and "After" columns:**
- **L** is the overall length of the model.
- **T** is the draught: the depth from the waterline to the lowest point of
  the mesh, including keel, rudder and propeller.

| Folder | Ship | Before | After | Change |
|---|---|---|---|---|
| Ownship | ALOTF-1_Net_out | 50 m, T 3.14 | 50 m, T 3.30 | YCorrection -9 → -9.5 |
| Ownship | ALOTF-1_NoNet | 37 m, T 3.14 | 37 m, T 3.30 | YCorrection -9.0 → -9.5 |
| Ownship | ALOTF-2_Net_out | 50 m, T 3.14 | 50 m, T 3.30 | YCorrection -9 → -9.5 |
| Ownship | ALOTF-2_NoNet | 37 m, T 3.14 | 37 m, T 3.30 | YCorrection -9 → -9.5 |
| Ownship | ALOTF-3_Net_out | 50 m, T 3.14 | 50 m, T 3.30 | YCorrection -9 → -9.5 |
| Ownship | ALOTF-3_NoNet | 37 m, T 3.14 | 37 m, T 3.30 | YCorrection -9 → -9.5 |
| Ownship | CQPM DAKHLA SAR-1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | CQPM DAKHLA SAR-2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | CQPM DAKHLA SAR-3 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | CQPM-SAR-1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | CQPM-SAR-2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | CQPM-SIDI-IFNI-SAR1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | CQPM-SIDI-IFNI-SAR2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | CQPM-SIDI-IFNI-SAR3 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | Fishing_Long_Line | unchanged | unchanged |  FileName "fishing.obj" → "Fishing.obj" (case only) |
| Ownship | ISPM SAR-STD-1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ISPM SAR-STD-2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ISPM SAR-STD-3 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ISPM SAR-STD-4 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ISPM SAR-STD-5 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ISPM SAR-STD-6 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ISPM SAR-STD-7 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ITPM-SAFI-SAR1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ITPM-SAFI-SAR2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ITPM-TANTAN-SAR1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | ITPM-TANTAN-SAR2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Ownship | Marine_Royale | 22 m, T 2.07 | 15 m, T 1.39 | ScaleFactor 0.3 → 0.201794 |
| Ownship | Pelagic_boeuf | 36 m, T 0.15 | 36 m, T 5.10 | YCorrection 0.25 → -1.4 |
| Ownship | Pilote | 23 m, T 0.41 | 14 m, T 1.10 | ScaleFactor 1.5 → 0.913043; YCorrection -0.15 → -1.0814 |
| Ownship | SAR Eurocopter | unchanged | unchanged |  FileName "sar.x" → "SAR.x" (case only) |
| Ownship | Zaid_MotorBoat | 19 m, T 0.67 | 10 m, T 0.50 | ScaleFactor 0.20 → 0.104712; YCorrection -1 → -2.425 |
| Othership | ALOTF-1_Net_in | 38 m, T 3.78 | 38 m, T 3.30 | YCorrection -11 → -9.5 |
| Othership | ALOTF-2_Net_in | 38 m, T 3.78 | 38 m, T 3.30 | YCorrection -11 → -9.5 |
| Othership | ALOTF-3_Net_out | 50 m, T 3.78 | 50 m, T 3.30 | YCorrection -11 → -9.5 |
| Othership | ALOTF-4_Net_out | 50 m, T 3.78 | 50 m, T 3.30 | YCorrection -11 → -9.5 |
| Othership | BDC-180 | 724 m, T 28.34 | 100 m, T 3.91 | ScaleFactor 3.5 → 0.483092 |
| Othership | BDC-190 | 653 m, T 27.65 | 70 m, T 2.96 | ScaleFactor 3.5 → 0.375306 |
| Othership | CQPM DAKHLA SAR-1 | 15 m, T 1.23 | 15 m, T 1.23 | ScaleFactor 0.202 → 0.201794 |
| Othership | CQPM DAKHLA SAR-2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | CQPM DAKHLA SAR-3 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | CQPM-SAR-1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | CQPM-SAR-2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | CQPM-SIDI-IFNI-SAR1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | CQPM-SIDI-IFNI-SAR2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | Chal-BULAND_Net_in | 38 m, T 3.78 | 38 m, T 3.30 | YCorrection -11 → -9.5 |
| Othership | Chal-NORDIC_NoNet | 37 m, T 3.78 | 37 m, T 3.30 | YCorrection -11 → -9.5 |
| Othership | Chal-ZANDER_Net_out | 50 m, T 3.78 | 50 m, T 3.30 | YCorrection -11 → -9.5 |
| Othership | Cruise Ship | 662 m, T 31.57 | 294 m, T 8.00 | ScaleFactor 1.3 → 0.577429; YCorrection -24 → -13.5699 |
| Othership | G11 | 18 m, T 1.14 | 18 m, T 1.21 | YCorrection 0.23 → 0.22 |
| Othership | GENDARMERIE_ROYALE | 13 m, T 1.34 | 10 m, T 1.02 | ScaleFactor 0.13 → 0.0992366 |
| Othership | HMAS_Armidale | 91 m, T 6.27 | 57 m, T 2.70 | ScaleFactor 0.3038 → 0.188795; YCorrection -12 → -5.6627 |
| Othership | ISPM SAR-STD-1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ISPM SAR-STD-2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ISPM SAR-STD-3 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ISPM SAR-STD-4 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ISPM SAR-STD-5 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ISPM SAR-STD-6 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ISPM SAR-STD-7 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ITPM-SAFI-SAR1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ITPM-SAFI-SAR2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ITPM-TANTAN-SAR1 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | ITPM-TANTAN-SAR2 | 22 m, T 1.83 | 15 m, T 1.23 | ScaleFactor 0.3 → 0.201794 |
| Othership | LNG AZROU Tanker | 194 m, T 8.73 | 290 m, T 11.50 | ScaleFactor 12 → 17.9396; YCorrection -0.27 → -0.1832 |
| Othership | M11 | 18 m, T 1.14 | 18 m, T 1.21 | YCorrection 0.23 → 0.22 |
| Othership | MOB | 4.5 m tall person | 1.8 m tall person | ScaleFactor 0.030 → 0.00803571 |
| Othership | MR | 281 m, T 8.59 | 50 m, T 2.50 | ScaleFactor 10.0 → 1.77999; YCorrection 0.13 → -0.4155 |
| Othership | Marine_Royale | 22 m, T 2.10 | 15 m, T 1.39 | ScaleFactor 0.3 → 0.201794; YCorrection -1.9 → -1.8 |
| Othership | NOAA | 119 m, T 10.25 | 68 m, T 5.00 | ScaleFactor 0.22 → 0.125503; YCorrection -11 → -4.2487 |
| Othership | Pilote | 23 m, T 0.94 | 14 m, T 1.10 | ScaleFactor 1.5 → 0.913043; YCorrection -0.5 → -1.0781 |
| Othership | SAR Pilote | 31 m, T 2.72 | 20 m, T 1.76 | ScaleFactor 0.55 → 0.354839 |
| Othership | Sinking_Zaid_MotorBoat | 19 m, T 3.10 | 10 m, T 1.62 | ScaleFactor 0.20 → 0.104712 |
| Othership | Zaid_MotorBoat | 19 m, T 0.67 | 10 m, T 0.50 | ScaleFactor 0.20 → 0.104712; YCorrection -1 → -2.425 |
