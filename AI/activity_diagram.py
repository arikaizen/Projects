#!/usr/bin/env python3
"""Interactive UI for editing an activity diagram backed by a JSON spec.

Run:
  python3 activity_diagram.py [SPEC.json]

Defaults to ./activity_diagram.json.

Features:
  - Drag nodes with the mouse to reposition them.
  - Click a node to select it; edit text / type / x / y in the side panel.
  - Add / delete nodes.
  - Add / delete edges (with optional label).
  - Every change is auto-saved to the JSON spec and the PNG is re-exported.

Also supports a non-UI render mode:
  python3 activity_diagram.py --render [SPEC.json] [OUT.png]
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Polygon


# ---------- rendering ----------

def draw_node(ax, x, y, text, shape, color, w, h, selected=False):
    edge_color = "#FF1744" if selected else "black"
    edge_width = 2.4 if selected else 1.0
    if shape == "diamond":
        pts = [(x, y + h * 0.9), (x + w * 0.7, y), (x, y - h * 0.9), (x - w * 0.7, y)]
        ax.add_patch(Polygon(pts, closed=True, facecolor=color,
                             edgecolor=edge_color, linewidth=edge_width))
    elif shape == "pill":
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="round,pad=0.05,rounding_size=0.35",
                                    facecolor=color, edgecolor=edge_color,
                                    linewidth=edge_width))
    elif shape == "sawtooth":
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="sawtooth,pad=0.05",
                                    facecolor=color, edgecolor=edge_color,
                                    linewidth=edge_width, linestyle="--"))
    else:
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="round,pad=0.04,rounding_size=0.12",
                                    facecolor=color, edgecolor=edge_color,
                                    linewidth=edge_width))
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


def render_to_ax(ax, spec, selected_id=None):
    canvas = spec.get("canvas", {})
    ax.clear()
    ax.set_xlim(canvas.get("xlim", [0, 10]))
    ax.set_ylim(canvas.get("ylim", [0, 10]))
    ax.set_aspect("equal")
    ax.axis("off")
    if "title" in spec:
        ax.set_title(spec["title"], fontsize=12, fontweight="bold", pad=10)

    size = spec.get("node_size", {"w": 2.6, "h": 0.7})
    w, h = size["w"], size["h"]
    styles = spec.get("styles", {})
    nodes = {n["id"]: n for n in spec["nodes"]}

    for edge in spec.get("edges", []):
        if edge["from"] not in nodes or edge["to"] not in nodes:
            continue
        s, d = nodes[edge["from"]], nodes[edge["to"]]
        draw_edge(ax, (s["x"], s["y"]), (d["x"], d["y"]),
                  edge.get("label", ""), edge.get("style", "solid"))

    for node in nodes.values():
        style = styles.get(node["type"], {"shape": "rounded", "color": "#CCCCCC"})
        draw_node(ax, node["x"], node["y"], node["text"],
                  style["shape"], style["color"], w, h,
                  selected=(node["id"] == selected_id))

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


def render_png(spec_path: Path, out_path: Path) -> None:
    with spec_path.open() as f:
        spec = json.load(f)
    figsize = spec.get("canvas", {}).get("figsize", [12, 12])
    fig, ax = plt.subplots(figsize=figsize)
    render_to_ax(ax, spec)
    plt.savefig(out_path, dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)


# ---------- UI ----------

def _load_tk():
    """Import tkinter + TkAgg backend lazily and stash in module globals."""
    global tk, ttk, messagebox, simpledialog, FigureCanvasTkAgg
    import tkinter as _tk
    from tkinter import messagebox as _mb, simpledialog as _sd, ttk as _ttk
    matplotlib.use("TkAgg")
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg as _Canvas
    tk, ttk, messagebox, simpledialog, FigureCanvasTkAgg = _tk, _ttk, _mb, _sd, _Canvas


class DiagramApp:
    HIT_RADIUS = 0.9  # data units

    def __init__(self, root, spec_path: Path):
        self.root = root
        self.spec_path = spec_path
        self.png_path = spec_path.with_suffix(".png")
        self.spec = self._load()

        self.selected_id: str | None = None
        self.drag_offset: tuple[float, float] | None = None

        root.title(f"Activity Diagram Editor — {spec_path.name}")
        root.geometry("1400x900")

        main = ttk.PanedWindow(root, orient=tk.HORIZONTAL)
        main.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(main)
        right = ttk.Frame(main, width=360)
        main.add(left, weight=4)
        main.add(right, weight=1)

        figsize = self.spec.get("canvas", {}).get("figsize", [12, 12])
        self.fig, self.ax = plt.subplots(figsize=figsize)
        self.canvas = FigureCanvasTkAgg(self.fig, master=left)  # noqa: F821
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        self.canvas.mpl_connect("button_press_event", self._on_press)
        self.canvas.mpl_connect("motion_notify_event", self._on_motion)
        self.canvas.mpl_connect("button_release_event", self._on_release)

        self._build_side_panel(right)
        self.redraw()

    # ----- persistence -----

    def _load(self) -> dict:
        with self.spec_path.open() as f:
            return json.load(f)

    def save(self) -> None:
        with self.spec_path.open("w") as f:
            json.dump(self.spec, f, indent=2)
        try:
            render_png(self.spec_path, self.png_path)
        except Exception as e:
            print(f"PNG export failed: {e}", file=sys.stderr)
        self.status.set(f"Saved {self.spec_path.name}")

    # ----- UI building -----

    def _build_side_panel(self, parent):
        pad = {"padx": 6, "pady": 3}

        ttk.Label(parent, text="Selected node", font=("", 10, "bold")).pack(anchor="w", **pad)

        form = ttk.Frame(parent)
        form.pack(fill=tk.X, **pad)

        self.var_id = tk.StringVar()
        self.var_text = tk.StringVar()
        self.var_type = tk.StringVar()
        self.var_x = tk.StringVar()
        self.var_y = tk.StringVar()

        rows = [
            ("ID",   self.var_id,   True),
            ("Text", self.var_text, False),
            ("Type", self.var_type, False),
            ("X",    self.var_x,    False),
            ("Y",    self.var_y,    False),
        ]
        for i, (label, var, readonly) in enumerate(rows):
            ttk.Label(form, text=label).grid(row=i, column=0, sticky="w", padx=2, pady=2)
            entry = ttk.Entry(form, textvariable=var, width=28,
                              state=("readonly" if readonly else "normal"))
            entry.grid(row=i, column=1, sticky="we", padx=2, pady=2)
        form.columnconfigure(1, weight=1)

        ttk.Button(parent, text="Apply changes to selected", command=self.apply_node_edits)\
            .pack(fill=tk.X, **pad)

        ttk.Separator(parent).pack(fill=tk.X, pady=8)

        ttk.Label(parent, text="Nodes", font=("", 10, "bold")).pack(anchor="w", **pad)
        btns = ttk.Frame(parent); btns.pack(fill=tk.X, **pad)
        ttk.Button(btns, text="Add node",    command=self.add_node).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        ttk.Button(btns, text="Delete node", command=self.delete_node).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)

        ttk.Separator(parent).pack(fill=tk.X, pady=8)

        ttk.Label(parent, text="Edges", font=("", 10, "bold")).pack(anchor="w", **pad)
        self.edge_list = tk.Listbox(parent, height=12)
        self.edge_list.pack(fill=tk.BOTH, expand=True, **pad)

        ebtns = ttk.Frame(parent); ebtns.pack(fill=tk.X, **pad)
        ttk.Button(ebtns, text="Add edge",    command=self.add_edge).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        ttk.Button(ebtns, text="Delete edge", command=self.delete_edge).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)

        ttk.Separator(parent).pack(fill=tk.X, pady=8)

        bottom = ttk.Frame(parent); bottom.pack(fill=tk.X, **pad)
        ttk.Button(bottom, text="Reload from JSON", command=self.reload).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
        ttk.Button(bottom, text="Save now",         command=self.save).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)

        self.status = tk.StringVar(value="Ready")
        ttk.Label(parent, textvariable=self.status, foreground="#1565C0").pack(anchor="w", **pad)

    # ----- redraw -----

    def redraw(self):
        render_to_ax(self.ax, self.spec, selected_id=self.selected_id)
        self.canvas.draw_idle()
        self._refresh_edge_list()
        self._refresh_selection_panel()

    def _refresh_edge_list(self):
        self.edge_list.delete(0, tk.END)
        for e in self.spec.get("edges", []):
            label = f' [{e["label"]}]' if e.get("label") else ""
            style = " (dashed)" if e.get("style") == "dashed" else ""
            self.edge_list.insert(tk.END, f'{e["from"]} -> {e["to"]}{label}{style}')

    def _refresh_selection_panel(self):
        if not self.selected_id:
            for v in (self.var_id, self.var_text, self.var_type, self.var_x, self.var_y):
                v.set("")
            return
        node = self._node(self.selected_id)
        if node is None:
            return
        self.var_id.set(node["id"])
        self.var_text.set(node["text"])
        self.var_type.set(node["type"])
        self.var_x.set(f'{node["x"]:.3f}')
        self.var_y.set(f'{node["y"]:.3f}')

    # ----- helpers -----

    def _node(self, nid: str) -> dict | None:
        for n in self.spec["nodes"]:
            if n["id"] == nid:
                return n
        return None

    def _hit_test(self, x: float, y: float) -> str | None:
        size = self.spec.get("node_size", {"w": 2.6, "h": 0.7})
        w, h = size["w"] / 2, size["h"] / 2
        for n in reversed(self.spec["nodes"]):
            if abs(n["x"] - x) <= w + 0.1 and abs(n["y"] - y) <= h + 0.1:
                return n["id"]
        return None

    # ----- mouse handlers -----

    def _on_press(self, event):
        if event.inaxes != self.ax or event.xdata is None:
            return
        nid = self._hit_test(event.xdata, event.ydata)
        self.selected_id = nid
        if nid:
            node = self._node(nid)
            self.drag_offset = (node["x"] - event.xdata, node["y"] - event.ydata)
        else:
            self.drag_offset = None
        self.redraw()

    def _on_motion(self, event):
        if (self.drag_offset is None or self.selected_id is None
                or event.inaxes != self.ax or event.xdata is None):
            return
        node = self._node(self.selected_id)
        node["x"] = event.xdata + self.drag_offset[0]
        node["y"] = event.ydata + self.drag_offset[1]
        render_to_ax(self.ax, self.spec, selected_id=self.selected_id)
        self.canvas.draw_idle()
        self.var_x.set(f'{node["x"]:.3f}')
        self.var_y.set(f'{node["y"]:.3f}')

    def _on_release(self, event):
        if self.drag_offset is not None:
            self.drag_offset = None
            self.save()

    # ----- actions -----

    def apply_node_edits(self):
        if not self.selected_id:
            return
        node = self._node(self.selected_id)
        if node is None:
            return
        try:
            node["text"] = self.var_text.get()
            node["type"] = self.var_type.get()
            node["x"] = float(self.var_x.get())
            node["y"] = float(self.var_y.get())
        except ValueError as e:
            messagebox.showerror("Invalid value", str(e))
            return
        self.save()
        self.redraw()

    def add_node(self):
        nid = simpledialog.askstring("Add node", "Node id (unique):", parent=self.root)
        if not nid:
            return
        if self._node(nid):
            messagebox.showerror("Duplicate", f"A node with id '{nid}' already exists.")
            return
        text = simpledialog.askstring("Add node", "Text:", parent=self.root) or nid
        ntype = simpledialog.askstring("Add node", "Type (e.g. action, decision):",
                                       parent=self.root) or "action"
        canvas = self.spec.get("canvas", {})
        cx = (canvas.get("xlim", [0, 10])[0] + canvas.get("xlim", [0, 10])[1]) / 2
        cy = (canvas.get("ylim", [0, 10])[0] + canvas.get("ylim", [0, 10])[1]) / 2
        self.spec["nodes"].append({"id": nid, "x": cx, "y": cy, "text": text, "type": ntype})
        self.selected_id = nid
        self.save()
        self.redraw()

    def delete_node(self):
        if not self.selected_id:
            messagebox.showinfo("Delete node", "Select a node first.")
            return
        nid = self.selected_id
        if not messagebox.askyesno("Delete node",
                                   f"Delete node '{nid}' and all its edges?"):
            return
        self.spec["nodes"] = [n for n in self.spec["nodes"] if n["id"] != nid]
        self.spec["edges"] = [e for e in self.spec.get("edges", [])
                              if e["from"] != nid and e["to"] != nid]
        self.selected_id = None
        self.save()
        self.redraw()

    def add_edge(self):
        src = simpledialog.askstring("Add edge", "From node id:", parent=self.root)
        if not src or not self._node(src):
            messagebox.showerror("Add edge", f"No such node: {src}")
            return
        dst = simpledialog.askstring("Add edge", "To node id:", parent=self.root)
        if not dst or not self._node(dst):
            messagebox.showerror("Add edge", f"No such node: {dst}")
            return
        label = simpledialog.askstring("Add edge", "Label (optional):", parent=self.root) or ""
        edge = {"from": src, "to": dst}
        if label:
            edge["label"] = label
        self.spec.setdefault("edges", []).append(edge)
        self.save()
        self.redraw()

    def delete_edge(self):
        sel = self.edge_list.curselection()
        if not sel:
            messagebox.showinfo("Delete edge", "Select an edge from the list first.")
            return
        idx = sel[0]
        del self.spec["edges"][idx]
        self.save()
        self.redraw()

    def reload(self):
        self.spec = self._load()
        self.selected_id = None
        self.status.set("Reloaded from disk")
        self.redraw()


# ---------- entry point ----------

def main():
    argv = sys.argv[1:]
    here = Path(__file__).resolve().parent

    if argv and argv[0] == "--render":
        spec = Path(argv[1]) if len(argv) > 1 else here / "activity_diagram.json"
        out = Path(argv[2]) if len(argv) > 2 else here / "activity_diagram.png"
        render_png(spec, out)
        print(f"Wrote {out}")
        return

    spec_path = Path(argv[0]) if argv else here / "activity_diagram.json"
    if not spec_path.exists():
        print(f"Spec not found: {spec_path}", file=sys.stderr)
        sys.exit(1)

    try:
        _load_tk()
    except ImportError as e:
        print(f"Tkinter is not available for this Python: {e}\n"
              f"Install it with: sudo apt install python3-tk\n"
              f"Or use --render to generate the PNG without the UI.",
              file=sys.stderr)
        sys.exit(1)

    root = tk.Tk()
    DiagramApp(root, spec_path)
    root.mainloop()


if __name__ == "__main__":
    main()
