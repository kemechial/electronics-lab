#!/usr/bin/env python3
"""Analyse a DHT11 logic-analyzer capture (exp02).

Input: a sigrok VCD export with the DHT11 DATA channel (default D0) and the
UART TX channel (default D1), plus the .sr file of the same capture. The .sr
file is used for the sample rate and for the sigrok decoders.

What it reports (datasheet references: docs/ref/DHT11.PDF):
  - sample rate and sample period
  - start pulse width (>= 18 ms, DHT p.6), release-to-response (20-40 us, p.6),
    response low/high (80/80 us, p.7), bit-start lows (50 us, p.7),
    final low (50 us, p.8)
  - the 40 bits: high width per bit, classified into 0 (26-28 us, DHT p.7) and
    1 (70 us, DHT p.8) with a threshold derived from the widths themselves
  - the 5 bytes, checksum check (DHT p.5), T and RH computed from the bytes
  - min/max high width per bit class
  - comparison with the raw= bytes of the firmware UART line in the same
    capture (sigrok `uart` decoder on the UART channel)
  - cross-check with the sigrok `am230x` decoder if it is installed: only its
    bit, byte and checksum annotations are used, never its T/RH values

Usage:
    python scripts/analyze_dht_edges.py docs/exp02-dht11/data/capture_001.vcd
    python scripts/analyze_dht_edges.py CAPTURE.vcd --sr CAPTURE.sr --dht D0 --uart D1 --csv widths.csv
"""
import argparse
import re
import subprocess
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sigrok_cli  # noqa: E402

START_MIN_US = 10_000.0       # a low longer than this is the MCU start pulse
RESP_WINDOW_US = (40.0, 120.0)
MIN_CLUSTER_GAP_US = 15.0

DATASHEET = {
    "start_low": ">= 18000 us (DHT p.6)",
    "release_to_response": "20-40 us (DHT p.6)",
    "resp_low": "80 us (DHT p.7)",
    "resp_high": "80 us (DHT p.7)",
    "bit_low": "50 us (DHT p.7)",
    "w0": "26-28 us (DHT p.7, Fig. 4)",
    "w1": "70 us (DHT p.8, Fig. 5)",
    "final_low": "50 us (DHT p.8)",
}


def parse_vcd(path):
    """Return (timescale_s, {name: [(t_s, value), ...]}) from a sigrok VCD."""
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    m = re.search(r"\$timescale\s+(\d+)\s*(s|ms|us|ns|ps|fs)\s+\$end", text)
    if not m:
        sys.exit("VCD: no $timescale")
    unit = {"s": 1, "ms": 1e-3, "us": 1e-6, "ns": 1e-9, "ps": 1e-12, "fs": 1e-15}[m.group(2)]
    ts = int(m.group(1)) * unit
    ids = {vid: name for vid, name in re.findall(r"\$var\s+wire\s+1\s+(\S+)\s+(\S+)\s+\$end", text)}
    changes = {name: [] for name in ids.values()}
    body = text.split("$enddefinitions $end", 1)[1]
    t = 0
    for tok in body.split():
        if tok.startswith("#"):
            t = int(tok[1:])
        elif tok[0] in "01" and tok[1:] in ids:
            changes[ids[tok[1:]]].append((t * ts, int(tok[0])))
    return ts, changes


def sr_samplerate(sr_path):
    meta = zipfile.ZipFile(sr_path).read("metadata").decode()
    m = re.search(r"samplerate=([\d.]+)\s*(Hz|kHz|MHz|GHz)", meta)
    return float(m.group(1)) * {"Hz": 1, "kHz": 1e3, "MHz": 1e6, "GHz": 1e9}[m.group(2)]


def segments(changes):
    """[(level, start_s, width_s)] between consecutive value changes (last open segment dropped)."""
    seg = []
    for (t0, v0), (t1, _) in zip(changes, changes[1:]):
        if t1 > t0:
            seg.append((v0, t0, t1 - t0))
    return seg


def two_means_threshold(widths, fallback):
    lo, hi = min(widths), max(widths)
    if hi - lo < MIN_CLUSTER_GAP_US:
        return fallback, "fallback (one cluster): mean bit-start low"
    thr = (lo + hi) / 2
    for _ in range(50):
        c0 = [w for w in widths if w < thr]
        c1 = [w for w in widths if w >= thr]
        if not c0 or not c1:
            break
        new = (sum(c0) / len(c0) + sum(c1) / len(c1)) / 2
        if abs(new - thr) < 1e-6:
            break
        thr = new
    return thr, "two-means midpoint of the 40 high widths"


def decode_dht(seg):
    """Locate start, response and 40 bits in the DHT channel segments (widths in us)."""
    us = [(lvl, t * 1e6, w * 1e6) for lvl, t, w in seg]
    i_start = next((i for i, (lvl, _, w) in enumerate(us) if lvl == 0 and w > START_MIN_US), None)
    if i_start is None:
        sys.exit("no start pulse (low > 10 ms) on the DHT channel")
    r = {"start_low": us[i_start][2], "start_t_us": us[i_start][1]}

    # after the start pulse: release (high), then response low + high
    i = i_start + 1
    if us[i][0] != 1:
        sys.exit("no release after the start pulse")
    r["release_to_response"] = us[i][2]
    i += 1
    (l0, _, wl), (l1, _, wh) = us[i], us[i + 1]
    if not (l0 == 0 and l1 == 1 and RESP_WINDOW_US[0] <= wl <= RESP_WINDOW_US[1]
            and RESP_WINDOW_US[0] <= wh <= RESP_WINDOW_US[1]):
        sys.exit(f"no 80/80 us response after release: {us[i][:3]}, {us[i + 1][:3]}")
    r["resp_low"], r["resp_high"] = wl, wh
    i += 2

    lows, highs = [], []
    for k in range(40):
        a, b = us[i + 2 * k], us[i + 2 * k + 1]
        if a[0] != 0 or b[0] != 1:
            sys.exit(f"bit {k}: levels do not alternate low/high")
        lows.append(a[2])
        highs.append(b[2])
    r["final_low"] = us[i + 80][2] if i + 80 < len(us) and us[i + 80][0] == 0 else None
    r["frame_end_us"] = us[i + 79][1] + us[i + 79][2]
    r["lows"], r["highs"] = lows, highs

    thr, how = two_means_threshold(highs, sum(lows) / len(lows))
    bits = [1 if h >= thr else 0 for h in highs]
    raw = [int("".join(map(str, bits[8 * n:8 * n + 8])), 2) for n in range(5)]   # MSB first (DHT p.5)
    r.update(thr=thr, thr_method=how, bits=bits, raw=raw,
             checksum_ok=(sum(raw[:4]) & 0xFF) == raw[4])
    return r


def uart_line(sr, uart_ch):
    p = sigrok_cli.run(["-i", str(sr), "-P",
                        f"uart:tx={uart_ch}:baudrate=115200:data_bits=8:parity=none:stop_bits=1.0",
                        "-B", "uart=tx"], capture_output=True)
    text = p.stdout.decode("ascii", errors="replace")
    lines = [ln.strip() for ln in text.splitlines() if ln.startswith("T=")]
    return lines[0] if lines else None, text


def am230x_check(sr, dht_ch):
    """Bits, bytes and checksum from the sigrok am230x decoder (T/RH annotations ignored)."""
    p = sigrok_cli.run(["-i", str(sr), "-P", f"am230x:sda={dht_ch}:device=dht11",
                        "-A", "am230x=bit:byte:checksum"], capture_output=True)
    out = p.stdout.decode("utf-8", errors="replace")
    bits = [int(b) for b in re.findall(r"Bit: ([01])", out)]
    raw = [int(b, 16) for b in re.findall(r"Byte: 0x([0-9a-fA-F]{2})", out)]
    chk = re.search(r"Checksum: (\w+)", out)
    return bits, raw, (chk.group(1) if chk else None)


def rng(values):
    return f"{min(values):.2f}-{max(values):.2f} us" if values else "-"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("vcd", type=Path)
    ap.add_argument("--sr", type=Path, help="sigrok session file of the same capture (default: VCD name with .sr)")
    ap.add_argument("--dht", default="D0", help="DHT11 DATA channel (default D0, see HARDWARE_NOTES.md)")
    ap.add_argument("--uart", default="D1", help="USART1 TX channel (default D1)")
    ap.add_argument("--csv", type=Path, help="write index,level,width_us of every DHT segment")
    args = ap.parse_args()
    sr = args.sr or args.vcd.with_suffix(".sr")

    ts, ch = parse_vcd(args.vcd)
    fs = sr_samplerate(sr) if sr.exists() else None
    print(f"capture: {args.vcd.name}  VCD timescale {ts * 1e12:g} ps")
    if fs:
        print(f"sample rate {fs / 1e6:g} MHz -> sample period {1e9 / fs:.2f} ns "
              f"(every width below is quantised to this, +/- 1 sample)")

    seg = segments(ch[args.dht])
    r = decode_dht(seg)

    print("\nDHT11 timing (measured vs datasheet)")
    rows = [("start_low", r["start_low"]), ("release_to_response", r["release_to_response"]),
            ("resp_low", r["resp_low"]), ("resp_high", r["resp_high"])]
    for name, v in rows:
        print(f"  {name:20s} {v:10.2f} us   datasheet {DATASHEET[name]}")
    print(f"  {'bit_low':20s} {rng(r['lows']):>21s}   datasheet {DATASHEET['bit_low']}")
    fl = f"{r['final_low']:.2f} us" if r["final_low"] else "not captured"
    print(f"  {'final_low':20s} {fl:>13s}   datasheet {DATASHEET['final_low']}")
    print(f"  frame: start of response to end of bit 40 = "
          f"{r['frame_end_us'] - (r['start_t_us'] + r['start_low'] + r['release_to_response']):.1f} us")

    w0 = [h for h, b in zip(r["highs"], r["bits"]) if b == 0]
    w1 = [h for h, b in zip(r["highs"], r["bits"]) if b == 1]
    print(f"\nbit classes (threshold {r['thr']:.2f} us, {r['thr_method']})")
    print(f"  w0 (n={len(w0):2d}) {rng(w0)}   datasheet {DATASHEET['w0']}")
    print(f"  w1 (n={len(w1):2d}) {rng(w1)}   datasheet {DATASHEET['w1']}")

    raw = r["raw"]
    print("\nbytes " + " ".join(f"{b:02X}" for b in raw) +
          f"  checksum {'OK' if r['checksum_ok'] else 'ERROR'} "
          f"((0x{raw[0]:02X}+0x{raw[1]:02X}+0x{raw[2]:02X}+0x{raw[3]:02X}) & 0xFF = 0x{sum(raw[:4]) & 0xFF:02X})")
    print(f"  computed from bytes: RH = {raw[0]} % (dec byte {raw[1]}), T = {raw[2]} C (dec byte {raw[3]})")

    line, _ = uart_line(sr, args.uart) if sr.exists() else (None, "")
    print("\nfirmware UART line in the same capture:")
    if line:
        print(f"  {line}")
        m = re.search(r"raw=([0-9A-F,]+)", line)
        fw_raw = [int(x, 16) for x in m.group(1).split(",")] if m else None
        print(f"  raw= bytes {'MATCH' if fw_raw == raw else 'DIFFER'} the analyzer decode")
    else:
        print("  no T= line decoded")

    if sr.exists():
        bits, aw_raw, chk = am230x_check(sr, args.dht)
        print("\nsigrok am230x cross-check (bits/bytes/checksum only):")
        if not bits:
            print("  no decode: the capture starts inside the start pulse (trigger on its falling edge),"
                  " and am230x needs the high idle before the start edge")
        else:
            print(f"  bytes {' '.join(f'{b:02X}' for b in aw_raw)}  checksum {chk}  "
                  f"bits {'MATCH' if bits == r['bits'] else 'DIFFER'}, "
                  f"bytes {'MATCH' if aw_raw == raw else 'DIFFER'}")

    if args.csv:
        with open(args.csv, "w", encoding="utf-8") as fp:
            fp.write("index,level,width_us\n")
            for k, (lvl, _, w) in enumerate(seg):
                fp.write(f"{k},{lvl},{w * 1e6:.3f}\n")
        print(f"\nsegments written to {args.csv}")


if __name__ == "__main__":
    main()
