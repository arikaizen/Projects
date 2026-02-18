#!/usr/bin/env python3
"""
Interactive Mind Map Application
A local Python application for creating and managing mind maps with persistence.
"""

import json
import os
import tkinter as tk
from tkinter import messagebox, simpledialog, filedialog, colorchooser
from dataclasses import dataclass, field, asdict
from typing import Optional
import uuid


# ============================================================================
# Data Models
# ============================================================================

@dataclass
class Node:
    """Represents a node in the mind map."""
    id: str
    text: str
    x: float
    y: float
    color: str = "#4A90D9"
    text_color: str = "#FFFFFF"
    width: int = 120
    height: int = 50
    parent_id: Optional[str] = None

    def to_dict(self) -> dict:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict) -> 'Node':
        return cls(**data)


@dataclass
class MindMap:
    """Represents the entire mind map."""
    name: str = "Untitled Mind Map"
    nodes: dict = field(default_factory=dict)
    connections: list = field(default_factory=list)

    def add_node(self, node: Node) -> None:
        self.nodes[node.id] = node

    def remove_node(self, node_id: str) -> None:
        if node_id in self.nodes:
            del self.nodes[node_id]
            # Remove connections involving this node
            self.connections = [
                conn for conn in self.connections
                if conn[0] != node_id and conn[1] != node_id
            ]

    def add_connection(self, from_id: str, to_id: str) -> None:
        if [from_id, to_id] not in self.connections and [to_id, from_id] not in self.connections:
            self.connections.append([from_id, to_id])

    def remove_connection(self, from_id: str, to_id: str) -> None:
        if [from_id, to_id] in self.connections:
            self.connections.remove([from_id, to_id])
        elif [to_id, from_id] in self.connections:
            self.connections.remove([to_id, from_id])

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "nodes": {nid: node.to_dict() for nid, node in self.nodes.items()},
            "connections": self.connections
        }

    @classmethod
    def from_dict(cls, data: dict) -> 'MindMap':
        mind_map = cls(name=data.get("name", "Untitled Mind Map"))
        for nid, node_data in data.get("nodes", {}).items():
            mind_map.nodes[nid] = Node.from_dict(node_data)
        mind_map.connections = data.get("connections", [])
        return mind_map


# ============================================================================
# Persistence Layer
# ============================================================================

class MindMapStorage:
    """Handles saving and loading mind maps."""

    DEFAULT_DIR = os.path.expanduser("~/.mindmaps")

    def __init__(self):
        os.makedirs(self.DEFAULT_DIR, exist_ok=True)

    def save(self, mind_map: MindMap, filepath: Optional[str] = None) -> str:
        if filepath is None:
            filepath = os.path.join(self.DEFAULT_DIR, f"{mind_map.name}.json")

        with open(filepath, 'w') as f:
            json.dump(mind_map.to_dict(), f, indent=2)
        return filepath

    def load(self, filepath: str) -> MindMap:
        with open(filepath, 'r') as f:
            data = json.load(f)
        return MindMap.from_dict(data)

    def list_saved_maps(self) -> list:
        files = []
        for f in os.listdir(self.DEFAULT_DIR):
            if f.endswith('.json'):
                files.append(os.path.join(self.DEFAULT_DIR, f))
        return files

    def get_autosave_path(self) -> str:
        return os.path.join(self.DEFAULT_DIR, "_autosave.json")


# ============================================================================
# GUI Application
# ============================================================================

class MindMapApp:
    """Main application class for the Mind Map GUI."""

    # Color palette for nodes
    COLORS = [
        "#4A90D9",  # Blue
        "#7B68EE",  # Purple
        "#50C878",  # Green
        "#FF6B6B",  # Red
        "#FFB347",  # Orange
        "#77DD77",  # Light Green
        "#DDA0DD",  # Plum
        "#87CEEB",  # Sky Blue
        "#F0E68C",  # Khaki
        "#E6E6FA",  # Lavender
    ]

    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Mind Map")
        self.root.geometry("1200x800")

        # Data
        self.mind_map = MindMap()
        self.storage = MindMapStorage()
        self.current_file = None

        # Canvas state
        self.selected_node_id: Optional[str] = None
        self.drag_data = {"x": 0, "y": 0, "item": None}
        self.connecting_mode = False
        self.connection_start_id: Optional[str] = None
        self.canvas_offset = {"x": 0, "y": 0}
        self.zoom_level = 1.0

        # Node visual elements tracking
        self.node_items = {}  # node_id -> {"rect": canvas_id, "text": canvas_id}
        self.connection_items = {}  # (from_id, to_id) -> canvas_id

        self._setup_ui()
        self._setup_bindings()
        self._load_autosave()

    def _setup_ui(self):
        """Set up the user interface."""
        # Menu bar
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)

        # File menu
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="New", command=self._new_map, accelerator="Ctrl+N")
        file_menu.add_command(label="Open...", command=self._open_map, accelerator="Ctrl+O")
        file_menu.add_command(label="Save", command=self._save_map, accelerator="Ctrl+S")
        file_menu.add_command(label="Save As...", command=self._save_map_as)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self._on_close)

        # Edit menu
        edit_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Edit", menu=edit_menu)
        edit_menu.add_command(label="Delete Selected", command=self._delete_selected, accelerator="Delete")
        edit_menu.add_command(label="Edit Selected", command=self._edit_selected, accelerator="F2")
        edit_menu.add_command(label="Change Color", command=self._change_color)

        # View menu
        view_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="View", menu=view_menu)
        view_menu.add_command(label="Zoom In", command=lambda: self._zoom(1.2), accelerator="Ctrl++")
        view_menu.add_command(label="Zoom Out", command=lambda: self._zoom(0.8), accelerator="Ctrl+-")
        view_menu.add_command(label="Reset Zoom", command=self._reset_zoom, accelerator="Ctrl+0")
        view_menu.add_command(label="Center View", command=self._center_view)

        # Toolbar
        toolbar = tk.Frame(self.root, bd=1, relief=tk.RAISED)
        toolbar.pack(side=tk.TOP, fill=tk.X)

        tk.Button(toolbar, text="➕ Add Node", command=self._add_node_dialog).pack(side=tk.LEFT, padx=2, pady=2)
        tk.Button(toolbar, text="🔗 Connect Mode", command=self._toggle_connect_mode).pack(side=tk.LEFT, padx=2, pady=2)
        tk.Button(toolbar, text="🗑️ Delete", command=self._delete_selected).pack(side=tk.LEFT, padx=2, pady=2)
        tk.Button(toolbar, text="🎨 Color", command=self._change_color).pack(side=tk.LEFT, padx=2, pady=2)

        # Status indicator for connect mode
        self.connect_label = tk.Label(toolbar, text="", fg="red")
        self.connect_label.pack(side=tk.LEFT, padx=10)

        # Zoom controls
        tk.Label(toolbar, text="Zoom:").pack(side=tk.RIGHT, padx=2)
        tk.Button(toolbar, text="-", command=lambda: self._zoom(0.8), width=2).pack(side=tk.RIGHT, padx=1)
        self.zoom_label = tk.Label(toolbar, text="100%", width=5)
        self.zoom_label.pack(side=tk.RIGHT, padx=1)
        tk.Button(toolbar, text="+", command=lambda: self._zoom(1.2), width=2).pack(side=tk.RIGHT, padx=1)

        # Canvas
        canvas_frame = tk.Frame(self.root)
        canvas_frame.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(
            canvas_frame,
            bg="#2E3440",
            highlightthickness=0,
            scrollregion=(-2000, -2000, 2000, 2000)
        )
        self.canvas.pack(fill=tk.BOTH, expand=True)

        # Status bar
        self.status_bar = tk.Label(self.root, text="Ready. Double-click to add a node.", bd=1, relief=tk.SUNKEN, anchor=tk.W)
        self.status_bar.pack(side=tk.BOTTOM, fill=tk.X)

        # Help text on canvas
        self._draw_help_text()

    def _draw_help_text(self):
        """Draw help text on empty canvas."""
        self.help_text = self.canvas.create_text(
            600, 400,
            text="🧠 Mind Map\n\n"
                 "• Double-click anywhere to add a node\n"
                 "• Click a node to select it\n"
                 "• Drag nodes to move them\n"
                 "• Use 'Connect Mode' to link nodes\n"
                 "• Right-click a node for options\n"
                 "• Press Delete to remove selected node\n"
                 "• Press F2 to edit selected node",
            fill="#5E6779",
            font=("Arial", 14),
            justify=tk.CENTER
        )

    def _setup_bindings(self):
        """Set up event bindings."""
        # Canvas bindings
        self.canvas.bind("<Double-Button-1>", self._on_double_click)
        self.canvas.bind("<Button-1>", self._on_click)
        self.canvas.bind("<B1-Motion>", self._on_drag)
        self.canvas.bind("<ButtonRelease-1>", self._on_release)
        self.canvas.bind("<Button-3>", self._on_right_click)

        # Pan with middle mouse or Ctrl+drag
        self.canvas.bind("<Button-2>", self._start_pan)
        self.canvas.bind("<B2-Motion>", self._do_pan)
        self.canvas.bind("<Control-Button-1>", self._start_pan)
        self.canvas.bind("<Control-B1-Motion>", self._do_pan)

        # Keyboard bindings
        self.root.bind("<Delete>", lambda e: self._delete_selected())
        self.root.bind("<F2>", lambda e: self._edit_selected())
        self.root.bind("<Control-n>", lambda e: self._new_map())
        self.root.bind("<Control-o>", lambda e: self._open_map())
        self.root.bind("<Control-s>", lambda e: self._save_map())
        self.root.bind("<Control-plus>", lambda e: self._zoom(1.2))
        self.root.bind("<Control-minus>", lambda e: self._zoom(0.8))
        self.root.bind("<Control-0>", lambda e: self._reset_zoom())
        self.root.bind("<Escape>", lambda e: self._cancel_connect_mode())

        # Mouse wheel zoom
        self.canvas.bind("<MouseWheel>", self._on_mousewheel)
        self.canvas.bind("<Button-4>", lambda e: self._zoom(1.1))  # Linux scroll up
        self.canvas.bind("<Button-5>", lambda e: self._zoom(0.9))  # Linux scroll down

        # Window close
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ========================================================================
    # Node Management
    # ========================================================================

    def _add_node(self, x: float, y: float, text: str = "New Idea", color: str = None) -> Node:
        """Add a new node to the mind map."""
        if color is None:
            color = self.COLORS[len(self.mind_map.nodes) % len(self.COLORS)]

        node = Node(
            id=str(uuid.uuid4()),
            text=text,
            x=x,
            y=y,
            color=color
        )
        self.mind_map.add_node(node)
        self._draw_node(node)
        self._hide_help_text()
        self._autosave()
        return node

    def _draw_node(self, node: Node):
        """Draw a node on the canvas."""
        x, y = node.x, node.y
        w, h = node.width, node.height

        # Draw rounded rectangle (simulated with oval corners)
        rect = self.canvas.create_rectangle(
            x - w/2, y - h/2, x + w/2, y + h/2,
            fill=node.color,
            outline="#FFFFFF" if self.selected_node_id == node.id else "",
            width=3 if self.selected_node_id == node.id else 0,
            tags=("node", f"node_{node.id}")
        )

        # Draw text
        text = self.canvas.create_text(
            x, y,
            text=node.text,
            fill=node.text_color,
            font=("Arial", 11, "bold"),
            width=w - 10,
            tags=("node", f"node_{node.id}")
        )

        self.node_items[node.id] = {"rect": rect, "text": text}

    def _redraw_node(self, node: Node):
        """Redraw a specific node."""
        if node.id in self.node_items:
            self.canvas.delete(f"node_{node.id}")
        self._draw_node(node)

    def _redraw_all(self):
        """Redraw all nodes and connections."""
        self.canvas.delete("node")
        self.canvas.delete("connection")
        self.node_items.clear()
        self.connection_items.clear()

        # Draw connections first (behind nodes)
        for conn in self.mind_map.connections:
            self._draw_connection(conn[0], conn[1])

        # Draw nodes
        for node in self.mind_map.nodes.values():
            self._draw_node(node)

        if self.mind_map.nodes:
            self._hide_help_text()
        else:
            self._show_help_text()

    def _hide_help_text(self):
        """Hide the help text."""
        if hasattr(self, 'help_text'):
            self.canvas.itemconfig(self.help_text, state='hidden')

    def _show_help_text(self):
        """Show the help text."""
        if hasattr(self, 'help_text'):
            self.canvas.itemconfig(self.help_text, state='normal')

    def _get_node_at(self, x: float, y: float) -> Optional[str]:
        """Get the node ID at the given coordinates."""
        items = self.canvas.find_overlapping(x-1, y-1, x+1, y+1)
        for item in items:
            tags = self.canvas.gettags(item)
            for tag in tags:
                if tag.startswith("node_"):
                    return tag[5:]  # Remove "node_" prefix
        return None

    def _select_node(self, node_id: Optional[str]):
        """Select a node."""
        old_selected = self.selected_node_id
        self.selected_node_id = node_id

        # Redraw old selected node (remove highlight)
        if old_selected and old_selected in self.mind_map.nodes:
            self._redraw_node(self.mind_map.nodes[old_selected])

        # Redraw new selected node (add highlight)
        if node_id and node_id in self.mind_map.nodes:
            self._redraw_node(self.mind_map.nodes[node_id])
            self.status_bar.config(text=f"Selected: {self.mind_map.nodes[node_id].text}")
        else:
            self.status_bar.config(text="Ready. Double-click to add a node.")

    # ========================================================================
    # Connection Management
    # ========================================================================

    def _draw_connection(self, from_id: str, to_id: str):
        """Draw a connection line between two nodes."""
        if from_id not in self.mind_map.nodes or to_id not in self.mind_map.nodes:
            return

        from_node = self.mind_map.nodes[from_id]
        to_node = self.mind_map.nodes[to_id]

        line = self.canvas.create_line(
            from_node.x, from_node.y,
            to_node.x, to_node.y,
            fill="#88C0D0",
            width=2,
            smooth=True,
            tags=("connection", f"conn_{from_id}_{to_id}")
        )
        self.canvas.tag_lower(line)  # Put behind nodes
        self.connection_items[(from_id, to_id)] = line

    def _update_connections(self, node_id: str):
        """Update all connections involving a node."""
        for conn in self.mind_map.connections:
            if conn[0] == node_id or conn[1] == node_id:
                key = (conn[0], conn[1])
                if key in self.connection_items:
                    self.canvas.delete(self.connection_items[key])
                self._draw_connection(conn[0], conn[1])

    def _toggle_connect_mode(self):
        """Toggle connection mode."""
        if self.connecting_mode:
            self._cancel_connect_mode()
        else:
            self.connecting_mode = True
            self.connection_start_id = None
            self.connect_label.config(text="🔗 Connect Mode: Click first node")
            self.status_bar.config(text="Connect Mode: Click the first node to connect")
            self.canvas.config(cursor="cross")

    def _cancel_connect_mode(self):
        """Cancel connection mode."""
        self.connecting_mode = False
        self.connection_start_id = None
        self.connect_label.config(text="")
        self.status_bar.config(text="Ready. Double-click to add a node.")
        self.canvas.config(cursor="")

    # ========================================================================
    # Event Handlers
    # ========================================================================

    def _on_double_click(self, event):
        """Handle double-click to add a new node."""
        x, y = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
        node_id = self._get_node_at(x, y)

        if node_id:
            # Double-click on existing node: edit it
            self._edit_node(node_id)
        else:
            # Double-click on empty space: add new node
            self._add_node_at(x, y)

    def _add_node_at(self, x: float, y: float):
        """Add a node at specific coordinates."""
        text = simpledialog.askstring("New Node", "Enter node text:", initialvalue="New Idea")
        if text:
            self._add_node(x, y, text)

    def _add_node_dialog(self):
        """Add a node via dialog."""
        text = simpledialog.askstring("New Node", "Enter node text:", initialvalue="New Idea")
        if text:
            # Add near center of visible area
            x = self.canvas.canvasx(self.canvas.winfo_width() / 2)
            y = self.canvas.canvasy(self.canvas.winfo_height() / 2)
            self._add_node(x, y, text)

    def _on_click(self, event):
        """Handle single click."""
        x, y = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
        node_id = self._get_node_at(x, y)

        if self.connecting_mode:
            if node_id:
                if self.connection_start_id is None:
                    self.connection_start_id = node_id
                    self.connect_label.config(text=f"🔗 Connecting from: {self.mind_map.nodes[node_id].text[:15]}...")
                    self.status_bar.config(text="Now click the second node to complete the connection")
                elif node_id != self.connection_start_id:
                    self.mind_map.add_connection(self.connection_start_id, node_id)
                    self._draw_connection(self.connection_start_id, node_id)
                    self._autosave()
                    self._cancel_connect_mode()
                    self.status_bar.config(text="Connection created!")
        else:
            self._select_node(node_id)
            if node_id:
                self.drag_data["x"] = x
                self.drag_data["y"] = y
                self.drag_data["item"] = node_id

    def _on_drag(self, event):
        """Handle drag to move nodes."""
        if self.drag_data["item"] and not self.connecting_mode:
            x, y = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
            dx = x - self.drag_data["x"]
            dy = y - self.drag_data["y"]

            node_id = self.drag_data["item"]
            if node_id in self.mind_map.nodes:
                node = self.mind_map.nodes[node_id]
                node.x += dx
                node.y += dy

                # Move canvas items
                self.canvas.move(f"node_{node_id}", dx, dy)
                self._update_connections(node_id)

                self.drag_data["x"] = x
                self.drag_data["y"] = y

    def _on_release(self, event):
        """Handle mouse release."""
        if self.drag_data["item"]:
            self._autosave()
        self.drag_data["item"] = None

    def _on_right_click(self, event):
        """Handle right-click for context menu."""
        x, y = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
        node_id = self._get_node_at(x, y)

        if node_id:
            self._select_node(node_id)

            menu = tk.Menu(self.root, tearoff=0)
            menu.add_command(label="Edit", command=lambda: self._edit_node(node_id))
            menu.add_command(label="Change Color", command=self._change_color)
            menu.add_separator()
            menu.add_command(label="Connect To...", command=self._start_connection_from_selected)
            menu.add_separator()
            menu.add_command(label="Delete", command=self._delete_selected)
            menu.tk_popup(event.x_root, event.y_root)

    def _start_pan(self, event):
        """Start panning the canvas."""
        self.canvas.scan_mark(event.x, event.y)

    def _do_pan(self, event):
        """Pan the canvas."""
        self.canvas.scan_dragto(event.x, event.y, gain=1)

    def _on_mousewheel(self, event):
        """Handle mouse wheel for zooming."""
        if event.delta > 0:
            self._zoom(1.1)
        else:
            self._zoom(0.9)

    # ========================================================================
    # Node Operations
    # ========================================================================

    def _edit_selected(self):
        """Edit the selected node."""
        if self.selected_node_id:
            self._edit_node(self.selected_node_id)

    def _edit_node(self, node_id: str):
        """Edit a node's text."""
        if node_id in self.mind_map.nodes:
            node = self.mind_map.nodes[node_id]
            new_text = simpledialog.askstring("Edit Node", "Enter new text:", initialvalue=node.text)
            if new_text:
                node.text = new_text
                self._redraw_node(node)
                self._autosave()

    def _delete_selected(self):
        """Delete the selected node."""
        if self.selected_node_id:
            node_id = self.selected_node_id
            self.mind_map.remove_node(node_id)
            self.canvas.delete(f"node_{node_id}")

            # Remove connections visually
            keys_to_remove = [k for k in self.connection_items if node_id in k]
            for key in keys_to_remove:
                self.canvas.delete(self.connection_items[key])
                del self.connection_items[key]

            if node_id in self.node_items:
                del self.node_items[node_id]

            self.selected_node_id = None
            self.status_bar.config(text="Node deleted")
            self._autosave()

            if not self.mind_map.nodes:
                self._show_help_text()

    def _change_color(self):
        """Change the color of the selected node."""
        if self.selected_node_id and self.selected_node_id in self.mind_map.nodes:
            node = self.mind_map.nodes[self.selected_node_id]
            color = colorchooser.askcolor(color=node.color, title="Choose Node Color")
            if color[1]:
                node.color = color[1]
                # Determine text color based on background brightness
                r, g, b = int(color[1][1:3], 16), int(color[1][3:5], 16), int(color[1][5:7], 16)
                brightness = (r * 299 + g * 587 + b * 114) / 1000
                node.text_color = "#000000" if brightness > 128 else "#FFFFFF"
                self._redraw_node(node)
                self._autosave()

    def _start_connection_from_selected(self):
        """Start a connection from the selected node."""
        if self.selected_node_id:
            self.connecting_mode = True
            self.connection_start_id = self.selected_node_id
            node = self.mind_map.nodes[self.selected_node_id]
            self.connect_label.config(text=f"🔗 Connecting from: {node.text[:15]}...")
            self.status_bar.config(text="Click another node to complete the connection")
            self.canvas.config(cursor="cross")

    # ========================================================================
    # View Operations
    # ========================================================================

    def _zoom(self, factor: float):
        """Zoom the canvas."""
        self.zoom_level *= factor
        self.zoom_level = max(0.25, min(4.0, self.zoom_level))

        # Scale all items
        self.canvas.scale("all", 0, 0, factor, factor)

        # Update zoom label
        self.zoom_label.config(text=f"{int(self.zoom_level * 100)}%")

    def _reset_zoom(self):
        """Reset zoom to 100%."""
        factor = 1.0 / self.zoom_level
        self._zoom(factor)

    def _center_view(self):
        """Center the view on all nodes."""
        if not self.mind_map.nodes:
            return

        # Calculate bounding box of all nodes
        min_x = min(n.x for n in self.mind_map.nodes.values())
        max_x = max(n.x for n in self.mind_map.nodes.values())
        min_y = min(n.y for n in self.mind_map.nodes.values())
        max_y = max(n.y for n in self.mind_map.nodes.values())

        center_x = (min_x + max_x) / 2
        center_y = (min_y + max_y) / 2

        # Scroll to center
        self.canvas.xview_moveto((center_x + 2000 - self.canvas.winfo_width()/2) / 4000)
        self.canvas.yview_moveto((center_y + 2000 - self.canvas.winfo_height()/2) / 4000)

    # ========================================================================
    # File Operations
    # ========================================================================

    def _new_map(self):
        """Create a new mind map."""
        if self.mind_map.nodes:
            if not messagebox.askyesno("New Mind Map", "Discard current mind map?"):
                return

        name = simpledialog.askstring("New Mind Map", "Enter name for the mind map:", initialvalue="My Mind Map")
        if name:
            self.mind_map = MindMap(name=name)
            self.current_file = None
            self.root.title(f"Mind Map - {name}")
            self._redraw_all()
            self._show_help_text()

    def _save_map(self):
        """Save the current mind map."""
        if self.current_file:
            self.storage.save(self.mind_map, self.current_file)
            self.status_bar.config(text=f"Saved to {self.current_file}")
        else:
            self._save_map_as()

    def _save_map_as(self):
        """Save the mind map to a new file."""
        filepath = filedialog.asksaveasfilename(
            defaultextension=".json",
            filetypes=[("Mind Map files", "*.json"), ("All files", "*.*")],
            initialdir=self.storage.DEFAULT_DIR,
            initialfile=f"{self.mind_map.name}.json"
        )
        if filepath:
            self.current_file = filepath
            self.storage.save(self.mind_map, filepath)
            self.status_bar.config(text=f"Saved to {filepath}")

    def _open_map(self):
        """Open a mind map file."""
        filepath = filedialog.askopenfilename(
            filetypes=[("Mind Map files", "*.json"), ("All files", "*.*")],
            initialdir=self.storage.DEFAULT_DIR
        )
        if filepath:
            try:
                self.mind_map = self.storage.load(filepath)
                self.current_file = filepath
                self.root.title(f"Mind Map - {self.mind_map.name}")
                self._redraw_all()
                self.status_bar.config(text=f"Loaded {filepath}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to load file: {e}")

    def _autosave(self):
        """Automatically save to autosave file."""
        try:
            self.storage.save(self.mind_map, self.storage.get_autosave_path())
        except Exception:
            pass  # Silently fail autosave

    def _load_autosave(self):
        """Load autosave file if it exists."""
        autosave_path = self.storage.get_autosave_path()
        if os.path.exists(autosave_path):
            try:
                self.mind_map = self.storage.load(autosave_path)
                self._redraw_all()
                self.status_bar.config(text="Restored from autosave")
            except Exception:
                pass

    def _on_close(self):
        """Handle window close."""
        self._autosave()
        self.root.destroy()


# ============================================================================
# Main Entry Point
# ============================================================================

def main():
    root = tk.Tk()
    app = MindMapApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
