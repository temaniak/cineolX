#!/usr/bin/env python3
"""Plot private per-program baseline measurements, not a correction scorecard."""
import csv
import sys
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main(directory):
    with (directory / "catalog-summary.csv").open() as file:
        rows = list(csv.DictReader(file))
    y = np.arange(len(rows))
    labels = [f"{int(row['index']) + 1:02d}  {row['name']}" for row in rows]
    def values(key):
        return [float(row[key]) if row[key] else np.nan for row in rows]
    fig, axes = plt.subplots(1, 2, figsize=(12, 10), sharey=True, gridspec_kw={"width_ratios": [1, 1.3]})
    axes[0].scatter(values("baseline_energy_error_db"), y, color="#246b9e", s=34)
    axes[0].axvline(0, color="#888888", linewidth=.8)
    axes[0].set_xlabel("Total energy difference vs reference (dB)")
    axes[0].set_yticks(y, labels);axes[0].invert_yaxis()
    axes[1].scatter(values("native_mean_abs_decay_error_common_percent"), y, color="#246b9e", s=34, label="Independent native")
    axes[1].scatter(values("injected_wcs_mean_abs_decay_error_common_percent"), y, color="#bf6537", s=34, marker="x", label="Reference WCS diagnostic")
    for n, row in enumerate(rows):
        if not int(row["common_decay_bands"]):
            axes[1].text(.2, n, "no usable paired fit", fontsize=8, color="#666666", va="center")
    axes[1].set_xlabel("Mean absolute decay-estimate difference (%)\nSame usable bands within each native/diagnostic pair")
    axes[1].set_xlim(left=0);axes[1].legend(loc="lower right", fontsize=9)
    for ax in axes:
        ax.grid(axis="x", alpha=.2);ax.set_axisbelow(True)
        ax.spines[["top", "right"]].set_visible(False)
    fig.suptitle("224XL: per-program sound baseline, all switches off", fontsize=16, x=.55)
    fig.text(.36, .94, "48 kHz wet A/C • noise seed 17, level 0.08 • Inverse Room levels opened", fontsize=10)
    fig.text(.05, .025, "The injected-WCS output is an offline diagnostic; these measurements are not a before/after plugin improvement.\n"
             "Unavailable decay fits are not zero errors. Full sound acceptance and physical-unit comparison remain open.", fontsize=9)
    fig.tight_layout(rect=[0, .07, 1, .925])
    fig.savefig(directory / "catalog-baseline.svg")
    fig.savefig(directory / "catalog-baseline.png", dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    main(Path(sys.argv[1]))
