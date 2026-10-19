#!/usr/bin/env python3
"""
build_harbour.py - generate 3D harbour structures for a Bridge Command world.

The world generator bakes breakwaters and quays into the terrain heightmap, but at
a few metres per cell they come out as smooth blobs. This tool finds those
structures in the heightmap and rebuilds them as proper 3D land objects:

  * rubble-mound breakwaters (rock armour, concrete crest road, crown wall,
    round head) swept along each breakwater's centre line
  * vertical quay walls along the basin, with concrete apron, fenders, bollards
  * beacon towers on the breakwater heads

It writes, inside the world folder:
  * Models/LandObject/<prefix>_breakwaters/ and <prefix>_quays/  (OBJ + MTL + textures)
  * <heightmap>_harbour.f32  - heightmap with structures lowered under the meshes
                               and the port platform levelled to quay height
  * terrain_harbour.jpg      - desert/port terrain texture
  * landobject.ini           - entries placing the new objects
and points terrain.ini at the new heightmap and texture. Source files are not
modified, so the tool can be re-run after changing the config.

Usage:
    python3 tools/harbour_builder/build_harbour.py bin/World/BOUJDOUR/harbour.json [--debug]

--debug also writes harbour_detection.png in the world folder, showing what was detected
(grey land, yellow breakwaters, blue basin, light grey levelled platform, red quay lines).

Coordinates follow the simulator: X east, Y up, Z north, metres, with the
simulator's own lat/long scaling (see Terrain::longToX / latToZ).
"""

import json
import math
import os
import re
import sys

import numpy as np
import shapely
from PIL import Image
from scipy import ndimage as ndi
from skimage.measure import approximate_polygon, find_contours
from skimage.draw import line as draw_line
from skimage.morphology import disk, skeletonize

EARTH_RAD_M = 6.371e6  # Must match Constants.hpp


# ----------------------------------------------------------------------------
# World / terrain helpers
# ----------------------------------------------------------------------------

def read_ini(path):
    values = {}
    with open(path, encoding="latin-1") as f:
        for line in f:
            if "=" in line:
                k, v = line.split("=", 1)
                values[k.strip()] = v.strip().strip('"')
    return values


def set_ini_value(path, key, value):
    with open(path, encoding="latin-1") as f:
        text = f.read()
    pattern = r"(?m)^%s=.*$" % re.escape(key)
    if re.search(pattern, text):
        text = re.sub(pattern, lambda m: "%s=%s" % (key, value), text)
    else:
        text = text.rstrip("\n") + "\n%s=%s\n" % (key, value)
    with open(path, "w", encoding="latin-1") as f:
        f.write(text)


class TerrainGrid:
    """Heightmap held north-up (row 0 = north edge), with lat/long <-> metre conversions."""

    def __init__(self, world_dir, heightmap_name):
        ini = read_ini(os.path.join(world_dir, "terrain.ini"))
        self.lon0 = float(ini["TerrainLong(1)"])
        self.lat0 = float(ini["TerrainLat(1)"])
        self.dlon = float(ini["TerrainLongExtent(1)"])
        self.dlat = float(ini["TerrainLatExtent(1)"])
        self.rows = int(ini["TerrainHeightMapRows(1)"])
        self.cols = int(ini["TerrainHeightMapColumns(1)"])
        raw = np.fromfile(os.path.join(world_dir, heightmap_name), dtype="<f4")
        # File order is south row first, west to east (see Terrain::heightMapBinaryToVector)
        self.h = np.flipud(raw.reshape(self.rows, self.cols)).astype(np.float64)
        # Metres per degree, as the simulator computes them
        self.kx = math.radians(1.0) * EARTH_RAD_M * math.cos(math.radians(self.lat0 + self.dlat / 2.0))
        self.kz = math.radians(1.0) * EARTH_RAD_M
        self.px_e = self.dlon / (self.cols - 1) * self.kx
        self.px_n = self.dlat / (self.rows - 1) * self.kz

    def lon_of_col(self, c):
        return self.lon0 + np.asarray(c) * self.dlon / (self.cols - 1)

    def lat_of_row(self, r):
        return self.lat0 + (self.rows - 1 - np.asarray(r)) * self.dlat / (self.rows - 1)

    def col_of_lon(self, lon):
        return (lon - self.lon0) * (self.cols - 1) / self.dlon

    def row_of_lat(self, lat):
        return (self.rows - 1) - (lat - self.lat0) * (self.rows - 1) / self.dlat

    def save(self, path, h):
        np.flipud(h).astype("<f4").tofile(path)


class LocalFrame:
    """Metric frame (east, north) centred on an anchor lat/long."""

    def __init__(self, grid, lat_a, lon_a):
        self.g, self.lat_a, self.lon_a = grid, lat_a, lon_a

    def from_pixel(self, r, c):
        e = (self.g.lon_of_col(c) - self.lon_a) * self.g.kx
        n = (self.g.lat_of_row(r) - self.lat_a) * self.g.kz
        return e, n

    def to_pixel(self, e, n):
        c = self.g.col_of_lon(self.lon_a + np.asarray(e) / self.g.kx)
        r = self.g.row_of_lat(self.lat_a + np.asarray(n) / self.g.kz)
        return r, c

    def from_latlon(self, lat, lon):
        return (lon - self.lon_a) * self.g.kx, (lat - self.lat_a) * self.g.kz


# ----------------------------------------------------------------------------
# Mesh + OBJ output
# ----------------------------------------------------------------------------

class Mesh:
    """Triangle soup grouped by material, with flat per-face normals."""

    def __init__(self):
        self.groups = {}

    @staticmethod
    def to_obj(q):
        # OBJ is right-handed: x = -east, y = up, z = north.
        # Irrlicht's OBJ loader negates x again and reverses the winding to match.
        return np.array([-q[0], q[1], q[2]], dtype=float)

    def tri(self, mat, pts, uvs, outward):
        """pts: 3 (east, up, north) points. outward: (east, up, north) direction the face should face."""
        p = [self.to_obj(q) for q in pts]
        n = np.cross(p[1] - p[0], p[2] - p[0])  # counter-clockwise front face, as OBJ expects
        ln = np.linalg.norm(n)
        if ln < 1e-9:
            return
        n /= ln
        if np.dot(n, self.to_obj(outward)) < 0:
            p = [p[0], p[2], p[1]]
            uvs = [uvs[0], uvs[2], uvs[1]]
            n = -n
        self.groups.setdefault(mat, []).append((p, uvs, n))

    def quad(self, mat, pts, uvs, outward):
        self.tri(mat, [pts[0], pts[1], pts[2]], [uvs[0], uvs[1], uvs[2]], outward)
        self.tri(mat, [pts[0], pts[2], pts[3]], [uvs[0], uvs[2], uvs[3]], outward)

    def triangle_count(self):
        return sum(len(v) for v in self.groups.values())

    def write_obj(self, folder, name, materials):
        os.makedirs(folder, exist_ok=True)
        with open(os.path.join(folder, name + ".mtl"), "w") as m:
            for mat in sorted(self.groups):
                spec = materials[mat]
                m.write("newmtl %s\n" % mat)
                m.write("Ka %.3f %.3f %.3f\n" % tuple(spec["kd"]))
                m.write("Kd %.3f %.3f %.3f\n" % tuple(spec["kd"]))
                m.write("Ks 0.000 0.000 0.000\nillum 1\n")
                if spec.get("texture"):
                    m.write("map_Kd %s\n" % spec["texture"])
                m.write("\n")
        with open(os.path.join(folder, name + ".obj"), "w") as o:
            o.write("# Generated by tools/harbour_builder/build_harbour.py\n")
            o.write("mtllib %s.mtl\n" % name)
            vi, fi = 1, 1
            for mat in sorted(self.groups):
                o.write("usemtl %s\n" % mat)
                for p, uvs, n in self.groups[mat]:
                    for q in p:
                        o.write("v %.3f %.3f %.3f\n" % tuple(q))
                    for t in uvs:
                        o.write("vt %.4f %.4f\n" % (t[0], t[1]))
                    o.write("vn %.4f %.4f %.4f\n" % tuple(n))
                    o.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (vi, vi, fi, vi + 1, vi + 1, fi, vi + 2, vi + 2, fi))
                    vi += 3
                    fi += 1
        with open(os.path.join(folder, "object.ini"), "w") as f:
            f.write('FileName="%s.obj"\nScalefactor=1\n' % name)


# ----------------------------------------------------------------------------
# Procedural textures
# ----------------------------------------------------------------------------

def value_noise(shape, scales, rng, weights=None):
    """Sum of upsampled random grids, normalised to 0..1."""
    out = np.zeros(shape)
    weights = weights or [1.0] * len(scales)
    for s, w in zip(scales, weights):
        gh, gw = max(2, shape[0] // s + 2), max(2, shape[1] // s + 2)
        grid = rng.random((gh, gw))
        up = ndi.zoom(grid, (shape[0] / (gh - 1), shape[1] / (gw - 1)), order=3)[:shape[0], :shape[1]]
        out += w * up
    out -= out.min()
    return out / max(out.max(), 1e-9)


def periodic_noise(size, max_freq, rng, terms=40):
    """Smooth noise that tiles exactly: a random sum of sinusoids with integer frequencies."""
    yy, xx = np.mgrid[0:size, 0:size] * (2 * math.pi / size)
    out = np.zeros((size, size))
    for _ in range(terms):
        fy, fx = rng.integers(-max_freq, max_freq + 1, 2)
        if fy == 0 and fx == 0:
            continue
        out += np.cos(fy * yy + fx * xx + rng.random() * 2 * math.pi) / math.hypot(fy, fx)
    out -= out.min()
    return out / max(out.max(), 1e-9)


def tileable(img):
    """Blend an image with its half-offset copy so it tiles without hard seams."""
    h, w = img.shape[:2]
    rolled = np.roll(np.roll(img, h // 2, 0), w // 2, 1)
    y = np.abs(np.linspace(-1, 1, h))[:, None]
    x = np.abs(np.linspace(-1, 1, w))[None, :]
    wgt = np.clip(np.maximum(x, y) * 1.6 - 0.6, 0, 1)
    if img.ndim == 3:
        wgt = wgt[..., None]
    return img * (1 - wgt) + rolled * wgt


def save_rgb(path, rgb, quality=90):
    Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).save(path, quality=quality)


def make_rock_texture(path, rng, size=512, stones=22):
    """Armour stones: tileable Voronoi cells, each shaded as a lit boulder with dark gaps between."""
    from scipy.spatial import cKDTree
    seeds = rng.random((stones, 2)) * size
    tiled = np.vstack([seeds + [dy, dx] for dy in (-size, 0, size) for dx in (-size, 0, size)])
    tree = cKDTree(tiled)
    yy, xx = np.mgrid[0:size, 0:size]
    # Warp the lookup so the stone outlines are irregular rather than straight-edged
    warp = 14.0
    wy = (periodic_noise(size, 10, rng) - 0.5) * 2 * warp
    wx = (periodic_noise(size, 10, rng) - 0.5) * 2 * warp
    pix = np.stack([(yy + wy).ravel(), (xx + wx).ravel()], axis=1)
    d, idx = tree.query(pix, k=2)
    gap = (d[:, 1] - d[:, 0]).reshape(size, size)
    cell = (idx[:, 0] % stones).reshape(size, size)
    centre = tiled[idx[:, 0]]
    off = (pix - centre).reshape(size, size, 2)
    radius = np.sqrt(size * size / stones) / 2
    # Rounded stones: brighter towards the centre and the lit (upper) side, darker into the gaps
    rounded = np.clip(gap / (radius * 0.9), 0, 1) ** 0.5
    light = 1.0 - 0.35 * np.clip(off[..., 0] / radius, -1, 1) - 0.1 * np.clip(off[..., 1] / radius, -1, 1)
    tone = (0.7 + 0.45 * rng.random(stones))[cell]
    grain = 0.6 * periodic_noise(size, 24, rng, 80) + 0.4 * tileable(rng.random((size, size)))
    shade = tone * (0.55 + 0.45 * rounded) * (0.8 + 0.2 * light) * (0.82 + 0.36 * grain)
    void = np.clip((gap - 3.0) / 9.0, 0, 1)  # dark voids between stones
    base = np.array([142, 134, 120.0])
    rgb = base[None, None, :] * shade[..., None] * (0.12 + 0.88 * void[..., None])
    save_rgb(path, rgb)


def make_concrete_texture(path, rng, size=512, joints=True):
    n1 = value_noise((size, size), [64, 8, 2], rng, [0.6, 0.3, 0.4])
    rgb = np.array([176, 174, 166.0])[None, None, :] * (0.86 + 0.18 * n1[..., None])
    if joints:
        line = np.zeros(size)
        line[:3] = 1
        rgb -= 35 * np.maximum(line[:, None], line[None, :])[..., None]
    save_rgb(path, tileable(rgb))


def make_quaywall_texture(path, rng, seabed, quay, size=512):
    """Non-tiling vertically: v=0 at seabed, v=1 at quay level, so the tide line sits at y=0."""
    n1 = value_noise((size, size), [64, 8, 2], rng, [0.6, 0.3, 0.4])
    rgb = np.array([160, 158, 150.0])[None, None, :] * (0.85 + 0.2 * n1[..., None])
    # Image row 0 is the top of the wall (v=1) because Irrlicht flips V on load.
    y_of_row = quay - (np.arange(size) + 0.5) / size * (quay - seabed)
    algae = np.clip((0.6 - y_of_row) / 1.2, 0, 1)[:, None, None]
    rgb = rgb * (1 - algae) + np.array([52, 62, 44.0]) * (0.8 + 0.3 * n1[..., None]) * algae
    stain = np.clip((1.6 - y_of_row) / 1.0, 0, 1)[:, None, None] * (1 - algae) * 0.35
    rgb = rgb * (1 - stain) + np.array([95, 92, 80.0]) * stain
    coping = (y_of_row > quay - 0.35)[:, None, None]
    rgb = np.where(coping, rgb * 1.08, rgb)
    # vertical block joints every 1/4 of the texture width
    cols = (np.arange(size) % (size // 4)) < 2
    rgb[:, cols, :] *= 0.8
    save_rgb(path, rgb)


# ----------------------------------------------------------------------------
# Feature extraction
# ----------------------------------------------------------------------------

def longest_skeleton_path(skel):
    """Return the pixels of the longest path through a skeleton, in order."""
    pts = np.argwhere(skel)
    index = {tuple(p): i for i, p in enumerate(pts)}
    nbrs = [[] for _ in pts]
    for i, (r, c) in enumerate(pts):
        for dr in (-1, 0, 1):
            for dc in (-1, 0, 1):
                j = index.get((r + dr, c + dc))
                if j is not None and j != i:
                    nbrs[i].append((j, math.hypot(dr, dc)))

    def farthest(start):
        import heapq
        dist = {start: 0.0}
        prev = {start: None}
        heap = [(0.0, start)]
        while heap:
            d, u = heapq.heappop(heap)
            if d > dist[u]:
                continue
            for v, w in nbrs[u]:
                nd = d + w
                if nd < dist.get(v, 1e18):
                    dist[v], prev[v] = nd, u
                    heapq.heappush(heap, (nd, v))
        end = max(dist, key=dist.get)
        return end, prev

    a, _ = farthest(0)
    b, prev = farthest(a)
    path = []
    while b is not None:
        path.append(pts[b])
        b = prev[b]
    return np.array(path, dtype=float)


def smooth_resample(pts, step, smooth_passes=3):
    """Smooth a 2D polyline and resample it at a fixed arc-length spacing."""
    p = pts.copy()
    for _ in range(smooth_passes):
        if len(p) > 4:
            q = p.copy()
            q[1:-1] = (p[:-2] + 2 * p[1:-1] + p[2:]) / 4.0
            p = q
    seg = np.linalg.norm(np.diff(p, axis=0), axis=1)
    s = np.concatenate([[0], np.cumsum(seg)])
    n = max(2, int(s[-1] / step) + 1)
    si = np.linspace(0, s[-1], n)
    out = np.stack([np.interp(si, s, p[:, 0]), np.interp(si, s, p[:, 1])], axis=1)
    for _ in range(smooth_passes):
        q = out.copy()
        q[1:-1] = (out[:-2] + 2 * out[1:-1] + out[2:]) / 4.0
        out = q
    return out


def tangents_normals(line):
    t = np.gradient(line, axis=0)
    t /= np.linalg.norm(t, axis=1, keepdims=True)
    left = np.stack([-t[:, 1], t[:, 0]], axis=1)  # (e,n): rotate +90 deg
    return t, left


# ----------------------------------------------------------------------------
# Geometry builders
# ----------------------------------------------------------------------------

def breakwater_profile(lv, wall_scale):
    """Cross-section from lee toe to seaward toe.

    Returns (points, materials): points [(s, y)] with s>0 seaward, and one material per segment.
    """
    crest, bed = lv["crest"], lv["seabed"]
    wall = crest + (lv["crown_wall"] - crest) * wall_scale
    road_lee, road_sea, wall_w = -lv["crest_half_width"], lv["crest_half_width"] - 2.0, 2.0
    rows = 9
    pts, mats = [], []
    # Lee armour, toe up to the crest road
    s_toe = road_lee - (crest - bed) * lv["lee_slope"]
    for k in range(rows):
        f = k / rows
        pts.append((s_toe + (road_lee - s_toe) * f, bed + (crest - bed) * f))
        mats.append("rock")
    # Crest road, crown wall lee face, wall top
    pts += [(road_lee, crest), (road_sea, crest), (road_sea, wall), (road_sea + wall_w, wall)]
    mats += ["concrete", "concrete", "concrete", "concrete"]
    # Seaward armour, from just under the wall top down to the toe
    top = wall - 0.6
    s0 = road_sea + wall_w
    s_toe2 = s0 + (top - bed) * lv["sea_slope"]
    for k in range(rows + 1):
        f = k / rows
        pts.append((s0 + (s_toe2 - s0) * f, top + (bed - top) * f))
        mats.append("rock")
    mats.pop()  # one fewer segment than points
    return pts, mats


class Rand:
    """Deterministic per-vertex jitter keyed by integer coordinates."""

    def __init__(self, seed):
        self.seed = seed

    def __call__(self, *key):
        h = hash((self.seed,) + key) & 0xFFFFFFFF
        return ((h * 2654435761) & 0xFFFFFFFF) / 0xFFFFFFFF * 2.0 - 1.0


def build_breakwater(mesh, line, seaward, lv, rnd, comp_id):
    """Sweep the profile along a centre line. line: (N,2) east/north; seaward: (N,2) unit normals."""
    n = len(line)
    seg = np.linalg.norm(np.diff(line, axis=0), axis=1)
    arc = np.concatenate([[0], np.cumsum(seg)])
    total = arc[-1]
    ramp = lv["wall_ramp_m"]
    sections, profiles = [], []
    for i in range(n):
        # Crown wall fades out over the last metres before the head
        wall_scale = float(np.clip((total - arc[i]) / ramp, 0, 1))
        prof, mats = breakwater_profile(lv, wall_scale)
        pts = []
        for j, (s, y) in enumerate(prof):
            e, nn = line[i] + seaward[i] * s
            # Jitter armour vertices (not the toes or the concrete edges) to look like rock
            rock_vertex = 0 < j < len(prof) - 1 and mats[j - 1] == "rock" and mats[min(j, len(mats) - 1)] == "rock"
            if rock_vertex:
                amp = lv["rock_jitter"]
                e += rnd(comp_id, i, j, 0) * amp
                nn += rnd(comp_id, i, j, 1) * amp
                y += rnd(comp_id, i, j, 2) * amp
            pts.append(np.array([e, y, nn]))
        sections.append(pts)
        profiles.append((prof, mats))
    for i in range(n - 1):
        a, b = sections[i], sections[i + 1]
        prof, mats = profiles[i]
        for j, mat in enumerate(mats):
            (s0, y0), (s1, y1) = prof[j], prof[j + 1]
            # Outward is left of the direction of travel along the profile (s right, y up)
            ns, ny = -(y1 - y0), (s1 - s0)
            out = np.array([ns * seaward[i][0], ny, ns * seaward[i][1]])
            # Rock texture tile is ~12 m x 10 m (armour stones ~2.5 m); concrete ~6 m x 4 m
            tu, tv = (12.0, 10.0) if mat == "rock" else (6.0, 4.0)
            u0, u1 = arc[i] / tu, arc[i + 1] / tu
            if s0 == s1:
                v0, v1 = y0 / tv, y1 / tv  # vertical face
            else:
                v0, v1 = s0 / tv, s1 / tv
            mesh.quad(mat, [a[j], b[j], b[j + 1], a[j + 1]], [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], out)


def build_roundhead(mesh, centre, lv, rnd, comp_id):
    """Conical rock armour with a concrete platform, centred on the breakwater head."""
    seg = 48
    rows = 10
    plat_r, plat_y = lv["head_platform_radius"], lv["crest"] + 0.5
    bed = lv["seabed"]
    toe_r = plat_r + (plat_y - bed) * lv["sea_slope"]
    ring = []
    for k in range(rows + 1):
        f = k / rows
        r = plat_r + (toe_r - plat_r) * f
        y = plat_y + (bed - plat_y) * f
        pts = []
        for a in range(seg):
            ang = 2 * math.pi * a / seg
            jit = lv["rock_jitter"] if 0 < k < rows else 0.0
            rr = r + rnd(comp_id, 900, k, a) * jit
            yy = y + rnd(comp_id, 901, k, a) * jit
            pts.append(np.array([centre[0] + rr * math.cos(ang), yy, centre[1] + rr * math.sin(ang)]))
        ring.append(pts)
    for k in range(rows):
        for a in range(seg):
            b = (a + 1) % seg
            p = [ring[k][a], ring[k][b], ring[k + 1][b], ring[k + 1][a]]
            ang = 2 * math.pi * (a + 0.5) / seg
            out = np.array([math.cos(ang), 1.0, math.sin(ang)])
            circ = 2 * math.pi * (plat_r + toe_r) / 2
            u0, u1 = a / seg * circ / 12.0, (a + 1) / seg * circ / 12.0
            v0, v1 = k / rows * (toe_r - plat_r) / 10.0, (k + 1) / rows * (toe_r - plat_r) / 10.0
            mesh.quad("rock", p, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], out)
    c = np.array([centre[0], plat_y, centre[1]])
    for a in range(seg):
        b = (a + 1) % seg
        pa, pb = ring[0][a], ring[0][b]
        mesh.tri("concrete", [c, pa, pb],
                 [(0.5, 0.5), (0.5 + (pa[0] - c[0]) / 6, 0.5 + (pa[2] - c[2]) / 6),
                  (0.5 + (pb[0] - c[0]) / 6, 0.5 + (pb[2] - c[2]) / 6)], np.array([0, 1.0, 0]))


def cylinder(mesh, mat, centre_e, centre_n, y0, y1, r, seg=12, cap=True):
    for a in range(seg):
        t0, t1 = 2 * math.pi * a / seg, 2 * math.pi * (a + 1) / seg
        p0 = np.array([centre_e + r * math.cos(t0), y0, centre_n + r * math.sin(t0)])
        p1 = np.array([centre_e + r * math.cos(t1), y0, centre_n + r * math.sin(t1)])
        out = np.array([math.cos((t0 + t1) / 2), 0, math.sin((t0 + t1) / 2)])
        mesh.quad(mat, [p0, p1, p1 + [0, y1 - y0, 0], p0 + [0, y1 - y0, 0]],
                  [(a / seg, 0), ((a + 1) / seg, 0), ((a + 1) / seg, 1), (a / seg, 1)], out)
        if cap:
            c = np.array([centre_e, y1, centre_n])
            mesh.tri(mat, [c, p0 + [0, y1 - y0, 0], p1 + [0, y1 - y0, 0]], [(0.5, 0.5)] * 3, np.array([0, 1.0, 0]))


def box(mesh, mat, centre, along, out_dir, half_along, half_out, y0, y1):
    """Axis box oriented by 'along' (unit e/n) with thickness along out_dir."""
    a = np.array([along[0], 0, along[1]])
    o = np.array([out_dir[0], 0, out_dir[1]])
    c = np.array([centre[0], 0, centre[1]])
    corners = {}
    for sa in (-1, 1):
        for so in (-1, 1):
            for sy, y in ((0, y0), (1, y1)):
                corners[(sa, so, sy)] = c + sa * half_along * a + so * half_out * o + np.array([0, y, 0])
    faces = [
        ((-1, 1, 0), (1, 1, 0), (1, 1, 1), (-1, 1, 1), o),
        ((1, -1, 0), (-1, -1, 0), (-1, -1, 1), (1, -1, 1), -o),
        ((1, 1, 0), (1, -1, 0), (1, -1, 1), (1, 1, 1), a),
        ((-1, -1, 0), (-1, 1, 0), (-1, 1, 1), (-1, -1, 1), -a),
        ((-1, -1, 1), (-1, 1, 1), (1, 1, 1), (1, -1, 1), np.array([0, 1.0, 0])),
    ]
    for f in faces:
        mesh.quad(mat, [corners[k] for k in f[:4]], [(0, 0), (1, 0), (1, 1), (0, 1)], f[4])


def build_beacon(mesh, centre, base_y, colour):
    e, n = centre
    cylinder(mesh, "white", e, n, base_y, base_y + 0.8, 1.6, seg=16)
    cylinder(mesh, colour, e, n, base_y + 0.8, base_y + 4.5, 1.2, seg=16, cap=False)
    cylinder(mesh, "white", e, n, base_y + 4.5, base_y + 5.5, 1.2, seg=16, cap=False)
    cylinder(mesh, colour, e, n, base_y + 5.5, base_y + 8.0, 1.2, seg=16)
    cylinder(mesh, "iron", e, n, base_y + 8.0, base_y + 8.2, 1.5, seg=16)
    cylinder(mesh, "white", e, n, base_y + 8.2, base_y + 9.2, 0.6, seg=12)
    cylinder(mesh, colour, e, n, base_y + 9.2, base_y + 9.7, 0.75, seg=12)


def miter_normals(pts, side_sign):
    """Per-vertex offset directions (scaled for constant offset) for a 2D polyline."""
    d = np.diff(pts, axis=0)
    d /= np.linalg.norm(d, axis=1, keepdims=True)
    seg_n = side_sign * np.stack([-d[:, 1], d[:, 0]], axis=1)
    out = np.zeros_like(pts)
    out[0], out[-1] = seg_n[0], seg_n[-1]
    for i in range(1, len(pts) - 1):
        m = seg_n[i - 1] + seg_n[i]
        m /= max(np.linalg.norm(m), 1e-9)
        cosh = max(np.dot(m, seg_n[i]), 0.35)
        out[i] = m / cosh
    return out


def build_quay(mesh, pts, land_dir, lv, cfg):
    """Vertical wall along pts (east/north), deck extending landward, fenders and bollards."""
    q, bed = lv["quay"], lv["seabed"]
    deck_w = cfg["quay_deck_width"]
    seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    arc = np.concatenate([[0], np.cumsum(seg)])
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        sea = -(land_dir[i] + land_dir[i + 1]) / 2
        out = np.array([sea[0], 0, sea[1]])
        u0, u1 = arc[i] / 8.0, arc[i + 1] / 8.0
        mesh.quad("quaywall",
                  [np.array([a[0], bed, a[1]]), np.array([b[0], bed, b[1]]),
                   np.array([b[0], q, b[1]]), np.array([a[0], q, a[1]])],
                  [(u0, 0), (u1, 0), (u1, 1), (u0, 1)], out)
        a2, b2 = a + land_dir[i] * deck_w, b + land_dir[i + 1] * deck_w
        mesh.quad("concrete",
                  [np.array([a[0], q, a[1]]), np.array([b[0], q, b[1]]),
                   np.array([b2[0], q, b2[1]]), np.array([a2[0], q, a2[1]])],
                  [(a[0] / 6, a[1] / 6), (b[0] / 6, b[1] / 6), (b2[0] / 6, b2[1] / 6), (a2[0] / 6, a2[1] / 6)],
                  np.array([0, 1.0, 0]))
        # Deck back edge, down into the levelled terrain
        mesh.quad("concrete",
                  [np.array([a2[0], q, a2[1]]), np.array([b2[0], q, b2[1]]),
                   np.array([b2[0], q - 1.0, b2[1]]), np.array([a2[0], q - 1.0, a2[1]])],
                  [(0, 0), (1, 0), (1, 0.2), (0, 0.2)], np.array([land_dir[i][0], 0, land_dir[i][1]]))
    # Fenders and bollards along the run, skipping the corners
    for spacing, kind in ((cfg["fender_spacing"], "fender"), (cfg["bollard_spacing"], "bollard")):
        s = spacing / 2
        while s < arc[-1] - 2:
            i = int(np.searchsorted(arc, s) - 1)
            i = min(max(i, 0), len(pts) - 2)
            f = (s - arc[i]) / max(seg[i], 1e-9)
            if 2.0 < s - arc[i] < seg[i] - 2.0:
                p = pts[i] + (pts[i + 1] - pts[i]) * f
                along = (pts[i + 1] - pts[i]) / seg[i]
                land = (land_dir[i] + land_dir[i + 1]) / 2
                land /= np.linalg.norm(land)
                if kind == "fender":
                    box(mesh, "rubber", p - land * 0.2, along, -land, 0.6, 0.2, -0.5, q - 0.4)
                else:
                    c = p + land * 1.0
                    cylinder(mesh, "iron", c[0], c[1], q, q + 0.55, 0.25, seg=10, cap=False)
                    cylinder(mesh, "iron", c[0], c[1], q + 0.55, q + 0.7, 0.36, seg=10)
            s += spacing


# ----------------------------------------------------------------------------
# Terrain texture
# ----------------------------------------------------------------------------

def make_terrain_texture(path, h, platform, size, rng):
    zoom = size / h.shape[0]
    hz = ndi.zoom(h, zoom, order=1)[:size, :size]
    pz = ndi.zoom(platform.astype(float), zoom, order=1)[:size, :size]
    gy, gx = np.gradient(ndi.gaussian_filter(hz, 6))
    slope = np.hypot(gx, gy) * (size / 3500.0) ** -1  # rough metres-per-pixel correction
    slope = np.clip(slope / 0.6, 0, 1)
    big = value_noise((size, size), [512, 128], rng, [1.0, 0.5])
    small = value_noise((size, size), [32, 8, 2], rng, [0.5, 0.35, 0.3])

    sand = np.array([206, 181, 139.0])
    sand_dark = np.array([176, 148, 108.0])
    rock = np.array([138, 116, 90.0])
    wet = np.array([160, 140, 108.0])
    seabed = np.array([150, 140, 110.0])
    deep = np.array([70, 80, 78.0])
    concrete = np.array([172, 170, 162.0])

    t = big[..., None]
    land = sand * (1 - t) + sand_dark * t
    land = land * (1 - slope[..., None]) + rock * slope[..., None]
    land *= (0.9 + 0.2 * small[..., None])
    shore = np.clip(1 - hz / 1.5, 0, 1)[..., None]
    land = land * (1 - shore) + wet * shore
    depth = np.clip(-hz / 12.0, 0, 1)[..., None]
    water = seabed * (1 - depth) + deep * depth
    water *= (0.9 + 0.2 * small[..., None])
    rgb = np.where((hz > 0)[..., None], land, water)
    conc = concrete * (0.88 + 0.16 * small[..., None])
    pz = np.clip(pz, 0, 1)[..., None]
    rgb = rgb * (1 - pz) + conc * pz
    save_rgb(path, rgb, quality=88)


# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

def main(config_path, debug=False):
    with open(config_path) as f:
        cfg = json.load(f)
    world = os.path.dirname(os.path.abspath(config_path))
    prefix = cfg["prefix"]
    lv = cfg["levels"]
    rng = np.random.default_rng(cfg.get("seed", 1))
    rnd = Rand(cfg.get("seed", 1))

    grid = TerrainGrid(world, cfg["source_heightmap"])
    win = cfg["window"]
    r0 = int(math.floor(grid.row_of_lat(win["lat_max"])))
    r1 = int(math.ceil(grid.row_of_lat(win["lat_min"])))
    c0 = int(math.floor(grid.col_of_lon(win["lon_min"])))
    c1 = int(math.ceil(grid.col_of_lon(win["lon_max"])))
    lat_a = float(grid.lat_of_row((r0 + r1) / 2))
    lon_a = float(grid.lon_of_col((c0 + c1) / 2))
    frame = LocalFrame(grid, lat_a, lon_a)
    px = (grid.px_n + grid.px_e) / 2
    print("Port window rows %d-%d cols %d-%d, %.2f m/px, anchor %.6f %.6f" % (r0, r1, c0, c1, px, lat_a, lon_a))

    w = grid.h[r0:r1, c0:c1]
    land = w > 0
    # Mainland: what survives an opening wider than any breakwater
    opened = ndi.binary_opening(land, disk(max(1, int(round(cfg["structure_max_width_m"] / px)))))
    lab, nlab = ndi.label(opened)
    sizes = ndi.sum(opened, lab, range(1, nlab + 1))
    mainland = lab == (1 + int(np.argmax(sizes)))
    mainland = ndi.binary_dilation(mainland, iterations=1) & land
    structures = land & ~mainland
    slab, ns = ndi.label(structures)
    ssizes = ndi.sum(structures, slab, range(1, ns + 1))
    min_px = cfg["structure_min_area_m2"] / (grid.px_e * grid.px_n)
    comp_ids = [i + 1 for i in range(ns) if ssizes[i] >= min_px]
    print("Breakwater components:", [(c, int(ssizes[c - 1])) for c in comp_ids])
    structure_mask = np.isin(slab, comp_ids)

    dist_to_main = ndi.distance_transform_edt(~mainland)

    # Centre lines, oriented root (touching the mainland) -> head
    paths = []
    for cid in comp_ids:
        path = longest_skeleton_path(skeletonize(slab == cid))
        if len(path) < 4:
            continue
        d0 = dist_to_main[int(path[0][0]), int(path[0][1])]
        d1 = dist_to_main[int(path[-1][0]), int(path[-1][1])]
        paths.append(path[::-1] if d0 > d1 else path)

    # Basin: seal the entrance (head to head, or head to the nearest shore), then fill enclosed water
    seal = np.zeros_like(land)
    head_px = [p[-1].astype(int) for p in paths]
    if len(head_px) >= 2:
        pairs = [(a, b) for i, a in enumerate(head_px) for b in head_px[i + 1:]]
    elif head_px:
        shore = np.argwhere(mainland)
        a = head_px[0]
        pairs = [(a, shore[np.argmin(np.hypot(*(shore - a).T))])]
    else:
        pairs = []
    for a, b in pairs:
        rr, cc = draw_line(int(a[0]), int(a[1]), int(b[0]), int(b[1]))
        seal[rr, cc] = True
    seal = ndi.binary_dilation(seal, iterations=1)
    basin = ndi.binary_fill_holes(land | seal) & ~land & ~seal
    if not basin.any():
        # Fallback: close the land with a large disk to seal the entrance
        pad = 40
        closed = ndi.binary_closing(np.pad(land, pad, mode="edge"),
                                    disk(int(round(cfg["basin_closing_m"] / px))))[pad:-pad, pad:-pad]
        basin = ndi.binary_fill_holes(closed) & ~land

    def in_mask(mask, e, n):
        r, c = frame.to_pixel(e, n)
        r, c = int(round(float(r))) - r0, int(round(float(c))) - c0
        return 0 <= r < mask.shape[0] and 0 <= c < mask.shape[1] and bool(mask[r, c])

    breakwaters = Mesh()
    heads = []
    for k, path in enumerate(paths):
        en = np.array([frame.from_pixel(r + r0, c + c0) for r, c in path])
        line = smooth_resample(en, cfg["sweep_step_m"])
        t, left = tangents_normals(line)
        # Extend root into the land so it is buried
        ext = cfg["root_extension_m"]
        root = [line[0] - t[0] * ext * f for f in np.linspace(1, 0, int(ext / cfg["sweep_step_m"]) + 1)[:-1]]
        line = np.vstack([np.array(root), line]) if root else line
        t, left = tangents_normals(line)
        # Seaward side: the side whose offset point is not in the basin
        votes = 0
        probe = cfg["side_probe_m"]
        for p, l in zip(line, left):
            votes += (1 if in_mask(basin, *(p - l * probe)) else 0) - (1 if in_mask(basin, *(p + l * probe)) else 0)
        seaward = left if votes >= 0 else -left
        build_breakwater(breakwaters, line, seaward, lv, rnd, k)
        build_roundhead(breakwaters, line[-1], lv, rnd, k)
        heads.append(line[-1])
        print("  breakwater %d: %.0f m long, head at %.6f %.6f" % (
            k, np.sum(np.linalg.norm(np.diff(line, axis=0), axis=1)),
            lat_a + line[-1][1] / grid.kz, lon_a + line[-1][0] / grid.kx))

    for b in cfg.get("beacons", []):
        target = np.array(frame.from_latlon(b["lat"], b["lon"]))
        head = min(heads, key=lambda hpt: np.linalg.norm(hpt - target))
        build_beacon(breakwaters, head, lv["crest"] + 0.5, "beacon_" + b["colour"])

    # Quay lines: the 0 m contour where basin meets mainland
    quays = Mesh()
    quay_runs = []
    for contour in find_contours(w, 0.0):
        flags = []
        for r, c in contour:
            ri, ci = int(round(r)), int(round(c))
            nb_land = mainland[max(ri - 1, 0):ri + 2, max(ci - 1, 0):ci + 2]
            nb_water = basin[max(ri - 1, 0):ri + 2, max(ci - 1, 0):ci + 2]
            nb_struct = structure_mask[max(ri - 2, 0):ri + 3, max(ci - 2, 0):ci + 3]
            flags.append(nb_land.any() and nb_water.any() and not nb_struct.any())
        flags = np.array(flags)
        start = None
        for i in range(len(flags) + 1):
            if i < len(flags) and flags[i]:
                start = i if start is None else start
            elif start is not None:
                run = contour[start:i]
                start = None
                if len(run) * px < cfg["quay_min_length_m"]:
                    continue
                run = approximate_polygon(run, tolerance=cfg["quay_simplify_px"])
                en = np.array([frame.from_pixel(r + r0, c + c0) for r, c in run])
                # Which side is land?
                mid = len(en) // 2
                d = en[min(mid + 1, len(en) - 1)] - en[max(mid - 1, 0)]
                d /= np.linalg.norm(d)
                lft = np.array([-d[1], d[0]])
                pm = (en[min(mid + 1, len(en) - 1)] + en[max(mid - 1, 0)]) / 2
                sign = 1.0 if in_mask(land, *(pm + lft * 6.0)) else -1.0
                land_dir = miter_normals(en, sign)
                build_quay(quays, en, land_dir, lv, cfg)
                quay_runs.append((en, land_dir))
                print("  quay run: %d corners, %.0f m" % (len(en), np.sum(np.linalg.norm(np.diff(en, axis=0), axis=1))))

    # Write models
    materials = {
        "rock": {"kd": (1, 1, 1), "texture": "rock.jpg"},
        "concrete": {"kd": (1, 1, 1), "texture": "concrete.jpg"},
        "quaywall": {"kd": (1, 1, 1), "texture": "quaywall.jpg"},
        "rubber": {"kd": (0.08, 0.08, 0.08)},
        "iron": {"kd": (0.18, 0.18, 0.2)},
        "white": {"kd": (0.92, 0.92, 0.9)},
        "beacon_red": {"kd": (0.78, 0.1, 0.08)},
        "beacon_green": {"kd": (0.05, 0.55, 0.2)},
    }
    models = os.path.join(world, "Models", "LandObject")
    objects = []
    for name, mesh in (("%s_breakwaters" % prefix, breakwaters), ("%s_quays" % prefix, quays)):
        if not mesh.groups:
            continue
        folder = os.path.join(models, name)
        mesh.write_obj(folder, name, materials)
        for mat in mesh.groups:
            if materials[mat].get("texture"):
                p = os.path.join(folder, materials[mat]["texture"])
                tex_rng = np.random.default_rng(cfg.get("seed", 1) + len(mat))
                if mat == "rock":
                    make_rock_texture(p, tex_rng)
                elif mat == "concrete":
                    make_concrete_texture(p, tex_rng)
                else:
                    make_quaywall_texture(p, tex_rng, lv["seabed"], lv["quay"])
        objects.append(name)
        print("Wrote %s: %d triangles" % (name, mesh.triangle_count()))

    # Terrain: lower the baked-in breakwaters under the meshes, level the port platform
    h = grid.h.copy()
    win_h = h[r0:r1, c0:c1]
    under = ndi.binary_dilation(structure_mask, iterations=1) & ~mainland
    win_h[under] = np.minimum(win_h[under], cfg["structure_terrain_level"])
    platform = np.zeros_like(land)
    quay_line = np.zeros_like(land)
    if quay_runs:
        rr, cc = np.mgrid[r0:r1, c0:c1]
        pe, pn = frame.from_pixel(rr.ravel(), cc.ravel())
        points = shapely.points(pe, pn)
        lines = shapely.MultiLineString([en for en, _ in quay_runs])
        dq = shapely.distance(points, lines).reshape(land.shape)
        decks = shapely.union_all([shapely.Polygon(np.vstack([en, (en + d * cfg["quay_deck_width"])[::-1]]))
                                   for en, d in quay_runs]).buffer(0)
        under_deck = shapely.contains(decks, points).reshape(land.shape)
        flat, blend = cfg["platform_flat_m"], cfg["platform_blend_m"]
        sel = mainland & (dq < flat + blend)
        wgt = np.clip((flat + blend - dq) / blend, 0, 1)
        target = lv["quay"] - 0.3
        win_h[sel] = win_h[sel] * (1 - wgt[sel]) + target * wgt[sel]
        # Terrain must not show in front of the wall or through the deck: drop it to the seabed
        # there. The slope back up to the platform then happens under the deck.
        keepout = (under_deck | (dq < cfg["quay_keepout_m"])) & (win_h > lv["seabed"])
        win_h[keepout] = lv["seabed"]
        platform = mainland & (dq < flat)
        quay_line = dq < px * 0.75
    h[r0:r1, c0:c1] = win_h
    hm_name = os.path.splitext(cfg["source_heightmap"])[0] + "_harbour.f32"
    grid.save(os.path.join(world, hm_name), h)

    full_platform = np.zeros_like(h, dtype=bool)
    full_platform[r0:r1, c0:c1] = platform
    tex_name = "terrain_harbour.jpg"
    make_terrain_texture(os.path.join(world, tex_name), h, full_platform, cfg["terrain_texture_size"], rng)
    terrain_ini = os.path.join(world, "terrain.ini")
    set_ini_value(terrain_ini, "HeightMap(1)", hm_name)
    set_ini_value(terrain_ini, "Texture(1)", tex_name)
    print("Updated terrain.ini -> %s, %s" % (hm_name, tex_name))

    # landobject.ini: generated objects first, then any extra entries from the config
    entries = []
    for name in objects:
        entries.append({"Type": name, "Lat": lat_a, "Long": lon_a, "HeightCorrection": 0, "Rotation": 0,
                        "Absolute": 1, "Collision": 1, "Radar": 1})
    entries.extend(cfg.get("extra_land_objects", []))
    with open(os.path.join(world, "landobject.ini"), "w") as f:
        f.write("Number=%d\n" % len(entries))
        for i, e in enumerate(entries, 1):
            f.write("\n")
            for k in ("Type", "Lat", "Long", "HeightCorrection", "Rotation", "Absolute", "Collision", "Radar"):
                if k in e:
                    v = e[k]
                    if k == "Type":
                        f.write('Type(%d)="%s"\n' % (i, v))
                    elif isinstance(v, float):
                        f.write("%s(%d)=%.7f\n" % (k, i, v))
                    else:
                        f.write("%s(%d)=%s\n" % (k, i, v))
    print("Wrote landobject.ini with %d objects" % len(entries))

    if not debug:
        return
    # Debug overview of what was detected
    dbg = np.zeros(w.shape + (3,), np.uint8)
    dbg[land] = (110, 110, 110)
    dbg[structure_mask] = (230, 180, 40)
    dbg[basin] = (40, 110, 220)
    dbg[platform] = (200, 200, 200)
    dbg[quay_line] = (255, 40, 40)
    Image.fromarray(dbg).resize((w.shape[1] * 3, w.shape[0] * 3), Image.NEAREST).save(
        os.path.join(world, "harbour_detection.png"))


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--debug"]
    if len(args) != 1:
        print(__doc__)
        sys.exit(1)
    main(args[0], debug="--debug" in sys.argv[1:])
