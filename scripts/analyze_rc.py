#!/usr/bin/env python3
"""Least-squares exponential fit of the exp01 RC step-response log.

Input: a serial log containing CSV rows "charge,t_us,adc,mv" and
"discharge,t_us,adc,mv" (lines starting with '#' and anything else are ignored).
A log may hold several 10 s cycles; a new cycle starts at each "charge,0,..." row.

Model (both phases): V(t) = Vf + (V0 - Vf) * exp(-t / tau), with Vf, V0, tau free.
For a fixed tau the model is linear in (Vf, V0), so tau is found by a 1-D search on
the residual sum of squares, and Vf, V0 by linear least squares (variable projection).
Standard errors come from the full 3-parameter Jacobian: s^2 * (J^T J)^-1.
They assume independent noise and a correct model; systematic errors are not included.

Usage:
    python scripts/analyze_rc.py logs/device-monitor-YYMMDD-HHMMSS.log
    python scripts/analyze_rc.py LOG --cycle 2 --c-nf 10000 --out other.png
"""
import argparse
import math
import re
import sys
from pathlib import Path

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROW_RE = re.compile(r"^(charge|discharge),(\d+),(\d+),(\d+)\s*$")
ADC_MAX = 4095
DEFAULT_OUT = Path(__file__).resolve().parent.parent / "docs" / "exp01-rc-step" / "rc_curves.png"

# Plot palette (light surface)
SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
PHASE_COLOR = {"charge": "#2a78d6", "discharge": "#eb6834"}


def parse_log(path):
    """Return a list of complete cycles: [{'charge': (t_us, adc, mv), 'discharge': (...)}, ...]."""
    cycles = []
    current = None
    with open(path, encoding="utf-8", errors="replace") as fp:
        for line in fp:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            m = ROW_RE.match(line)
            if not m:
                continue
            phase, t_us, adc, mv = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
            if phase == "charge" and t_us == 0:
                current = {"charge": [], "discharge": []}
                cycles.append(current)
            if current is None:
                continue  # rows before the first full cycle (monitor opened mid-print)
            current[phase].append((t_us, adc, mv))

    complete = []
    for c in cycles:
        ch, dis = c["charge"], c["discharge"]
        if len(ch) > 10 and len(ch) == len(dis) and dis[0][0] == 0:
            complete.append({k: np.array(v, dtype=float).T for k, v in c.items()})
    return complete


def _linear_part(t, v, tau):
    e = np.exp(-t / tau)
    a = np.column_stack([1.0 - e, e])          # V = Vf * (1 - e) + V0 * e
    coef, *_ = np.linalg.lstsq(a, v, rcond=None)
    resid = v - a @ coef
    return coef, float(resid @ resid)


def fit_exponential(t, v):
    """Fit V(t) = Vf + (V0 - Vf) exp(-t/tau). Returns dict with values and standard errors."""
    span = float(t.max() - t.min())
    step = float(np.median(np.diff(t)))
    log_lo, log_hi = math.log(step), math.log(50.0 * span)

    # coarse grid on log(tau), then golden-section refinement around the best point
    grid = np.linspace(log_lo, log_hi, 400)
    ssr = [_linear_part(t, v, math.exp(g))[1] for g in grid]
    i = int(np.argmin(ssr))
    a, b = grid[max(i - 1, 0)], grid[min(i + 1, len(grid) - 1)]
    gr = (math.sqrt(5.0) - 1.0) / 2.0
    c, d = b - gr * (b - a), a + gr * (b - a)
    fc, fd = _linear_part(t, v, math.exp(c))[1], _linear_part(t, v, math.exp(d))[1]
    for _ in range(100):
        if fc < fd:
            b, d, fd = d, c, fc
            c = b - gr * (b - a)
            fc = _linear_part(t, v, math.exp(c))[1]
        else:
            a, c, fc = c, d, fd
            d = a + gr * (b - a)
            fd = _linear_part(t, v, math.exp(d))[1]
    tau = math.exp((a + b) / 2.0)
    (vf, v0), ssr_min = _linear_part(t, v, tau)

    n, p = len(t), 3
    e = np.exp(-t / tau)
    jac = np.column_stack([1.0 - e, e, (v0 - vf) * e * t / tau**2])
    s2 = ssr_min / (n - p)
    cov = s2 * np.linalg.inv(jac.T @ jac)
    se = np.sqrt(np.diag(cov))
    model = vf + (v0 - vf) * e
    return {
        "tau": tau, "tau_se": se[2],
        "vf": vf, "vf_se": se[0],
        "v0": v0, "v0_se": se[1],
        "rms": math.sqrt(ssr_min / n),
        "n": n, "model": model, "resid": v - model,
    }


def fit_cycle(cycle):
    """Fit both phases of one cycle; ADC-clipped samples (0 or 4095) are excluded."""
    out = {}
    for phase in ("charge", "discharge"):
        t, adc, mv = cycle[phase]
        keep = (adc > 0) & (adc < ADC_MAX)
        r = fit_exponential(t[keep], mv[keep])
        r.update(t=t[keep], mv=mv[keep], clipped=int((~keep).sum()))
        out[phase] = r
    return out


def dev_pct(measured, expected):
    return 100.0 * (measured - expected) / expected


def report(fits, tau_exp_us, r_ohm, c_nf, chosen):
    print(f"Expected tau = R*C = {r_ohm:.0f} ohm * {c_nf:.0f} nF = {tau_exp_us:.0f} us")
    print(f"{len(fits)} complete cycle(s) in log; detailed fit for cycle {chosen + 1}\n")

    f = fits[chosen]
    for phase in ("charge", "discharge"):
        r = f[phase]
        print(f"[{phase}] n={r['n']} (clipped excluded: {r['clipped']})")
        print(f"  tau = {r['tau']:.0f} +/- {r['tau_se']:.0f} us (1 sigma, statistical)"
              f"   dev vs R*C = {dev_pct(r['tau'], tau_exp_us):+.2f} %")
        print(f"  V0  = {r['v0']:.1f} +/- {r['v0_se']:.1f} mV")
        print(f"  Vf  = {r['vf']:.1f} +/- {r['vf_se']:.1f} mV")
        print(f"  RMS residual = {r['rms']:.2f} mV\n")

    if len(fits) > 1:
        print("cycle  tau_charge_us  tau_discharge_us  rms_ch_mV  rms_dis_mV")
        for k, fc in enumerate(fits):
            print(f"{k + 1:5d}  {fc['charge']['tau']:13.0f}  {fc['discharge']['tau']:16.0f}"
                  f"  {fc['charge']['rms']:9.2f}  {fc['discharge']['rms']:10.2f}")
        for phase in ("charge", "discharge"):
            taus = np.array([fc[phase]["tau"] for fc in fits])
            sd = taus.std(ddof=1)
            print(f"{phase:>9}: mean tau = {taus.mean():.0f} us, cycle-to-cycle sd = {sd:.0f} us,"
                  f" dev vs R*C = {dev_pct(taus.mean(), tau_exp_us):+.2f} %")


def plot(fit, tau_exp_us, out_path, title):
    plt.rcParams.update({
        "font.size": 9, "axes.edgecolor": TEXT_SECONDARY, "axes.labelcolor": TEXT_SECONDARY,
        "xtick.color": TEXT_SECONDARY, "ytick.color": TEXT_SECONDARY, "text.color": TEXT_PRIMARY,
    })
    fig, (ax_v, ax_r) = plt.subplots(
        2, 1, figsize=(8, 6.5), sharex=True, gridspec_kw={"height_ratios": [2.2, 1]},
        facecolor=SURFACE,
    )
    for ax in (ax_v, ax_r):
        ax.set_facecolor(SURFACE)
        ax.grid(True, color=GRID, linewidth=0.6)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)

    for phase in ("charge", "discharge"):
        r = fit[phase]
        col = PHASE_COLOR[phase]
        t_ms = r["t"] / 1000.0
        ax_v.plot(t_ms, r["mv"], "o", ms=3, color=col, alpha=0.35, mec="none",
                  label=f"{phase} measured")
        ax_v.plot(t_ms, r["model"], "-", lw=2, color=col,
                  label=f"{phase} fit: tau = {r['tau'] / 1000:.2f} ms "
                        f"({dev_pct(r['tau'], tau_exp_us):+.2f} %)")
        ax_r.plot(t_ms, r["resid"], "o", ms=3, color=col, alpha=0.6, mec="none",
                  label=f"{phase} (RMS {r['rms']:.2f} mV)")

    ax_v.axvline(tau_exp_us / 1000.0, color=TEXT_SECONDARY, lw=1, ls=":")
    ax_v.annotate(f"R*C = {tau_exp_us / 1000:.1f} ms", (tau_exp_us / 1000.0, 0.5),
                  xycoords=("data", "axes fraction"), xytext=(4, 0), textcoords="offset points",
                  color=TEXT_SECONDARY, fontsize=8)
    ax_v.set_ylabel("Node voltage (mV)")
    ax_v.set_title(title, loc="left", color=TEXT_PRIMARY)
    ax_v.legend(frameon=False, loc="center right", fontsize=8)

    ax_r.axhline(0, color=TEXT_SECONDARY, lw=1)
    ax_r.set_ylabel("Residual (mV)")
    ax_r.set_xlabel("Time since step (ms)")
    ax_r.legend(frameon=False, loc="lower right", bbox_to_anchor=(1.0, 1.0), ncol=2,
                fontsize=8, borderaxespad=0.2)

    fig.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=150, facecolor=SURFACE)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log", type=Path, help="serial log file")
    ap.add_argument("--r-ohm", type=float, default=9830.0, help="R in ohm (default: measured 9830)")
    ap.add_argument("--c-nf", type=float, default=10460.0, help="C in nF (default: measured 10460)")
    ap.add_argument("--cycle", type=int, default=0,
                    help="1-based cycle to plot and detail (default: last complete cycle)")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help=f"plot path (default: {DEFAULT_OUT})")
    args = ap.parse_args()

    cycles = parse_log(args.log)
    if not cycles:
        sys.exit(f"No complete charge+discharge cycle found in {args.log}")
    chosen = (args.cycle - 1) if args.cycle > 0 else len(cycles) - 1
    if not 0 <= chosen < len(cycles):
        sys.exit(f"--cycle must be 1..{len(cycles)}")

    tau_exp_us = args.r_ohm * args.c_nf / 1000.0   # ohm * nF = ns
    fits = [fit_cycle(c) for c in cycles]
    report(fits, tau_exp_us, args.r_ohm, args.c_nf, chosen)
    plot(fits[chosen], tau_exp_us, args.out,
         f"exp01 RC step, cycle {chosen + 1} of {len(cycles)} ({args.log.name})")
    print(f"\nPlot saved to {args.out}")


if __name__ == "__main__":
    main()
