#!/usr/bin/env python3
"""Render the AI ConvoManager activity diagram to a PNG using matplotlib."""

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Polygon
from matplotlib.lines import Line2D


# (x, y, text, shape, color) where shape in {start, end, action, decision, signal}
NODES = {
    "start":     (5.0, 24.0, "Start: ./convo_manager",                "start",    "#4CAF50"),
    "parse":     (5.0, 22.5, "ParseCmdline(argc, argv)",              "action",   "#90CAF9"),
    "chk":       (5.0, 21.0, "ModelPaths empty?",                     "decision", "#FFD54F"),
    "errusage":  (1.5, 21.0, "Print usage to stderr",                 "action",   "#EF9A9A"),
    "exiterr":   (1.5, 19.6, "Exit code 1",                           "end",      "#E57373"),

    "signals":   (5.0, 19.5, "Install SIGINT / SIGTERM handlers",     "action",   "#90CAF9"),
    "loadloop":  (5.0, 18.0, "For each model path",                   "action",   "#90CAF9"),
    "addmodel":  (5.0, 16.7, "manager.AddModel(path, ctx)",           "action",   "#90CAF9"),
    "newconvo":  (5.0, 15.4, "NewConversation(sys_prompt, default)",  "action",   "#90CAF9"),
    "switch":    (5.0, 14.1, "SwitchActiveConversation",              "action",   "#90CAF9"),
    "more":      (5.0, 12.7, "More models?",                          "decision", "#FFD54F"),
    "banner":    (5.0, 11.2, "Print loaded count + active model",     "action",   "#90CAF9"),

    "readloop":  (5.0,  9.7, "g_should_exit?",                        "decision", "#FFD54F"),
    "saveall":   (9.5,  6.0, "manager.SaveAllNoThrow",                "action",   "#A5D6A7"),
    "exitok":    (9.5,  4.5, "Exit code 0",                           "end",      "#66BB6A"),

    "prompt":    (5.0,  8.3, "Print prompt; getline(stdin)",          "action",   "#90CAF9"),
    "eof":       (5.0,  6.9, "EOF?",                                  "decision", "#FFD54F"),
    "empty":     (5.0,  5.5, "Empty line?",                           "decision", "#FFD54F"),
    "iscmd":     (5.0,  4.1, "Starts with '/'?",                      "decision", "#FFD54F"),

    "chat":      (1.0,  2.7, "manager.Chat(model, line, 0.7, max)",   "action",   "#90CAF9"),
    "chatok":    (1.0,  1.4, "Exception?",                            "decision", "#FFD54F"),
    "reply":     (-1.5, 0.1, "Print reply",                           "action",   "#A5D6A7"),
    "perr":      (3.0,  0.1, "Print error",                           "action",   "#EF9A9A"),

    "split":     (8.5,  2.7, "SplitArgs(line)",                       "action",   "#90CAF9"),
    "disp":      (8.5,  1.4, "command?",                              "decision", "#FFD54F"),

    "help":      (12.5, 3.5, "/help -> PrintHelp",                    "action",   "#CE93D8"),
    "lism":      (12.5, 2.7, "/models -> ListModels",                 "action",   "#CE93D8"),
    "usem":      (12.5, 1.9, "/use-model -> set active",              "action",   "#CE93D8"),
    "newc":      (12.5, 1.1, "/new -> NewConv + Switch",              "action",   "#CE93D8"),
    "lisc":      (12.5, 0.3, "/list -> ListConversations",            "action",   "#CE93D8"),
    "usec":      (12.5,-0.5, "/use -> SwitchActive",                  "action",   "#CE93D8"),
    "savec":     (12.5,-1.3, "/save -> manager.Save",                 "action",   "#CE93D8"),
    "closec":    (12.5,-2.1, "/close -> manager.Close",               "action",   "#CE93D8"),
    "quitc":     (12.5,-2.9, "/quit -> SaveAll + exit",               "action",   "#FFAB91"),
    "unkn":      (12.5,-3.7, "unknown -> error msg",                  "action",   "#EF9A9A"),

    "sig":       (-2.5, 9.7, "SIGINT / SIGTERM",                      "signal",   "#FFCC80"),
    "sigh":      (-2.5, 8.3, "SignalHandler:\nset g_should_exit\nSaveAllNoThrow", "action", "#FFCC80"),
}


EDGES = [
    ("start", "parse", ""),
    ("parse", "chk", ""),
    ("chk", "errusage", "yes"),
    ("errusage", "exiterr", ""),
    ("chk", "signals", "no"),
    ("signals", "loadloop", ""),
    ("loadloop", "addmodel", ""),
    ("addmodel", "newconvo", ""),
    ("newconvo", "switch", ""),
    ("switch", "more", ""),
    ("more", "loadloop", "yes"),
    ("more", "banner", "no"),
    ("banner", "readloop", ""),
    ("readloop", "saveall", "true"),
    ("saveall", "exitok", ""),
    ("readloop", "prompt", "false"),
    ("prompt", "eof", ""),
    ("eof", "saveall", "yes"),
    ("eof", "empty", "no"),
    ("empty", "readloop", "yes"),
    ("empty", "iscmd", "no"),
    ("iscmd", "chat", "no"),
    ("chat", "chatok", ""),
    ("chatok", "reply", "no"),
    ("chatok", "perr", "yes"),
    ("reply", "readloop", ""),
    ("perr", "readloop", ""),
    ("iscmd", "split", "yes"),
    ("split", "disp", ""),
    ("disp", "help", ""),
    ("disp", "lism", ""),
    ("disp", "usem", ""),
    ("disp", "newc", ""),
    ("disp", "lisc", ""),
    ("disp", "usec", ""),
    ("disp", "savec", ""),
    ("disp", "closec", ""),
    ("disp", "quitc", ""),
    ("disp", "unkn", ""),
    ("help", "readloop", ""),
    ("lism", "readloop", ""),
    ("usem", "readloop", ""),
    ("newc", "readloop", ""),
    ("lisc", "readloop", ""),
    ("usec", "readloop", ""),
    ("savec", "readloop", ""),
    ("closec", "readloop", ""),
    ("quitc", "saveall", ""),
    ("unkn", "readloop", ""),
    ("sig", "sigh", ""),
    ("sigh", "readloop", ""),
]


def draw_node(ax, x, y, text, shape, color):
    w, h = 2.6, 0.7
    if shape == "decision":
        pts = [(x, y + h * 0.9), (x + w * 0.7, y), (x, y - h * 0.9), (x - w * 0.7, y)]
        ax.add_patch(Polygon(pts, closed=True, facecolor=color, edgecolor="black", linewidth=1.2))
    elif shape in ("start", "end"):
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="round,pad=0.05,rounding_size=0.35",
                                    facecolor=color, edgecolor="black", linewidth=1.4))
    elif shape == "signal":
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="sawtooth,pad=0.05",
                                    facecolor=color, edgecolor="black", linewidth=1.0,
                                    linestyle="--"))
    else:  # action
        ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                    boxstyle="round,pad=0.04,rounding_size=0.12",
                                    facecolor=color, edgecolor="black", linewidth=1.0))
    ax.text(x, y, text, ha="center", va="center", fontsize=7.2, wrap=True)


def draw_edge(ax, src, dst, label):
    x1, y1 = NODES[src][0], NODES[src][1]
    x2, y2 = NODES[dst][0], NODES[dst][1]
    arrow = FancyArrowPatch((x1, y1), (x2, y2),
                            arrowstyle="-|>", mutation_scale=10,
                            connectionstyle="arc3,rad=0.05",
                            color="#444", linewidth=0.9,
                            shrinkA=22, shrinkB=22)
    ax.add_patch(arrow)
    if label:
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        ax.text(mx, my, label, fontsize=6.5, color="#1565C0",
                bbox=dict(facecolor="white", edgecolor="none", pad=1))


def main():
    fig, ax = plt.subplots(figsize=(16, 18))
    ax.set_xlim(-5, 16)
    ax.set_ylim(-5, 26)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("AI ConvoManager — Activity Diagram", fontsize=14, fontweight="bold", pad=12)

    for src, dst, label in EDGES:
        draw_edge(ax, src, dst, label)
    for _, (x, y, text, shape, color) in NODES.items():
        draw_node(ax, x, y, text, shape, color)

    legend = [
        Line2D([0], [0], marker="o", color="w", markerfacecolor="#4CAF50", markersize=10, label="Start"),
        Line2D([0], [0], marker="o", color="w", markerfacecolor="#66BB6A", markersize=10, label="End"),
        Line2D([0], [0], marker="s", color="w", markerfacecolor="#90CAF9", markersize=10, label="Action"),
        Line2D([0], [0], marker="D", color="w", markerfacecolor="#FFD54F", markersize=10, label="Decision"),
        Line2D([0], [0], marker="s", color="w", markerfacecolor="#CE93D8", markersize=10, label="Slash command"),
        Line2D([0], [0], marker="s", color="w", markerfacecolor="#FFCC80", markersize=10, label="Signal"),
    ]
    ax.legend(handles=legend, loc="lower left", fontsize=8, frameon=True)

    out = "/home/user/Projects/AI/activity_diagram.png"
    plt.savefig(out, dpi=150, bbox_inches="tight", facecolor="white")
    print(f"Wrote {out}")


if __name__ == "__main__":
    main()
