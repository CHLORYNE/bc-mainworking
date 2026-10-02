#!/usr/bin/env python3
# NAUTITECH - Simulateur de Navigation
# KYARA FEUX: copy a hand-placed navigation-light block from one boat.ini to another version of
# the SAME 3D model, and tag each lamp with a LightRole
# so the COLREG situations (feux de route, mouillage, NUC, ...) drive it correctly.
#
# Why this exists: a lamp's position is a property of the 3D model, not something that can be
# guessed from a bounding box - a mast is where the modeller put it. But the same model is often
# installed several times (own ship, other ship, a sinking variant) and only one copy has the
# lights. This carries them across instead of placing them by hand again.
#
# Lamp coordinates are in the MODEL's own frame, the same frame as the mesh vertices, so copying
# them verbatim puts each lamp back on the same bolt of the same hull. YCorrection only decides
# how deep that hull floats, and a lamp is bolted to the hull, so it goes down with it: the copy
# does NOT compensate for a different YCorrection. (--match-waterline does that instead, if a
# variant ever needs it - but a settling wreck or a helicopter is exactly where it would be wrong.)
#
#   python3 copy_lights.py --scan "C:/path/to/Models"
#       Report only: which boat.ini files have lights, which don't, and which could be filled in
#       from another copy of the same model. Changes nothing.
#
#   python3 copy_lights.py --scan "C:/path/to/Models" --write
#       Do it. Every file that is changed is backed up next to itself as boat.ini.bak first.
#
#   python3 copy_lights.py --from src/boat.ini --to dst/boat.ini [--write]
#       One specific pair.
#
# The role guess is only a guess: check it, and correct any LightRole= line by hand. Everything
# else in the target file is left untouched - the block is appended at the end.

import argparse, os, re, sys, shutil

LIGHT_KEYS = ["LightX", "LightY", "LightZ", "LightRange", "LightRed", "LightGreen", "LightBlue",
              "LightStartAngle", "LightEndAngle", "Sequence", "PhaseStart"]

def read_ini(path):
    """Bridge Command ini: 'Key=value' lines. A '/' instead of '=' is how these files comment a
    lamp out, so those lines are read but flagged, never copied."""
    values = {}
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#") or s.startswith("//"):
                continue
            m = re.match(r'^([A-Za-z_0-9()]+)\s*=\s*(.*?)\s*$', s)
            if m:
                values[m.group(1)] = m.group(2).strip().strip('"')
    return values

def f(values, key, default=0.0):
    try:
        return float(values.get(key, default))
    except ValueError:
        return default

def model_file(values):
    return values.get("FileName", "").strip().strip('"').lower()

def light_count(values):
    try:
        return int(float(values.get("NumberOfLights", 0)))
    except ValueError:
        return 0

def guess_role(x, y, z, r, g, b, rng, bow_positive_z, zmin, zmax, ymax):
    """Colour first, then position. Deliberately conservative: anything it is not sure about is
    left without a role, and ShipLights then infers one itself."""
    # Red and green are the sidelights, whichever way round the model is built.
    if r > 150 and g < 100 and b < 100:
        return "port" if x < 0 else "starboard"
    if g > 150 and r < 100 and b < 100:
        return "starboard" if x > 0 else "port"
    # Yellow-ish: a working light, not a navigation light.
    if r > 150 and g > 150 and b < 120:
        return "deck"
    # White. Long range = a signal light; short range = usually accommodation or a floodlight.
    if r > 150 and g > 150 and b > 150:
        forward = (z > 0) if bow_positive_z else (z < 0)
        if rng >= 15:
            return "masthead" if forward else "stern"
        if y >= ymax - 0.05 * max(1.0, (ymax - 0)):
            return "masthead"          # highest white lamp on the vessel: the mast top
        return ""                       # let ShipLights decide
    return ""

def build_block(src_values, y_shift, bow_positive_z):
    n = light_count(src_values)
    lamps = []
    zmin, zmax, ymax = 1e9, -1e9, -1e9
    for i in range(1, n + 1):
        red = f(src_values, "LightRed(%d)" % i)
        green = f(src_values, "LightGreen(%d)" % i)
        blue = f(src_values, "LightBlue(%d)" % i)
        if "LightX(%d)" % i not in src_values:
            continue                    # commented out with '/'
        if red == 0 and green == 0 and blue == 0:
            continue                    # black = switched off in these files
        x = f(src_values, "LightX(%d)" % i)
        y = f(src_values, "LightY(%d)" % i) + y_shift
        z = f(src_values, "LightZ(%d)" % i)
        lamps.append(dict(x=x, y=y, z=z, r=red, g=green, b=blue,
                          rng=f(src_values, "LightRange(%d)" % i, 6),
                          a0=f(src_values, "LightStartAngle(%d)" % i, -360),
                          a1=f(src_values, "LightEndAngle(%d)" % i, 360),
                          seq=src_values.get("Sequence(%d)" % i, ""),
                          phase=src_values.get("PhaseStart(%d)" % i, "")))
        zmin, zmax, ymax = min(zmin, z), max(zmax, z), max(ymax, y)

    out = ["", "# ---- KYARA FEUX: lamps copied from another installed copy of this model ----"]
    out += (["# Y shifted by %+.3f model units (--match-waterline)." % y_shift]
            if abs(y_shift) > 0.001 else ["# Positions copied verbatim: same mesh, same fitting."])
    out += ["# LightRole is a guess from colour, height and position - CHECK IT and correct by hand.",
           "NumberOfLights=%d" % len(lamps), ""]
    for idx, L in enumerate(lamps, start=1):
        role = guess_role(L["x"], L["y"], L["z"], L["r"], L["g"], L["b"], L["rng"],
                          bow_positive_z, zmin, zmax, ymax)
        out.append("LightX(%d)=%g" % (idx, L["x"]))
        out.append("LightY(%d)=%g" % (idx, L["y"]))
        out.append("LightZ(%d)=%g" % (idx, L["z"]))
        out.append("LightRange(%d)=%g" % (idx, L["rng"]))
        out.append("LightRed(%d)=%g" % (idx, L["r"]))
        out.append("LightGreen(%d)=%g" % (idx, L["g"]))
        out.append("LightBlue(%d)=%g" % (idx, L["b"]))
        out.append("LightStartAngle(%d)=%g" % (idx, L["a0"]))
        out.append("LightEndAngle(%d)=%g" % (idx, L["a1"]))
        if role:
            out.append("LightRole(%d)=%s" % (idx, role))
        if L["seq"]:
            out.append("Sequence(%d)=%s" % (idx, L["seq"]))
        if L["phase"]:
            out.append("PhaseStart(%d)=%s" % (idx, L["phase"]))
        out.append("")
    return "\n".join(out), len(lamps)

def transfer(src_path, dst_path, write, bow_positive_z, match_waterline=False):
    src, dst = read_ini(src_path), read_ini(dst_path)
    if light_count(src) == 0:
        print("  source has no lights, skipped: %s" % src_path); return False
    if light_count(dst) > 0:
        print("  target already has %d lights, left alone: %s" % (light_count(dst), dst_path)); return False
    if model_file(src) != model_file(dst):
        print("  different models (%s vs %s), skipped" % (model_file(src), model_file(dst))); return False

    # Copied verbatim: the lamp stays on the same point of the same mesh (see the note at the top).
    dy = f(src, "YCorrection") - f(dst, "YCorrection")
    y_shift = dy if match_waterline else 0.0
    note = ""
    if abs(dy) > 0.001:
        note = ("\n      [!] YCorrection %s here vs %s in the source (%+.3f). Deliberate for a "
                "wreck or an aircraft;\n          otherwise the two copies float at different "
                "heights and one of them is wrong." %
                (dst.get("YCorrection", "?"), src.get("YCorrection", "?"), -dy))
    block, n = build_block(src, y_shift, bow_positive_z)
    print("  %d lamps  ->  %s%s" % (n, dst_path, note))
    if not write:
        return True
    shutil.copyfile(dst_path, dst_path + ".bak")
    with open(dst_path, "a", encoding="utf-8") as fh:
        fh.write("\n" + block + "\n")
    return True

def scan(root, write, bow_positive_z, match_waterline=False):
    by_model = {}
    for folder, _, files in os.walk(root):
        for name in files:
            if name.lower() != "boat.ini":
                continue
            p = os.path.join(folder, name)
            v = read_ini(p)
            mf = model_file(v)
            if not mf:
                continue
            by_model.setdefault(mf, []).append((p, light_count(v)))

    print("%d distinct 3D models found under %s\n" % (len(by_model), root))
    donors, orphans, done = 0, [], 0
    for mf, copies in sorted(by_model.items()):
        withL = [p for p, n in copies if n > 0]
        without = [p for p, n in copies if n == 0]
        if withL and without:
            donors += 1
            print("%s: %d copy(ies) with lights, %d without" % (mf, len(withL), len(without)))
            for dst in without:
                if transfer(withL[0], dst, write, bow_positive_z, match_waterline):
                    done += 1
        elif not withL:
            orphans.append((mf, copies[0][0]))

    print("\n%d model(s) filled in%s." % (done, "" if write else " (dry run - add --write to apply)"))
    if orphans:
        print("\n%d model(s) have NO light data anywhere - these need positions measured once:" % len(orphans))
        for mf, p in orphans:
            print("   %-28s %s" % (mf, p))

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--scan")
    ap.add_argument("--from", dest="src")
    ap.add_argument("--to", dest="dst")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--bow-negative-z", action="store_true",
                    help="use if your models are built with the bow towards -Z")
    ap.add_argument("--match-waterline", action="store_true",
                    help="shift Y so lamps keep the same height above the SEA when two copies "
                         "have different YCorrection. Off by default: a lamp belongs to the hull.")
    a = ap.parse_args()
    bow_pos = not a.bow_negative_z
    if a.scan:
        scan(a.scan, a.write, bow_pos, a.match_waterline)
    elif a.src and a.dst:
        transfer(a.src, a.dst, a.write, bow_pos, a.match_waterline)
    else:
        ap.print_help(); sys.exit(1)
