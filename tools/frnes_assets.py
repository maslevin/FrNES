#!/usr/bin/env python3
"""frnes_assets.py ASSETS_DIR OUT_DIR [--jobs N] [--only NAME] [--limit N]: the menus' box art and manuals.

Compiles the scans in ASSETS_DIR into Dreamcast textures under OUT_DIR, a folder per game (copied to the SD card as
/sd/FrNES/art, or served over dc-tool-ip for development):

    OUT_DIR/<key>/info.txt      what's there and each texture's picture size (the rest of the texture is black)
    OUT_DIR/<key>/box.dt        the US box, 256 x 512 (the box fitted into 256 x 352; --box 256 or 128: smaller)
    OUT_DIR/<key>/box_j.dt      the Famicom box (landscape), 512 x 256 (--box: the same)
    OUT_DIR/<key>/page000.dt .. the manual's pages, 1024 x 1024 each (the page fitted into it; --page 512 or 256)
    OUT_DIR/<key>/pagej000.dt . the Famicom manual's pages, the same (the viewer's X switches manuals)

To fit a CD: --estimate prints the library's size at the chosen sizes; --box, --page, --no-famicom, --no-manuals
and --no-boxes trade pictures for room. A game made at other sizes is made again.
    OUT_DIR/index.txt           each game's key and name

<key> is the game's name as the menus match a ROM's (frontend/game_name.c: lower case letters and digits, without
what's in brackets, "The", "Disney's", "Part", apostrophes; II-VI as digits), so "Legend of Zelda, The - Manual
(Clearscan).pdf", "Legend of Zelda, The (USA).png" and a ROM "Legend of Zelda, The (U) (PRG1).nes" meet at
"legendofzelda". The textures are KOS pvrtex .dt files (RGB565, VQ compressed: about 34 kB a box, 258 kB a page).

Inputs, a folder each in ASSETS_DIR (any folders below them are searched too; a file's name is its game's):
    nes_boxes/       US boxes: <game>.png / .jpg, e.g. "Legend of Zelda, The (USA).png"
    famicom_boxes/   Famicom boxes: <game>.png / .jpg
    nes_manuals/     manuals: <game>.pdf, or "<game> - Manual (...).pdf" (with several for a game, a Clearscan scan
                     first, then Searchable, then Cleartext, then any other)
    famicom_manuals/ Famicom manuals, the same

Needs ImageMagick (magick), Ghostscript (gs) and KOS's pvrtex ($KOS_BASE/utils/pvrtex/pvrtex). A game already
compiled is skipped unless its sources are newer.
"""
import argparse, glob, multiprocessing, os, re, shutil, subprocess, sys, tempfile

PVRTEX = os.path.join(os.environ.get("KOS_BASE", "/opt/toolchains/dc/kos"), "utils/pvrtex/pvrtex")
# the sizes --box and --page choose from: a box's texture's long side (US boxes portrait, fitted into its width by
# 0.6875 of its height, Famicom boxes landscape), a page's square texture's side
BOX_SIZES = (128, 256, 512)
PAGE_SIZES = (256, 512, 1024)
# (Ghostscript's resolution for the pages before they're fitted, per 1024 pixels of page: NES manuals are small,
# ~4.5 x 7 inches)
PAGE_DPI_1024 = 220


def box_textures(size):
    """the US and Famicom boxes' textures and the box fitted into each, for a long side of size"""
    return ((size // 2, size), (size // 2, size * 11 // 16)), ((size, size // 2), (size, size // 2))


def vq_bytes(width, height):
    """a VQ .dt file's size: the header, a full codebook, an index a 2 x 2 texels"""
    return 32 + 2048 + width * height // 4
MANUAL_KINDS = ("Clearscan", "Searchable", "Cleartext")
ROMAN = {"ii": "2", "iii": "3", "iv": "4", "v": "5", "vi": "6"}


def name_key(name, title=False):
    """frontend/game_name.c's name_key, the same steps"""
    words, depth = [], 0
    for c in name:
        if c in "([":
            depth += 1
        elif c in ")]" and depth > 0:
            depth -= 1
        elif depth == 0 and c != "'":
            words.append(c.lower())
    text = "".join(words)
    if title:
        for mark in (":", "--", " - "):
            i = text.find(mark)
            if i > 0:
                text = text[:i]
    text = text.replace(", the", "     ")
    key, first = [], True
    for word in re.split(r"[ ,.!?;:\-/]+", text):
        if not word:
            continue
        if (first and word == "the") or word in ("disneys", "part"):
            continue
        first = False
        word = ROMAN.get(word, word)
        if word == "&":
            word = "and"
        key.append("".join(c if c.isalnum() else ("and" if c == "&" else "") for c in word))
    return "".join(key)


def game_name(path):
    name = os.path.splitext(os.path.basename(path))[0]
    return re.sub(r"\s+-\s+Manual\b.*$", "", name)


def run(args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def picture_size(path):
    out = subprocess.run(["magick", "identify", "-format", "%w %h", path + "[0]"], check=True, capture_output=True, text=True)
    return tuple(int(v) for v in out.stdout.split()[:2])


def texture(source, out, fit, canvas, work):
    """the picture fitted into fit (keeping its shape), on a black canvas, VQ compressed; its size in the texture"""
    fitted = os.path.join(work, os.path.basename(out) + ".png")
    run(["magick", source + "[0]", "-colorspace", "sRGB", "-resize", "%dx%d" % fit, "-background", "black",
         "-gravity", "northwest", "-extent", "%dx%d" % canvas, "-type", "TrueColor", fitted])
    width, height = picture_size(source)
    scale = min(fit[0] / width, fit[1] / height)
    run([PVRTEX, "-i", fitted, "-o", out, "-f", "RGB565", "-c"])
    os.remove(fitted)
    return min(fit[0], round(width * scale)), min(fit[1], round(height * scale))


def compile_game(job):
    key, game, out_dir = job["key"], job["name"], job["out"]
    folder = os.path.join(out_dir, key)
    os.makedirs(folder, exist_ok=True)
    info_path = os.path.join(folder, "info.txt")
    sources = [s for s in (job.get("box"), job.get("box_j"), job.get("manual"), job.get("manual_j")) if s]
    settings = "settings box %d page %d" % (job["box_size"], job["page_size"])
    if os.path.exists(info_path) and all(os.path.getmtime(s) <= os.path.getmtime(info_path) for s in sources):
        with open(info_path) as f:
            made = f.read()
        # (a library made before the sizes could be chosen was made at the defaults)
        if (settings in made) or ("settings " not in made and settings == "settings box 512 page 1024"):
            return key, "kept"
    # (a game made at other sizes: its textures go, so none is left over at the old size)
    for old in glob.glob(os.path.join(folder, "*.dt")):
        os.remove(old)
    (box_texture, box_fit), (box_j_texture, box_j_fit) = box_textures(job["box_size"])
    page_texture = (job["page_size"], job["page_size"])
    page_dpi = max(36, PAGE_DPI_1024 * job["page_size"] // 1024)
    lines = ["name %s" % game, settings]
    with tempfile.TemporaryDirectory() as work:
        try:
            for which, fit, canvas in (("box", box_fit, box_texture), ("box_j", box_j_fit, box_j_texture)):
                if job.get(which):
                    w, h = texture(job[which], os.path.join(folder, which + ".dt"), fit, canvas, work)
                    lines.append("%s %d %d" % (which, w, h))
            # (the manual's pages, page000.dt.., "pages" and "page" lines; the Famicom one's pagej000.dt..)
            for which, prefix, count_line, page_line in (("manual", "page", "pages", "page"),
                                                         ("manual_j", "pagej", "pages_j", "page_j")):
                if not job.get(which):
                    continue
                pages_dir = os.path.join(work, prefix)
                os.makedirs(pages_dir)
                run(["gs", "-q", "-dNOPAUSE", "-dBATCH", "-dSAFER", "-sDEVICE=png16m", "-r%d" % page_dpi,
                     "-sOutputFile=%s/%%03d.png" % pages_dir, job[which]])
                pages = sorted(glob.glob(os.path.join(pages_dir, "*.png")))
                sizes = []
                for i, page in enumerate(pages):
                    sizes.append(texture(page, os.path.join(folder, "%s%03d.dt" % (prefix, i)), page_texture, page_texture, work))
                    os.remove(page)
                lines.append("%s %d" % (count_line, len(pages)))
                lines += ["%s %d %d %d" % (page_line, i, w, h) for i, (w, h) in enumerate(sizes)]
        except subprocess.CalledProcessError as error:
            return key, "failed: %s %s" % (" ".join(error.cmd[:2]), error.stderr.decode(errors="replace")[:200])
    with open(info_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return key, "made"


# (ASSETS_DIR's folders, and the files taken from them)
BOX_FOLDERS = (("nes_boxes", "box"), ("famicom_boxes", "box_j"))
# (the NES manuals and the Famicom ones)
MANUAL_FOLDERS = (("nes_manuals", "manual"), ("famicom_manuals", "manual_j"))
PICTURE_EXTENSIONS = (".png", ".jpg", ".jpeg")


def files_in(folder, extensions):
    """the files below folder (sorted) with one of the extensions, any case"""
    found = []
    for root, _, names in os.walk(folder):
        found += [os.path.join(root, n) for n in names if os.path.splitext(n)[1].lower() in extensions and not n.startswith(".")]
    return sorted(found)


def manual_rank(path):
    """a manual's preference: Clearscan, Searchable, Cleartext, then anything else"""
    match = re.search(r" - Manual \((%s)\)" % "|".join(MANUAL_KINDS), os.path.basename(path))
    return MANUAL_KINDS.index(match.group(1)) if match else len(MANUAL_KINDS)


def gather(assets):
    """each game's sources, by key: US box, Famicom box, NES and Famicom manuals"""
    missing = [f for f, _ in BOX_FOLDERS + MANUAL_FOLDERS if not os.path.isdir(os.path.join(assets, f))]
    if len(missing) == len(BOX_FOLDERS) + len(MANUAL_FOLDERS):
        sys.exit("%s has none of %s" % (assets, ", ".join(missing)))
    games = {}
    def add(path, which):
        name = game_name(path)
        key = name_key(name)
        if not key:
            return
        game = games.setdefault(key, {"key": key, "name": re.sub(r"\s*[(\[].*$", "", name)})
        game.setdefault(which, path)
    for folder, which in BOX_FOLDERS:
        for path in files_in(os.path.join(assets, folder), PICTURE_EXTENSIONS):
            add(path, which)
    for folder, which in MANUAL_FOLDERS:
        manuals = {}
        for path in files_in(os.path.join(assets, folder), (".pdf",)):
            key = name_key(game_name(path))
            rank = manual_rank(path)
            if key and (key not in manuals or rank < manuals[key][0]):
                manuals[key] = (rank, path)
        for key, (_, path) in sorted(manuals.items()):
            add(path, which)
    return games


def pdf_pages(path):
    out = subprocess.run(["gs", "-q", "-dNODISPLAY", "-dNOSAFER", "-c", "(%s) (r) file runpdfbegin pdfpagecount = quit" % path],
                         capture_output=True, text=True)
    try:
        return int(out.stdout.strip().split()[-1])
    except (ValueError, IndexError):
        return 0


def estimate(games, args):
    """the library's size at the chosen sizes: each box and page a VQ texture of its fixed size"""
    (box_texture, _), (box_j_texture, _) = box_textures(args.box)
    boxes = sum("box" in g for g in games.values())
    boxes_j = sum("box_j" in g for g in games.values())
    manuals = [g[which] for g in games.values() for which in ("manual", "manual_j") if which in g]
    with multiprocessing.Pool(max(1, os.cpu_count() - 2)) as pool:
        pages = sum(pool.map(pdf_pages, manuals))
    sizes = (boxes * vq_bytes(*box_texture), boxes_j * vq_bytes(*box_j_texture), pages * vq_bytes(args.page, args.page))
    print("%d games: %d US boxes %.0f MB, %d Famicom boxes %.0f MB, %d manuals (%d pages) %.0f MB; %.0f MB in all"
          " (a CD holds about 650 MB)" % (len(games), boxes, sizes[0] / 1e6, boxes_j, sizes[1] / 1e6, len(manuals), pages,
                                          sizes[2] / 1e6, sum(sizes) / 1e6))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("assets")
    parser.add_argument("out")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() - 2))
    parser.add_argument("--only", help="compile just the games whose key contains this")
    parser.add_argument("--limit", type=int, help="compile at most this many games")
    parser.add_argument("--box", type=int, choices=BOX_SIZES, default=512,
                        help="a box texture's long side (512: 256 x 512, ~34 kB; 256: ~10 kB; 128: ~4 kB)")
    parser.add_argument("--page", type=int, choices=PAGE_SIZES, default=1024,
                        help="a manual page texture's side (1024: ~258 kB; 512: ~66 kB; 256: ~18 kB)")
    parser.add_argument("--no-manuals", action="store_true", help="boxes only")
    parser.add_argument("--no-boxes", action="store_true", help="manuals only")
    parser.add_argument("--no-famicom", action="store_true", help="no Famicom boxes or manuals")
    parser.add_argument("--estimate", action="store_true",
                        help="print what the library would take at these sizes (the manuals' pages counted), and stop")
    args = parser.parse_args()
    for tool in ("magick", "gs"):
        if not shutil.which(tool):
            sys.exit("%s isn't installed" % tool)
    if not os.path.exists(PVRTEX):
        sys.exit("no pvrtex at %s (build $KOS_BASE/utils/pvrtex)" % PVRTEX)
    games = gather(args.assets)
    for game in games.values():
        if args.no_manuals:
            game.pop("manual", None)
            game.pop("manual_j", None)
        if args.no_boxes:
            game.pop("box", None)
            game.pop("box_j", None)
        if args.no_famicom:
            game.pop("box_j", None)
            game.pop("manual_j", None)
    games = {key: game for key, game in games.items() if any(which in game for which in ("box", "box_j", "manual", "manual_j"))}
    if args.estimate:
        estimate(games, args)
        return
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "index.txt"), "w") as f:
        for key in sorted(games):
            f.write("%s\t%s\n" % (key, games[key]["name"]))
    jobs = [dict(games[key], out=args.out, box_size=args.box, page_size=args.page) for key in sorted(games)
            if not args.only or args.only in key]
    if args.limit:
        jobs = jobs[:args.limit]
    print("%d games (%d US boxes, %d Famicom boxes, %d manuals), %d to look at" % (
        len(games), sum("box" in g for g in games.values()), sum("box_j" in g for g in games.values()),
        sum(("manual" in g) + ("manual_j" in g) for g in games.values()), len(jobs)), flush=True)
    done = 0
    with multiprocessing.Pool(args.jobs) as pool:
        # (the manuals first: they're the long ones)
        jobs.sort(key=lambda job: ("manual" not in job) and ("manual_j" not in job))
        for key, result in pool.imap_unordered(compile_game, jobs):
            done += 1
            if result != "kept":
                print("%4d/%d %s: %s" % (done, len(jobs), key, result), flush=True)
    print("done", flush=True)


if __name__ == "__main__":
    main()
