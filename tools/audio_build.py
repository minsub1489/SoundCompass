"""Build public/audio/<id>.wav from tools/audio_sources.json (CC0 copies + Stable Audio 3 via local ComfyUI).

Run with the ComfyUI venv python (has numpy):
  <venv>/python.exe -X utf8 tools/audio_build.py [--force] [--only id1,id2]
"""
import argparse, json, subprocess, sys, time, urllib.error, urllib.request
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "public" / "audio"
SERVER = "http://127.0.0.1:8188"
COMFY_OUT = Path(r"C:\Users\Home\AppData\Local\Comfy-Desktop\ComfyUI-Shared\output")
SR = 48000
LUFS = {"sfx": -18, "amb": -30, "bgm": -26}
LOOP_XFADE_S = 2.0


def sa3_workflow(prompt, seconds, seed, prefix):
    # Same graph as D:\ComfyUI\workflows\stable_audio3_run.py; only prompt/length/seed/prefix change.
    return {
        "ck": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": "stable_audio_3_medium.safetensors"}},
        "cl": {"class_type": "CLIPLoader", "inputs": {"clip_name": "t5gemma_b_b_ul2.safetensors", "type": "stable_audio", "device": "default"}},
        "pos": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["cl", 0], "text": prompt}},
        "neg": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["cl", 0], "text": ""}},
        "lat": {"class_type": "EmptyLatentAudio", "inputs": {"seconds": float(seconds), "batch_size": 1}},
        "ks": {"class_type": "KSampler", "inputs": {"model": ["ck", 0], "positive": ["pos", 0], "negative": ["neg", 0], "latent_image": ["lat", 0],
                                                     "seed": seed, "control_after_generate": "fixed", "steps": 8, "cfg": 1.0,
                                                     "sampler_name": "lcm", "scheduler": "simple", "denoise": 1.0}},
        "dec": {"class_type": "VAEDecodeAudio", "inputs": {"samples": ["ks", 0], "vae": ["ck", 2]}},
        "sv": {"class_type": "SaveAudio", "inputs": {"audio": ["dec", 0], "filename_prefix": prefix}},
    }


def generate_sa3(prompt, seconds, seed, prefix):
    body = json.dumps({"prompt": sa3_workflow(prompt, seconds, seed, prefix)}).encode()
    req = urllib.request.Request(SERVER + "/prompt", data=body, headers={"Content-Type": "application/json"})
    try:
        pid = json.loads(urllib.request.urlopen(req).read())["prompt_id"]
    except urllib.error.HTTPError as e:
        raise RuntimeError(e.read().decode()[:2000])
    t0 = time.time()
    while time.time() - t0 < 600:
        time.sleep(2)
        hist = json.loads(urllib.request.urlopen(f"{SERVER}/history/{pid}").read())
        if pid not in hist:
            continue
        entry = hist[pid]
        if entry["status"]["status_str"] != "success":
            raise RuntimeError(json.dumps(entry["status"])[:2000])
        f = entry["outputs"]["sv"]["audio"][0]
        return COMFY_OUT / f.get("subfolder", "") / f["filename"]
    raise RuntimeError("ComfyUI timed out")


def ffmpeg(args):
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", *args], check=True)


def decode(path, channels):
    raw = subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(path), "-f", "f32le", "-ac", str(channels), "-ar", str(SR), "-"],
                         check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, channels).copy()


def encode(samples, path, channels):
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "f32le", "-ac", str(channels), "-ar", str(SR), "-i", "-",
                    "-c:a", "pcm_s16le", str(path)], input=samples.astype(np.float32).tobytes(), check=True)


def process(src, dst, kind, max_seconds=None):
    tmp = dst.with_suffix(".tmp.wav")
    if kind == "sfx":
        trim = "silenceremove=start_periods=1:start_threshold=-50dB,areverse,silenceremove=start_periods=1:start_threshold=-50dB,areverse"
        cap = f",atrim=0:{max_seconds}" if max_seconds else ""
        fade = ",areverse,afade=t=in:d=0.05,areverse"
        ffmpeg(["-i", str(src), "-af", f"{trim}{cap}{fade},loudnorm=I={LUFS[kind]}:TP=-1.5", "-ac", "1", "-ar", str(SR), "-c:a", "pcm_s16le", str(dst)])
        return
    ffmpeg(["-i", str(src), "-af", f"loudnorm=I={LUFS[kind]}:TP=-1.5", "-ac", "2", "-ar", str(SR), "-c:a", "pcm_s16le", str(tmp)])
    x = decode(tmp, 2)
    tmp.unlink()
    n = int(LOOP_XFADE_S * SR)
    # Seamless loop: fold the last n samples into the first n with an equal-power crossfade.
    t = np.linspace(0, np.pi / 2, n, dtype=np.float32)[:, None]
    y = x[:-n].copy()
    y[:n] = x[:n] * np.sin(t) + x[-n:] * np.cos(t)
    encode(y, dst, 2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    only = set(filter(None, a.only.split(",")))
    cfg = json.loads((ROOT / "tools" / "audio_sources.json").read_text(encoding="utf-8"))
    OUT.mkdir(parents=True, exist_ok=True)
    failed = []
    for item in cfg["items"]:
        if only and item["id"] not in only:
            continue
        variants = item.get("variants", 0)
        for n in range(1, variants + 1) if variants else [0]:
            name = f"{item['id']}_{n}" if variants else item["id"]
            dst = OUT / f"{name}.wav"
            if dst.exists() and not a.force:
                print("skip", name)
                continue
            try:
                if item["source"] == "cc0":
                    src = Path(cfg["cc0Root"]) / item["path"].replace("{n}", str(n))
                else:
                    src = generate_sa3(item["prompt"], item["seconds"], item["seed"] + n, f"audio/testbed_{name}")
                process(src, dst, item["kind"], item.get("maxSeconds"))
                print("ok", name, flush=True)
            except Exception as e:  # keep going; report at the end
                failed.append(name)
                print("FAIL", name, e, flush=True)
    if failed:
        print("failed:", ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
