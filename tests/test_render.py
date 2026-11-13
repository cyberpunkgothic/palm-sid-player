"""Render test for the SID player core (PC builds from tools/pc_build.bat).

  python tests/test_render.py                 check build/pc/sid2wav.exe against tests/golden.json
  python tests/test_render.py --exe NAME      check another build (e.g. sid2wav_fixed; sid2wav_core_arm
                                              from tools/arm_check.sh; sidcore = the Palm core block
                                              from tools/blob_build.sh + tools/blob_check.sh)
  python tests/test_render.py --golden        rewrite tests/golden.json from build/pc/sid2wav_ref.exe
                                              (unchanged libsidplay 1.36.60: the reference)
  python tests/test_render.py --compare A B   render with builds A and B, report how far apart they
                                              are, keep the WAVs in build/compare/ for listening

Tunes in tests/tunes/user (the user's own collection, not in the repository) are skipped if absent.
Extra local cases, with their checksums, can go in tests/tunes/user/local_cases.json:
  {"cases": [["user/name.sid", 1], ...], "golden": {"name_1": "<sha1>", ...}}
"""
import argparse, array, hashlib, json, math, os, subprocess, sys, wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden.json")
LOCAL = os.path.join(ROOT, "tests", "tunes", "user", "local_cases.json")
SECONDS = 60

# (file, subtune).  The user's filter-heavy tunes (plus any local cases, see above).
CASES = [
    ("user/Elvira-The_Arcade_Game.sid", 1), ("user/Elvira-The_Arcade_Game.sid", 2),
    ("user/Back_to_the_Future_II.sid", 1), ("user/Back_to_the_Future_III.sid", 1),
    ("user/Blade_Runner.sid", 1), ("user/Chevy_Chase.sid", 1), ("user/Elite.sid", 1),
    ("user/Freestyler.sid", 1), ("user/Ghostbusters.sid", 1), ("user/Kane.sid", 1),
    ("user/Rainbow_Islands.sid", 1), ("user/Supremacy.sid", 1),
]


def load_local():
    """The local cases and their checksums, if tests/tunes/user/local_cases.json exists."""
    if not os.path.exists(LOCAL):
        return [], {}
    with open(LOCAL) as f:
        d = json.load(f)
    return [tuple(c) for c in d.get("cases", [])], d.get("golden", {})


def exe_path(name):
    return os.path.join(ROOT, "build", "pc", name if name.endswith(".exe") else name + ".exe")


def case_name(tune, song):
    return "%s_%d" % (os.path.splitext(os.path.basename(tune))[0], song)


def wsl_path(p):
    p = os.path.abspath(p).replace("\\", "/")
    return "/mnt/" + p[0].lower() + p[2:]


def render(exe, tune, song, out):
    path = os.path.join(ROOT, "tests", "tunes", tune)
    if not os.path.exists(path):
        return False
    if exe == "sidcore":
        # the relocatable Palm core (tools/blob_build.sh), loaded by tests/blob_harness.c under qemu-arm
        subprocess.run(["wsl.exe", "-d", "Ubuntu-24.04", "-u", "chris", "--", "qemu-arm",
                        "/mnt/c/Projects/sid-player/build/arm/blob_harness",
                        wsl_path(os.environ.get("SIDCORE_BIN",              # (another build of it)
                                                os.path.join(ROOT, "build", "blob", "sidcore.bin"))),
                        wsl_path(path), wsl_path(out), "-s", str(song), "-t", str(SECONDS)],
                       check=True, stdout=subprocess.DEVNULL)
        return True
    if exe.endswith("_arm"):
        # ARM build (tools/arm_check.sh) under qemu-arm in WSL
        arm = "/mnt/c/Projects/sid-player/build/arm/" + exe
        subprocess.run(["wsl.exe", "-d", "Ubuntu-24.04", "-u", "chris", "--", "qemu-arm", arm,
                        wsl_path(path), wsl_path(out), "-s", str(song), "-t", str(SECONDS)],
                       check=True, stdout=subprocess.DEVNULL)
        return True
    subprocess.run([exe_path(exe), path, out, "-s", str(song), "-t", str(SECONDS)],
                   check=True, stdout=subprocess.DEVNULL)
    return True


def samples(path):
    with wave.open(path) as w:
        return array.array("h", w.readframes(w.getnframes()))


def sha1(path):
    with open(path, "rb") as f:
        return hashlib.sha1(f.read()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default="sid2wav")
    ap.add_argument("--golden", action="store_true")
    ap.add_argument("--compare", nargs=2, metavar=("A", "B"))
    args = ap.parse_args()
    local_cases, local_golden = load_local()
    cases = CASES + local_cases
    tmp = os.path.join(ROOT, "build", "test")
    os.makedirs(tmp, exist_ok=True)

    if args.compare:
        a, b = args.compare
        outdir = os.path.join(ROOT, "build", "compare")
        os.makedirs(outdir, exist_ok=True)
        print("%-34s %8s %8s %9s" % ("tune", "maxdiff", "rmsdiff", "SNR dB"))
        for tune, song in cases:
            n = case_name(tune, song)
            fa, fb = os.path.join(outdir, n + "_" + a + ".wav"), os.path.join(outdir, n + "_" + b + ".wav")
            if not (render(a, tune, song, fa) and render(b, tune, song, fb)):
                continue
            sa, sb = samples(fa), samples(fb)
            d = [x - y for x, y in zip(sa, sb)]
            sig = math.sqrt(sum(x * x for x in sa) / len(sa)) or 1
            rd = math.sqrt(sum(x * x for x in d) / len(d))
            snr = "identical" if rd == 0 else "%9.1f" % (20 * math.log10(sig / rd))
            print("%-34s %8d %8.1f %9s" % (n, max(map(abs, d)), rd, snr))
        return 0

    if args.golden:
        golden, local = {}, {}
        for tune, song in cases:
            out = os.path.join(tmp, "g.wav")
            if render("sid2wav_ref", tune, song, out):
                ((tune, song) in local_cases and local or golden)[case_name(tune, song)] = sha1(out)
        with open(GOLDEN, "w") as f:
            json.dump(golden, f, indent=1, sort_keys=True)
        if local_cases:
            with open(LOCAL, "w") as f:
                json.dump({"cases": [list(c) for c in local_cases], "golden": local}, f, indent=1, sort_keys=True)
        print("wrote %d checksums from sid2wav_ref" % (len(golden) + len(local)))
        return 0

    with open(GOLDEN) as f:
        golden = json.load(f)
    golden.update(local_golden)
    bad = ran = 0
    for tune, song in cases:
        n = case_name(tune, song)
        out = os.path.join(tmp, "t.wav")
        if n not in golden or not render(args.exe, tune, song, out):
            print("skip", n)
            continue
        ran += 1
        if sha1(out) != golden[n]:
            bad += 1
            print("DIFFERS", n)
    print("%s: %d/%d identical to the reference" % (args.exe, ran - bad, ran))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
