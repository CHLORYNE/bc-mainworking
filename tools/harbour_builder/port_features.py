"""
Port platform, roads, perimeter wall, lamp posts and buildings for build_harbour.py.

All geometry is in the local metric frame used by build_harbour.py: 2D points are (east, north),
3D points are (east, up, north). Meshes are build_harbour.Mesh objects.
"""

import math

import numpy as np
import shapely
from PIL import Image, ImageDraw
from shapely.geometry import LineString, MultiLineString, Point, Polygon
from shapely.ops import nearest_points

UP = np.array([0.0, 1.0, 0.0])


def _largest(geom):
    """Largest polygon of a (Multi)Polygon / collection, without holes."""
    polys = [g for g in getattr(geom, "geoms", [geom]) if isinstance(g, Polygon) and not g.is_empty]
    if not polys:
        return None
    return Polygon(max(polys, key=lambda p: p.area).exterior)


def mask_to_polygon(mask, frame, r0, c0):
    """Vectorise a boolean raster (window coordinates) into a metric polygon (largest part)."""
    from skimage.measure import find_contours
    padded = np.pad(mask.astype(float), 1)
    polys = []
    for contour in find_contours(padded, 0.5):
        if len(contour) < 4:
            continue
        e, n = frame.from_pixel(contour[:, 0] - 1 + r0, contour[:, 1] - 1 + c0)
        p = Polygon(np.stack([e, n], axis=1)).buffer(0)
        if not p.is_empty:
            polys.append(p)
    if not polys:
        return None
    return _largest(shapely.union_all(polys))


def quay_strip(en, land_dir, d0, d1):
    """Polygon between offsets d0 and d1 landward of a quay line."""
    return Polygon(np.vstack([en + land_dir * d0, (en + land_dir * d1)[::-1]])).buffer(0)


def platform_polygon(quay_runs, land_core, cfg):
    """A tidy port platform: mitred hull around the quays, clipped to the land, plus the quay aprons."""
    lines = MultiLineString([en for en, _ in quay_runs])
    hull = lines.convex_hull.buffer(cfg["platform_margin_m"], join_style="mitre", mitre_limit=3.0)
    aprons = shapely.union_all([quay_strip(en, d, 0.0, cfg["quay_deck_width"]) for en, d in quay_runs])
    plat = hull.intersection(land_core).union(aprons)
    # Close slivers between the apron strips and the clipped hull, then tidy the outline
    plat = plat.buffer(3.0, join_style="mitre").buffer(-3.0, join_style="mitre")
    plat = _largest(plat.union(aprons))
    return shapely.simplify(plat, cfg["platform_simplify_m"], preserve_topology=True)


def roads(platform, quay_runs, gate, cfg):
    """Service lane behind the quay aprons plus an access road from the gate."""
    lane_in, lane_out = cfg["quay_deck_width"] + 2.0, cfg["quay_deck_width"] + 2.0 + cfg["road_width_m"]
    lanes = shapely.union_all([quay_strip(en, d, lane_in, lane_out) for en, d in quay_runs])
    inner = platform.buffer(-2.0)
    lanes = lanes.intersection(inner)
    target = nearest_points(lanes, Point(gate))[0]
    access = LineString([gate, (target.x, target.y)]).buffer(cfg["road_width_m"] / 2, cap_style="flat")
    return shapely.union_all([lanes, access.intersection(platform)]).buffer(0)


def choose_gate(platform, quay_lines, toward):
    """Point on the land side of the platform outline closest to 'toward' (e.g. the town)."""
    ring = LineString(platform.exterior.coords)
    best, best_d = None, 1e18
    for f in np.linspace(0, 1, 400, endpoint=False):
        p = ring.interpolate(f, normalized=True)
        if p.distance(quay_lines) < 30.0:
            continue
        d = p.distance(Point(toward))
        if d < best_d:
            best, best_d = (p.x, p.y), d
    return best


# ----------------------------------------------------------------------------
# Mesh helpers
# ----------------------------------------------------------------------------

def v3(p2, y):
    return np.array([p2[0], y, p2[1]], dtype=float)


def flat_polygon(mesh, mat, poly, y, tex_m):
    """Triangulate a polygon (with holes) as a flat upward surface."""
    if poly.is_empty:
        return
    tris = shapely.constrained_delaunay_triangles(poly)
    for t in getattr(tris, "geoms", []):
        c = list(t.exterior.coords)[:3]
        mesh.tri(mat, [v3(p, y) for p in c], [(p[0] / tex_m, p[1] / tex_m) for p in c], UP)


def vertical_quad(mesh, mat, a, b, y0, y1, outward, u0=0.0, u1=1.0, v0=0.0, v1=1.0):
    out = np.array([outward[0], 0.0, outward[1]])
    mesh.quad(mat, [v3(a, y0), v3(b, y0), v3(b, y1), v3(a, y1)], [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], out)


def ring_edges(poly):
    """Exterior edges with outward normals (counter-clockwise ring, so outward is to the right)."""
    ring = shapely.geometry.polygon.orient(poly, 1.0).exterior.coords
    pts = np.array(ring)
    for a, b in zip(pts[:-1], pts[1:]):
        d = b - a
        ln = np.linalg.norm(d)
        if ln < 1e-6:
            continue
        yield a, b, np.array([d[1], -d[0]]) / ln, ln


def build_platform(mesh, platform, road_poly, quay_lines, lv, cfg):
    q = lv["quay"]
    flat_polygon(mesh, "concrete", platform.difference(road_poly), q, 6.0)
    flat_polygon(mesh, "asphalt", platform.intersection(road_poly), q, 6.0)
    # Skirt round the land-side edges so no gap shows above the terrain
    for a, b, out, ln in ring_edges(platform):
        mid = Point((a + b) / 2)
        if mid.distance(quay_lines) < 1.0:
            continue  # quay wall already closes this edge
        vertical_quad(mesh, "concrete", a, b, q - 3.0, q, out, 0, ln / 6.0, 0, 0.5)


def build_fence(mesh, platform, quay_lines, gate, cfg, lv):
    """Perimeter wall along the land side of the platform, with a gate opening."""
    q = lv["quay"]
    hgt, thick, gap = cfg["fence_height_m"], 0.3, cfg["gate_width_m"] / 2
    inset = Polygon(platform.buffer(-1.0, join_style="mitre").exterior)
    for a, b, out, ln in ring_edges(inset):
        seg = LineString([a, b])
        if seg.distance(quay_lines) < 8.0:
            continue
        # Cut the gate out of the edge
        pieces = [seg.difference(Point(gate).buffer(gap))] if seg.distance(Point(gate)) < gap else [seg]
        for piece in pieces:
            for part in getattr(piece, "geoms", [piece]):
                if part.length < 0.5:
                    continue
                pa, pb = np.array(part.coords[0]), np.array(part.coords[-1])
                o = out * thick / 2
                vertical_quad(mesh, "plaster", pa + o, pb + o, q, q + hgt, out, 0, part.length / 4, 0, 1)
                vertical_quad(mesh, "plaster", pb - o, pa - o, q, q + hgt, -out, 0, part.length / 4, 0, 1)
                mesh.quad("plaster", [v3(pa + o, q + hgt), v3(pb + o, q + hgt), v3(pb - o, q + hgt),
                                      v3(pa - o, q + hgt)], [(0, 0), (1, 0), (1, 1), (0, 1)], UP)
    # Gate posts
    if gate is not None:
        ring = LineString(inset.exterior.coords)
        g = ring.interpolate(ring.project(Point(gate)))
        t0 = ring.interpolate(max(ring.project(g) - 1.0, 0))
        t1 = ring.interpolate(ring.project(g) + 1.0)
        along = np.array([t1.x - t0.x, t1.y - t0.y])
        along /= max(np.linalg.norm(along), 1e-9)
        for s in (-gap - 0.4, gap + 0.4):
            c = np.array([g.x, g.y]) + along * s
            box_mesh(mesh, "plaster", c, along, 0.4, 0.4, q, q + hgt + 0.8)


def box_mesh(mesh, mat, centre, along, half_a, half_o, y0, y1, uv_scale=None, top_mat=None):
    """Box with vertical sides (no bottom). uv_scale: metres per texture tile for the sides."""
    a = np.array([along[0], along[1]])
    o = np.array([-along[1], along[0]])
    c = np.asarray(centre, dtype=float)
    corners = [c - a * half_a - o * half_o, c + a * half_a - o * half_o,
               c + a * half_a + o * half_o, c - a * half_a + o * half_o]
    for i in range(4):
        p, r = corners[i], corners[(i + 1) % 4]
        d = r - p
        ln = np.linalg.norm(d)
        out = np.array([d[1], -d[0]]) / ln
        u1 = ln / uv_scale if uv_scale else 1.0
        vertical_quad(mesh, mat, p, r, y0, y1, out, 0, u1, 0, 1)
    mesh.quad(top_mat or mat, [v3(p, y1) for p in corners], [(0, 0), (1, 0), (1, 1), (0, 1)], UP)


def cylinder_mesh(mesh, mat, centre, y0, y1, r, seg=16, cap_mat=None):
    for k in range(seg):
        t0, t1 = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
        p0 = np.array([centre[0] + r * math.cos(t0), centre[1] + r * math.sin(t0)])
        p1 = np.array([centre[0] + r * math.cos(t1), centre[1] + r * math.sin(t1)])
        mid = (t0 + t1) / 2
        vertical_quad(mesh, mat, p0, p1, y0, y1, (math.cos(mid), math.sin(mid)), k / seg, (k + 1) / seg, 0, 1)
        if cap_mat:
            mesh.tri(cap_mat, [v3(centre, y1), v3(p0, y1), v3(p1, y1)], [(0.5, 0.5)] * 3, UP)


def build_lamps(mesh, quay_runs, lv, cfg):
    q = lv["quay"]
    for en, land_dir in quay_runs:
        seg = np.linalg.norm(np.diff(en, axis=0), axis=1)
        for i in range(len(en) - 1):
            if seg[i] < 20:
                continue
            along = (en[i + 1] - en[i]) / seg[i]
            n = max(1, int(seg[i] // cfg["lamp_spacing_m"]))
            for k in range(n):
                f = (k + 0.5) / n
                p = en[i] + (en[i + 1] - en[i]) * f + (land_dir[i] + land_dir[i + 1]) / 2 * cfg["lamp_offset_m"]
                cylinder_mesh(mesh, "galvanised", p, q, q + 11.5, 0.14, seg=8)
                head = p - along * 0.0 + (-(land_dir[i] + land_dir[i + 1]) / 2) * 0.9
                box_mesh(mesh, "galvanised", (p + head) / 2, along, 0.08, 0.5, q + 11.3, q + 11.45)
                box_mesh(mesh, "lamp", head, along, 0.35, 0.25, q + 11.0, q + 11.45)


# ----------------------------------------------------------------------------
# Buildings
# ----------------------------------------------------------------------------

def build_building(mesh, b, kind, base_y):
    """b: dict with centre (e,n), along (unit e,n), L, D. kind: building type from the config."""
    c, a = np.asarray(b["centre"]), np.asarray(b["along"])
    o = np.array([-a[1], a[0]])
    L, D, H = b["L"], b["D"], kind["wall_height"]
    name = kind["name"]
    roof = kind.get("roof", "flat")
    parapet = kind.get("parapet", 0.8) if roof == "flat" else 0.0
    top = base_y + H + parapet
    corners = [c - a * L / 2 - o * D / 2, c + a * L / 2 - o * D / 2, c + a * L / 2 + o * D / 2, c - a * L / 2 + o * D / 2]
    bay = kind.get("bay_m", 6.0)
    for i in range(4):
        p, r = corners[i], corners[(i + 1) % 4]
        d = r - p
        ln = np.linalg.norm(d)
        out = np.array([d[1], -d[0]]) / ln
        mat = name + ("_facade" if i % 2 == 0 else "_end")
        # Whole number of bays so the facade pattern ends cleanly at the corners
        u1 = max(1, round(ln / bay))
        vertical_quad(mesh, mat, p, r, base_y, top, out, 0, u1, 0, 1)
    if roof == "flat":
        # Roof slab inside the parapet, inner parapet faces and parapet top
        inner = [cc - (cc - c) / np.linalg.norm(cc - c) * 0.3 * math.sqrt(2) for cc in corners]
        mesh.quad("roof_flat", [v3(p, base_y + H) for p in inner],
                  [(p[0] / 6, p[1] / 6) for p in inner], UP)
        for i in range(4):
            p, r = inner[i], inner[(i + 1) % 4]
            d = r - p
            ln = np.linalg.norm(d)
            inward = -np.array([d[1], -d[0]]) / ln
            vertical_quad(mesh, name + "_plain", p, r, base_y + H, top, inward)
            mesh.quad(name + "_plain", [v3(corners[i], top), v3(corners[(i + 1) % 4], top), v3(r, top), v3(p, top)],
                      [(0, 0), (1, 0), (1, 1), (0, 1)], UP)
        if kind.get("tower"):
            t = kind["tower"]
            tc = c + a * (L / 2 - t["size"] / 2 - 1.0)
            box_mesh(mesh, name + "_plain", tc, a, t["size"] / 2, t["size"] / 2, base_y + H, base_y + H + t["height"] - 2.2,
                     top_mat=name + "_plain")
            box_mesh(mesh, "glass", tc, a, t["size"] / 2, t["size"] / 2, base_y + H + t["height"] - 2.2,
                     base_y + H + t["height"] - 0.4, top_mat="glass")
            box_mesh(mesh, "roof_flat", tc, a, t["size"] / 2 + 0.4, t["size"] / 2 + 0.4,
                     base_y + H + t["height"] - 0.4, base_y + H + t["height"])
    else:
        rise, over = kind.get("roof_rise", 3.0), 0.6
        eave = base_y + H
        ridge_a, ridge_b = c - a * (L / 2 + over), c + a * (L / 2 + over)
        roof_mat = "roof_" + kind.get("roof_colour", "grey")
        for side in (-1, 1):
            ea = c - a * (L / 2 + over) + o * side * (D / 2 + over)
            eb = c + a * (L / 2 + over) + o * side * (D / 2 + over)
            slope_len = math.hypot(D / 2 + over, rise)
            mesh.quad(roof_mat, [v3(ea, eave - over * rise / (D / 2)), v3(eb, eave - over * rise / (D / 2)),
                                 v3(ridge_b, eave + rise), v3(ridge_a, eave + rise)],
                      [(0, 0), ((L + 2 * over) / 4, 0), ((L + 2 * over) / 4, slope_len / 4), (0, slope_len / 4)],
                      np.array([o[0] * side, D / 2 / rise, o[1] * side]))
        # Gable triangles on the two end walls
        for end in (-1, 1):
            e0 = c + a * end * L / 2 - o * D / 2
            e1 = c + a * end * L / 2 + o * D / 2
            apex = c + a * end * L / 2
            mesh.tri(name + "_plain", [v3(e0, eave), v3(e1, eave), v3(apex, eave + rise)],
                     [(0, 0), (1, 0), (0.5, 1)], np.array([a[0] * end, 0, a[1] * end]))


def build_tanks(mesh, b, kind, base_y):
    c, a = np.asarray(b["centre"]), np.asarray(b["along"])
    r, h, n = kind["radius"], kind["wall_height"], kind.get("count", 2)
    for k in range(n):
        p = c + a * (k - (n - 1) / 2) * (2 * r + 3)
        cylinder_mesh(mesh, "tank", p, base_y, base_y + h, r, seg=24)
        cylinder_mesh(mesh, "tank", p, base_y + h, base_y + h + 0.6, r * 0.85, seg=24, cap_mat="tank")
        # Low containment bund
    bund = Polygon([c - a * (n * (r + 1.5) + 1) - np.array([-a[1], a[0]]) * (r + 2),
                    c + a * (n * (r + 1.5) + 1) - np.array([-a[1], a[0]]) * (r + 2),
                    c + a * (n * (r + 1.5) + 1) + np.array([-a[1], a[0]]) * (r + 2),
                    c - a * (n * (r + 1.5) + 1) + np.array([-a[1], a[0]]) * (r + 2)])
    for pa, pb, out, ln in ring_edges(bund):
        vertical_quad(mesh, "plaster", pa, pb, base_y, base_y + 0.9, out, 0, ln / 4, 0, 0.25)


def footprint(centre, along, L, D):
    a = np.asarray(along)
    o = np.array([-a[1], a[0]])
    c = np.asarray(centre)
    return Polygon([c - a * L / 2 - o * D / 2, c + a * L / 2 - o * D / 2, c + a * L / 2 + o * D / 2, c - a * L / 2 + o * D / 2])


def building_zone(platform, quay_runs, road_poly, cfg):
    lines = MultiLineString([en for en, _ in quay_runs])
    return platform.buffer(-cfg["building_edge_clearance_m"]).difference(
        lines.buffer(cfg["building_setback_m"])).difference(road_poly.buffer(2.0))


def place_buildings(kinds, platform, quay_runs, road_poly, cfg):
    """Greedy placement behind the quay aprons, aligned with the quay they face."""
    zone = building_zone(platform, quay_runs, road_poly, cfg)
    placed, taken = [], Polygon()
    # Candidate slots along every quay segment, scanning outward from each segment's middle
    slots = []
    for en, land_dir in quay_runs:
        seg = np.linalg.norm(np.diff(en, axis=0), axis=1)
        for i in range(len(en) - 1):
            if seg[i] < 15:
                continue
            along = (en[i + 1] - en[i]) / seg[i]
            land = (land_dir[i] + land_dir[i + 1]) / 2
            land /= np.linalg.norm(land)
            offs = np.arange(0, seg[i] / 2, 3.0)
            for s in sorted(np.concatenate([offs, -offs[1:]]), key=abs):
                slots.append((seg[i], en[i] + along * (seg[i] / 2 + s), along, land))
    slots.sort(key=lambda t: -t[0])  # longest quays first
    for kind in kinds:
        L, D = kind["size"]
        done = False
        for row in range(3):
            for _, p, along, land in slots:
                dist = cfg["building_setback_m"] + D / 2 + 1.0 + row * (D + cfg["building_gap_m"])
                centre = p + land * dist
                fp = footprint(centre, along, L, D)
                if zone.contains(fp) and (taken.is_empty or fp.distance(taken) >= cfg["building_gap_m"]):
                    placed.append({"kind": kind, "centre": centre, "along": along, "L": L, "D": D})
                    taken = taken.union(fp)
                    done = True
                    break
            if done:
                break
        if not done:
            print("  no room for building '%s'" % kind["name"])
    return placed


# ----------------------------------------------------------------------------
# Textures for buildings and surfaces
# ----------------------------------------------------------------------------

PX_PER_M = 40


def _grime(rgb, rng, amount=0.08):
    h, w = rgb.shape[:2]
    noise = rng.random((h // 8 + 2, w // 8 + 2))
    from scipy import ndimage as ndi
    up = ndi.zoom(noise, (h / (noise.shape[0] - 1), w / (noise.shape[1] - 1)), order=1)[:h, :w]
    streak = np.linspace(0, 1, h)[:, None] ** 3  # darker towards the bottom
    return rgb * (1 - amount * up[..., None] - 0.06 * streak[..., None])


def make_facade_texture(path, kind, rng, plain=False):
    """One bay wide and the full wall height (row 0 = top of the wall)."""
    bay = kind.get("bay_m", 6.0)
    H = kind["wall_height"] + (kind.get("parapet", 0.8) if kind.get("roof", "flat") == "flat" else 0.0)
    w, h = int(bay * PX_PER_M), int(H * PX_PER_M)
    scale = min(1.0, 512 / max(w, h))
    w, h = max(8, int(w * scale)), max(8, int(h * scale))
    m = PX_PER_M * scale

    def y(metres_from_ground):
        return int(h - metres_from_ground * m)

    wall = tuple(kind.get("wall_colour", (232, 230, 222)))
    trim = tuple(kind.get("trim_colour", (40, 92, 160)))
    img = Image.new("RGB", (w, h), wall)
    d = ImageDraw.Draw(img)
    d.rectangle([0, y(kind.get("plinth_m", 0.8)), w, h], fill=trim)
    d.rectangle([0, 0, w, max(1, int(0.35 * m))], fill=trim)
    if not plain:
        style = kind.get("facade", "windows")
        if style == "hall":
            # Wide shuttered opening with a clerestory window strip above
            x0, x1 = int(w * 0.15), int(w * 0.85)
            d.rectangle([x0, y(4.2), x1, y(0.0)], fill=(70, 86, 104))
            for k in range(1, 12):
                yy = y(4.2) + int((y(0.0) - y(4.2)) * k / 12)
                d.line([x0, yy, x1, yy], fill=(58, 72, 88), width=1)
            d.rectangle([x0 - 2, y(4.4), x1 + 2, y(4.2)], fill=trim)
            if kind["wall_height"] > 6:
                d.rectangle([int(w * 0.1), y(kind["wall_height"] - 0.6), int(w * 0.9), y(kind["wall_height"] - 1.6)],
                            fill=(60, 80, 100))
        elif style == "roller":
            x0, x1 = int(w * 0.2), int(w * 0.8)
            d.rectangle([x0, y(4.8), x1, y(0.0)], fill=(150, 152, 150))
            for k in range(1, 24):
                yy = y(4.8) + int((y(0.0) - y(4.8)) * k / 24)
                d.line([x0, yy, x1, yy], fill=(122, 124, 122), width=1)
            d.rectangle([x0 - 3, y(5.0), x1 + 3, y(4.8)], fill=trim)
        elif style == "cold":
            x0, x1 = int(w * 0.32), int(w * 0.68)
            d.rectangle([x0, y(3.2), x1, y(0.0)], fill=(215, 220, 222))
            d.rectangle([x0, y(3.2), x1, y(3.0)], fill=trim)
        # Windows on every storey above the ground floor (and on the ground floor for offices)
        storey = kind.get("storey_m", 3.5)
        first = 0 if style == "windows" else 1
        nwin = kind.get("windows_per_bay", 2)
        for s in range(first, int(kind["wall_height"] // storey)):
            base = s * storey + 1.0
            if style != "windows" and base + 1.4 > kind["wall_height"] - 0.4:
                continue
            for k in range(nwin):
                cx = w * (k + 0.5) / nwin
                hw = min(0.75 * m, w / nwin * 0.32)
                d.rectangle([int(cx - hw) - 2, y(base + 1.45) - 2, int(cx + hw) + 2, y(base) + 2], fill=trim)
                d.rectangle([int(cx - hw), y(base + 1.4), int(cx + hw), y(base)], fill=(52, 70, 92))
                d.line([int(cx), y(base + 1.4), int(cx), y(base)], fill=trim, width=2)
    rgb = _grime(np.asarray(img, dtype=float), rng)
    Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).save(path, quality=90)


def make_corrugated_texture(path, rng, size=256):
    """Corrugated roof sheeting, ribs running down the slope (along v)."""
    x = np.arange(size)
    ribs = 0.75 + 0.25 * np.cos(x / size * 2 * math.pi * 20)
    rgb = np.ones((size, size, 3)) * 200 * ribs[None, :, None]
    seams = (np.arange(size) % (size // 2)) < 2
    rgb[seams] *= 0.8
    rgb = _grime(rgb, rng, 0.12)
    Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).save(path, quality=90)


def make_asphalt_texture(path, rng, size=256):
    base = 72 + 18 * rng.random((size, size))
    from scipy import ndimage as ndi
    base = ndi.gaussian_filter(base, 0.7)
    rgb = np.stack([base, base, base * 1.04], axis=-1)
    Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).save(path, quality=90)


def make_plaster_texture(path, rng, colour=(226, 222, 210), size=256):
    rgb = np.ones((size, size, 3)) * np.array(colour, dtype=float)
    rgb = _grime(rgb, rng, 0.1)
    Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).save(path, quality=90)


def paint_polygon(rgb, poly, to_px, colour, rng):
    """Paint a metric polygon onto a terrain texture array (anti-aliased by supersampling)."""
    if poly is None or poly.is_empty:
        return
    h, w = rgb.shape[:2]
    ss = 2
    mask_img = Image.new("L", (w * ss, h * ss), 0)
    d = ImageDraw.Draw(mask_img)
    for p in getattr(poly, "geoms", [poly]):
        xs, ys = to_px(np.array(p.exterior.coords))
        d.polygon(list(zip(xs * ss, ys * ss)), fill=255)
    mask = np.asarray(mask_img.resize((w, h), Image.BILINEAR), dtype=float)[..., None] / 255.0
    tone = np.array(colour, dtype=float) * (0.92 + 0.12 * rng.random((h, w, 1)))
    rgb[:] = rgb * (1 - mask) + tone * mask
