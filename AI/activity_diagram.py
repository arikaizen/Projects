#!/usr/bin/env python3
"""Render an activity diagram from a JSON spec.

Usage:
  python3 activity_diagram.py [INPUT.json] [OUTPUT.png]

Defaults: ./activity_diagram.json -> ./activity_diagram.png

The JSON spec defines:
  - title, canvas (xlim, ylim, figsize), node_size (w, h)
  - styles: map of type -> { shape, color }
      shape in { rounded, pill, diamond, sawtooth }
  - nodes: [ { id, x, y, text, type } ]
  - edges: [ { from, to, label?, style? } ]   style in { solid (default), dashed }
  - legend: [ { type, label } ]

Edit the JSON to add/remove nodes, change colors, reposition, or change labels.
No code edits required.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Polygon
from matplotlib.lines import Line2D


def draw_node(ax, x, y, text, shape, color, w, h):
    if shape == "diamond":
        pts = [(x, y + h * 0.9), (x + w * 0.7, y), (x, y - h * 0.9), (x - w * 0.7, y)]
        ax.add_patch(Polygon(pts, closed=True, facecolor=color, edgecolor="black", linewidth=1.2))
    elif shape == "pill":
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="round,pad=0.05,rounding_size=0.35",
                                    facecolor=color, edgecolor="black", linewidth=1.4))
    elif shape == "sawtooth":
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="sawtooth,pad=0.05",
                                    facecolor=color, edgecolor="black", linewidth=1.0,
                                    linestyle="--"))
    else:  # rounded
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="round,pad=0.04,rounding_size=0.12",
                                    facecolor=color, edgecolor="black", linewidth=1.0))
    ax.text(x, y, text, ha="center", va="center", fontsize=7.2, wrap=True)


def draw_edge(ax, src_xy, dst_xy, label, style):
    linestyle = "--" if style == "dashed" else "-"
    arrow = FancyArrowPatch(src_xy, dst_xy,
                            arrowstyle="-|>", mutation_scale=10,
                            connectionstyle="arc3,rad=0.05",
                            color="#444", linewidth=0.9, linestyle=linestyle,
                            shrinkA=22, shrinkB=22)
    ax.add_patch(arrow)
    if label:
        mx, my = (src_xy[0] + dst_xy[0]) / 2, (src_xy[1] + dst_xy[1]) / 2
        ax.text(mx, my, label, fontsize=6.5, color="#1565C0",
                bbox=dict(facecolor="white", edgecolor="none", pad=1))


def render(spec_path: Path, out_path: Path) -> None:
    with spec_path.open() as f:
        spec = json.load(f)

    canvas = spec.get("canvas", {})
    xlim = canvas.get("xlim", [0, 10])
    ylim = canvas.get("ylim", [0, 10])
    figsize = canvas.get("figsize", [12, 12])
    size = spec.get("node_size", {"w": 2.6, "h": 0.7})
    w, h = size["w"], size["h"]
    styles = spec.get("styles", {})

    nodes = {n["id"]: n for n in spec["nodes"]}

    fig, ax = plt.subplots(figsize=figsize)
    ax.set_xlim(xlim)
    ax.set_ylim(ylim)
    ax.set_aspect("equal")
    ax.axis("off")
    if "title" in spec:
        ax.set_title(spec["title"], fontsize=14, fontweight="bold", pad=12)

    for edge in spec.get("edges", []):
        src = nodes[edge["from"]]
        dst = nodes[edge["to"]]
        draw_edge(ax, (src["x"], src["y"]), (dst["x"], dst["y"]),
                  edge.get("label", ""), edge.get("style", "solid"))

    for node in nodes.values():
        style = styles.get(node["type"], {"shape": "rounded", "color": "#CCCCCC"})
        draw_node(ax, node["x"], node["y"], node["text"],
                  style["shape"], style["color"], w, h)

    legend_items = spec.get("legend", [])
    if legend_items:
        handles = []
        for item in legend_items:
            color = styles.get(item["type"], {}).get("color", "#CCCCCC")
            shape = styles.get(item["type"], {}).get("shape", "rounded")
            marker = {"diamond": "D", "pill": "o", "sawtooth": "s"}.get(shape, "s")
            handles.append(Line2D([0], [0], marker=marker, color="w",
                                  markerfacecolor=color, markersize=10, label=item["label"]))
        ax.legend(handles=handles, loc="lower left", fontsize=8, frameon=True)

    plt.savefig(out_path, dpi=150, bbox_inches="tight", facecolor="white")
    print(f"Read  {spec_path}\nWrote {out_path}")


def main():
    here = Path(__file__).resolve().parent
    spec = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "activity_diagram.json"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else here / "activity_diagram.png"
    render(spec, out)


if __name__ == "__main__":
    main()
