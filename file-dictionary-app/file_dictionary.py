#!/usr/bin/env python3
"""
================================================================================
FILE DICTIONARY APPLICATION
================================================================================

A local Python desktop application for managing, searching, and viewing text
files with JSON template support and interactive autocomplete search.

FEATURES:
    - Browse and search files by filename or content keywords
    - Live autocomplete suggestions as you type
    - Scrollable file viewer with find-in-file highlighting
    - JSON template editor: load templates, fill in key-value pairs, save

--------------------------------------------------------------------------------
QUICK START
--------------------------------------------------------------------------------

    # Run with default settings (opens directory picker):
    python file_dictionary.py

    # Run with a specific directory:
    python file_dictionary.py /path/to/your/files

    # Run with a specific directory using flag:
    python file_dictionary.py --directory /path/to/your/files

    # Show help:
    python file_dictionary.py --help

--------------------------------------------------------------------------------
CONFIGURATION
--------------------------------------------------------------------------------

You can configure the application in THREE ways (in order of priority):

1. COMMAND-LINE ARGUMENTS (highest priority):
       python file_dictionary.py --directory /my/files

2. CONFIGURATION FILE (~/.file_dictionary_config.json):
       Create this JSON file in your home directory:
       {
           "default_directory": "/path/to/your/default/folder",
           "default_template": "my_template.json",
           "supported_extensions": [".txt", ".md", ".json", ".py"],
           "window_width": 1280,
           "window_height": 780,
           "template_autosave_enabled": true
       }

3. EDIT THE CONFIG CLASS BELOW (lowest priority):
       Scroll down to the 'Config' class and modify the default values.

--------------------------------------------------------------------------------
TEMPLATE EDITOR WORKFLOW
--------------------------------------------------------------------------------

The Template Editor lets you load a JSON template, fill in values, and save
to a NEW file (without modifying the original template).

WORKFLOW:
    1. Open a directory containing your files and template(s)
    2. Go to the "Template Editor" tab
    3. Select a template from the dropdown and click "Load Template"
    4. Edit the values for each key
    5. Click "Save as New File" to create a new file with your values
       (The original template remains unchanged!)

AUTO-SAVE:
    Your work-in-progress is automatically saved when you close the app.
    If you reopen the app, your unsaved edits will be restored.

CONFIGURE DEFAULT TEMPLATE:
    To auto-load a template when the app starts, set in your config:
        "default_template": "my_template.json"

--------------------------------------------------------------------------------
KEYBOARD SHORTCUTS
--------------------------------------------------------------------------------

    Ctrl+F     Focus the find-in-file search bar
    Enter      Execute search / go to next match
    Escape     Clear search / close autocomplete dropdown

--------------------------------------------------------------------------------
SUPPORTED FILE TYPES
--------------------------------------------------------------------------------

By default, the application indexes these file extensions:
    .txt, .md, .json, .csv, .py, .yaml, .yml, .html, .css, .js, .xml,
    .rst, .ini, .cfg, .toml, .sh, .bat, .log, .text, .sql

You can customize this list via the configuration file or by editing
the Config.SUPPORTED_EXTENSIONS set below.

================================================================================
"""

import argparse
import json
import os
import re
import sys
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from pathlib import Path
from typing import Optional, List, Dict, Set


# ═══════════════════════════════════════════════════════════════════════════════
# CONFIGURATION  –  Edit these values to customize the application
# ═══════════════════════════════════════════════════════════════════════════════

class Config:
    """
    Application configuration settings.

    INSTRUCTIONS:
        1. Edit the values below to change default behavior
        2. Or create ~/.file_dictionary_config.json to override at runtime
        3. Or use command-line arguments (highest priority)
    """

    # ┌─────────────────────────────────────────────────────────────────────────┐
    # │  DEFAULT DIRECTORY                                                       │
    # │  Set this to automatically open a folder when the app starts.            │
    # │  Leave as None to show the directory picker on startup.                  │
    # └─────────────────────────────────────────────────────────────────────────┘
    DEFAULT_DIRECTORY: Optional[str] = None
    # Example: DEFAULT_DIRECTORY = "/home/user/Documents/my-files"
    # Example: DEFAULT_DIRECTORY = "C:\\Users\\YourName\\Documents\\files"

    # ┌─────────────────────────────────────────────────────────────────────────┐
    # │  SUPPORTED FILE EXTENSIONS                                               │
    # │  Only files with these extensions will be indexed and displayed.         │
    # │  Add or remove extensions as needed.                                     │
    # └─────────────────────────────────────────────────────────────────────────┘
    SUPPORTED_EXTENSIONS: Set[str] = {
        '.txt',     # Plain text
        '.md',      # Markdown
        '.json',    # JSON (also used for templates)
        '.csv',     # Comma-separated values
        '.py',      # Python source
        '.yaml',    # YAML config
        '.yml',     # YAML config (alternate extension)
        '.html',    # HTML
        '.css',     # CSS stylesheets
        '.js',      # JavaScript
        '.xml',     # XML
        '.rst',     # reStructuredText
        '.ini',     # INI config files
        '.cfg',     # Config files
        '.toml',    # TOML config
        '.sh',      # Shell scripts
        '.bat',     # Batch scripts
        '.log',     # Log files
        '.text',    # Text files (alternate extension)
        '.sql',     # SQL scripts
    }

    # ┌─────────────────────────────────────────────────────────────────────────┐
    # │  WINDOW SETTINGS                                                         │
    # │  Customize the initial window size.                                      │
    # └─────────────────────────────────────────────────────────────────────────┘
    WINDOW_WIDTH: int = 1280
    WINDOW_HEIGHT: int = 780
    WINDOW_MIN_WIDTH: int = 900
    WINDOW_MIN_HEIGHT: int = 600

    # ┌─────────────────────────────────────────────────────────────────────────┐
    # │  SEARCH SETTINGS                                                         │
    # │  Tune how search and autocomplete behave.                                │
    # └─────────────────────────────────────────────────────────────────────────┘
    AUTOCOMPLETE_MAX_SUGGESTIONS: int = 15    # Max items in autocomplete dropdown
    MIN_WORD_LENGTH: int = 2                   # Min word length to index

    # ┌─────────────────────────────────────────────────────────────────────────┐
    # │  TEMPLATE SETTINGS                                                       │
    # │  Configure default template and auto-save behavior.                      │
    # └─────────────────────────────────────────────────────────────────────────┘
    DEFAULT_TEMPLATE: Optional[str] = None
    # Set this to the FILENAME of a template to auto-load when the app starts.
    # The file must exist in the working directory.
    # Example: DEFAULT_TEMPLATE = "my_template.json"

    TEMPLATE_AUTOSAVE_ENABLED: bool = True
    # When True, your work-in-progress edits are automatically saved
    # so you don't lose them if you close the app.

    TEMPLATE_AUTOSAVE_PATH: str = os.path.expanduser("~/.file_dictionary_autosave.json")
    # Location where work-in-progress template edits are stored.

    # ┌─────────────────────────────────────────────────────────────────────────┐
    # │  CONFIG FILE PATH                                                        │
    # │  Location of the optional JSON configuration file.                       │
    # └─────────────────────────────────────────────────────────────────────────┘
    CONFIG_FILE_PATH: str = os.path.expanduser("~/.file_dictionary_config.json")

    @classmethod
    def load_from_file(cls) -> None:
        """
        Load configuration from the JSON config file if it exists.
        This overrides the default values set above.
        """
        if os.path.exists(cls.CONFIG_FILE_PATH):
            try:
                with open(cls.CONFIG_FILE_PATH, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                if 'default_directory' in data:
                    cls.DEFAULT_DIRECTORY = data['default_directory']
                if 'supported_extensions' in data:
                    cls.SUPPORTED_EXTENSIONS = set(data['supported_extensions'])
                if 'window_width' in data:
                    cls.WINDOW_WIDTH = data['window_width']
                if 'window_height' in data:
                    cls.WINDOW_HEIGHT = data['window_height']
                if 'autocomplete_max_suggestions' in data:
                    cls.AUTOCOMPLETE_MAX_SUGGESTIONS = data['autocomplete_max_suggestions']
                if 'min_word_length' in data:
                    cls.MIN_WORD_LENGTH = data['min_word_length']
                if 'default_template' in data:
                    cls.DEFAULT_TEMPLATE = data['default_template']
                if 'template_autosave_enabled' in data:
                    cls.TEMPLATE_AUTOSAVE_ENABLED = data['template_autosave_enabled']

            except (json.JSONDecodeError, OSError) as e:
                print(f"Warning: Could not load config file: {e}", file=sys.stderr)

    @classmethod
    def create_sample_config_file(cls) -> str:
        """
        Generate a sample configuration file content.
        Users can save this to ~/.file_dictionary_config.json
        """
        sample = {
            "_comment": "File Dictionary configuration file",
            "default_directory": "/path/to/your/files",
            "default_template": "template.json",
            "supported_extensions": list(cls.SUPPORTED_EXTENSIONS),
            "window_width": cls.WINDOW_WIDTH,
            "window_height": cls.WINDOW_HEIGHT,
            "autocomplete_max_suggestions": cls.AUTOCOMPLETE_MAX_SUGGESTIONS,
            "min_word_length": cls.MIN_WORD_LENGTH,
            "template_autosave_enabled": cls.TEMPLATE_AUTOSAVE_ENABLED,
        }
        return json.dumps(sample, indent=2)


# ═══════════════════════════════════════════════════════════════════════════════
# FILE INDEX  –  Indexing and search engine
# ═══════════════════════════════════════════════════════════════════════════════

class FileIndex:
    """
    Scans a directory and builds a searchable index of text files.

    The index supports two types of search:
        - Filename search: matches against file names
        - Content search: matches words found inside files

    Attributes:
        directory (str): The currently indexed directory path
        files (List[str]): Sorted list of indexed filenames
        word_index (Dict[str, Set[str]]): Maps words to files containing them
        file_contents (Dict[str, str]): Maps filenames to their full text content
    """

    def __init__(self):
        self.directory: Optional[str] = None
        self.files: List[str] = []                          # sorted filenames
        self.word_index: Dict[str, Set[str]] = {}           # word → {filename, …}
        self.file_contents: Dict[str, str] = {}             # filename → raw text

    # ── directory management ──────────────────────────────────────────────

    def set_directory(self, directory: str) -> None:
        """
        Set the working directory and rebuild the index.

        Args:
            directory: Absolute path to the folder containing files to index
        """
        self.directory = directory
        self.rebuild()

    def rebuild(self) -> None:
        """
        Re-scan the current directory and rebuild all indexes.
        Call this after files are added, modified, or deleted externally.
        """
        self.files.clear()
        self.word_index.clear()
        self.file_contents.clear()
        if not self.directory or not os.path.isdir(self.directory):
            return
        entries = sorted(os.scandir(self.directory), key=lambda e: e.name.lower())
        for entry in entries:
            if entry.is_file() and Path(entry.name).suffix.lower() in Config.SUPPORTED_EXTENSIONS:
                self._index_file(entry.name, entry.path)

    def _index_file(self, name: str, path: str) -> None:
        """
        Read a file and add its content to the search indexes.

        Args:
            name: The filename (without path)
            path: The full path to the file
        """
        try:
            content = Path(path).read_text(encoding='utf-8', errors='replace')
        except OSError:
            return
        self.file_contents[name] = content
        if name not in self.files:
            self.files.append(name)
            self.files.sort(key=str.lower)
        # Index words of minimum length (configured in Config.MIN_WORD_LENGTH)
        pattern = rf'\b\w{{{Config.MIN_WORD_LENGTH},}}\b'
        for word in set(re.findall(pattern, content.lower())):
            self.word_index.setdefault(word, set()).add(name)

    # ── search ────────────────────────────────────────────────────────────

    def search_filenames(self, query: str) -> List[str]:
        q = query.lower()
        return [f for f in self.files if q in f.lower()]

    def search_content(self, query: str) -> List[str]:
        """Return files whose content contains the query word/phrase."""
        q = query.lower()
        # exact word match first
        if q in self.word_index:
            return sorted(self.word_index[q])
        # substring match among indexed words
        hits: Set[str] = set()
        for word, files in self.word_index.items():
            if q in word:
                hits.update(files)
        return sorted(hits)

    # ── autocomplete ──────────────────────────────────────────────────────

    def autocomplete(self, prefix: str, mode: str) -> List[str]:
        """
        Get autocomplete suggestions for the search box.

        Args:
            prefix: The text the user has typed so far
            mode: Either 'filename' or 'content'

        Returns:
            List of suggested completions (max Config.AUTOCOMPLETE_MAX_SUGGESTIONS)
        """
        if not prefix:
            return []
        p = prefix.lower()
        max_suggestions = Config.AUTOCOMPLETE_MAX_SUGGESTIONS
        if mode == 'filename':
            exact   = [f for f in self.files if f.lower().startswith(p)]
            partial = [f for f in self.files if p in f.lower() and f not in exact]
            return (exact + partial)[:max_suggestions]
        else:                                                # content / keyword
            words = sorted(w for w in self.word_index if w.startswith(p))
            return words[:max_suggestions + 5]  # slightly more for keyword mode

    # ── write back ────────────────────────────────────────────────────────

    def save_file(self, filename: str, content: str) -> None:
        if not self.directory:
            raise RuntimeError("No directory selected")
        path = os.path.join(self.directory, filename)
        Path(path).write_text(content, encoding='utf-8')
        self._index_file(filename, path)

    def reload_file(self, filename: str) -> None:
        if self.directory:
            self._index_file(filename, os.path.join(self.directory, filename))


# ─────────────────────────────────────────────────────────────────────────────
# AUTOCOMPLETE ENTRY  –  Entry widget with live suggestion dropdown
# ─────────────────────────────────────────────────────────────────────────────

class AutocompleteEntry(tk.Frame):
    """Entry + floating Listbox that shows autocomplete suggestions."""

    def __init__(self, parent, suggestion_callback, on_select=None, **entry_kwargs):
        super().__init__(parent, bg=parent.cget('bg'))
        self._suggest_fn = suggestion_callback
        self._on_select  = on_select
        self._popup: Optional[tk.Toplevel] = None
        self._listbox: Optional[tk.Listbox] = None

        self.var = tk.StringVar()
        self.var.trace_add('write', self._on_text_change)

        self.entry = tk.Entry(self, textvariable=self.var, **entry_kwargs)
        self.entry.pack(fill=tk.X, expand=True)

        self.entry.bind('<Down>',    self._focus_list)
        self.entry.bind('<Return>',  self._on_enter)
        self.entry.bind('<Escape>',  lambda _: self._hide())
        self.entry.bind('<FocusOut>', lambda _: self.after(150, self._hide))

    # ── public helpers ────────────────────────────────────────────────────

    def get(self) -> str:
        return self.var.get()

    def set(self, value: str) -> None:
        self.var.set(value)

    def clear(self) -> None:
        self.var.set('')

    def focus(self) -> None:
        self.entry.focus_set()

    # ── internal ──────────────────────────────────────────────────────────

    def _on_text_change(self, *_):
        text = self.var.get()
        suggestions = self._suggest_fn(text)
        if suggestions and text:
            self._show(suggestions)
        else:
            self._hide()

    def _show(self, suggestions: List[str]):
        self._hide()
        self._popup = tk.Toplevel(self)
        self._popup.wm_overrideredirect(True)
        self._popup.wm_attributes('-topmost', True)

        x = self.entry.winfo_rootx()
        y = self.entry.winfo_rooty() + self.entry.winfo_height()
        w = max(self.entry.winfo_width(), 200)
        h = min(len(suggestions) * 22 + 4, 220)
        self._popup.geometry(f"{w}x{h}+{x}+{y}")

        outer = tk.Frame(self._popup, relief=tk.SOLID, bd=1, bg='#3A3A3A')
        outer.pack(fill=tk.BOTH, expand=True)

        sb = tk.Scrollbar(outer, orient=tk.VERTICAL)
        self._listbox = tk.Listbox(
            outer,
            yscrollcommand=sb.set,
            selectmode=tk.SINGLE,
            activestyle='dotbox',
            bg='#2B2B2B', fg='#A9B7C6',
            selectbackground='#4A90D9',
            selectforeground='#FFFFFF',
            font=('Consolas', 10),
            relief=tk.FLAT, bd=0,
        )
        sb.config(command=self._listbox.yview)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        self._listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        for s in suggestions:
            self._listbox.insert(tk.END, f'  {s}')

        self._listbox.bind('<ButtonRelease-1>', self._pick)
        self._listbox.bind('<Double-Button-1>', self._pick)
        self._listbox.bind('<Return>',          self._pick)
        self._listbox.bind('<Escape>',          lambda _: self._hide())
        self._listbox.bind('<FocusOut>',        lambda _: self.after(150, self._hide))

    def _hide(self, *_):
        if self._popup:
            self._popup.destroy()
            self._popup = None

    def _pick(self, _=None):
        if self._popup and self._listbox and self._listbox.curselection():
            val = self._listbox.get(self._listbox.curselection()[0]).strip()
            self.var.set(val)
            self._hide()
            if self._on_select:
                self._on_select(val)

    def _focus_list(self, _):
        if self._popup and self._listbox:
            self._listbox.focus_set()
            if self._listbox.size():
                self._listbox.selection_set(0)

    def _on_enter(self, _):
        self._hide()
        if self._on_select:
            self._on_select(self.var.get())


# ─────────────────────────────────────────────────────────────────────────────
# CONTENT VIEWER  –  scrollable text display with find-in-file
# ─────────────────────────────────────────────────────────────────────────────

class ContentViewer(tk.Frame):
    """Displays a file's text content with highlight-able find bar."""

    _HL  = 'highlight'
    _CUR = 'current_match'

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._matches: List[tuple] = []
        self._cur = -1
        self._current_file = ''
        self._build()

    def _build(self):
        # ── header ──
        hdr = tk.Frame(self, bg='#313335', padx=8, pady=4)
        hdr.pack(fill=tk.X)

        self._file_lbl = tk.Label(
            hdr, text='No file selected',
            bg='#313335', fg='#A9B7C6',
            font=('Arial', 10, 'bold'), anchor='w',
        )
        self._file_lbl.pack(side=tk.LEFT)

        self._info_lbl = tk.Label(
            hdr, text='',
            bg='#313335', fg='#666',
            font=('Arial', 9), anchor='e',
        )
        self._info_lbl.pack(side=tk.RIGHT)

        # ── find bar ──
        findbar = tk.Frame(self, bg='#252526', padx=6, pady=4)
        findbar.pack(fill=tk.X)

        tk.Label(findbar, text='Find in file:', bg='#252526', fg='#888',
                 font=('Arial', 9)).pack(side=tk.LEFT)

        self._find_var = tk.StringVar()
        self._find_entry = tk.Entry(
            findbar, textvariable=self._find_var, width=28,
            bg='#3C3F41', fg='#A9B7C6', insertbackground='#A9B7C6',
            relief=tk.FLAT, font=('Consolas', 10),
        )
        self._find_entry.pack(side=tk.LEFT, padx=6)
        self._find_entry.bind('<Return>',   lambda _: self._next())
        self._find_entry.bind('<KeyRelease>', lambda _: self._run_find())

        _btn = dict(bg='#4A90D9', fg='white', relief=tk.FLAT,
                    padx=6, pady=1, font=('Arial', 9), cursor='hand2')
        tk.Button(findbar, text='▲ Prev', command=self._prev, **_btn).pack(side=tk.LEFT, padx=2)
        tk.Button(findbar, text='▼ Next', command=self._next, **_btn).pack(side=tk.LEFT, padx=2)

        self._match_lbl = tk.Label(findbar, text='', bg='#252526', fg='#A9B7C6',
                                   font=('Arial', 9), width=10)
        self._match_lbl.pack(side=tk.LEFT, padx=6)

        tk.Button(findbar, text='✕ Clear', command=self._clear_find,
                  bg='#444', fg='#A9B7C6', relief=tk.FLAT,
                  padx=6, pady=1, font=('Arial', 9), cursor='hand2',
                  ).pack(side=tk.RIGHT, padx=4)

        # ── text area with both scrollbars ──
        txt_frame = tk.Frame(self, bg='#1E1E1E')
        txt_frame.pack(fill=tk.BOTH, expand=True)

        vbar = tk.Scrollbar(txt_frame, orient=tk.VERTICAL)
        hbar = tk.Scrollbar(txt_frame, orient=tk.HORIZONTAL)

        self.text = tk.Text(
            txt_frame,
            wrap=tk.NONE,
            bg='#1E1E1E', fg='#D4D4D4',
            font=('Consolas', 11),
            insertbackground='#D4D4D4',
            selectbackground='#264F78',
            relief=tk.FLAT,
            padx=12, pady=8,
            yscrollcommand=vbar.set,
            xscrollcommand=hbar.set,
            state=tk.DISABLED,
        )
        vbar.config(command=self.text.yview)
        hbar.config(command=self.text.xview)

        vbar.pack(side=tk.RIGHT,  fill=tk.Y)
        hbar.pack(side=tk.BOTTOM, fill=tk.X)
        self.text.pack(fill=tk.BOTH, expand=True)

        self.text.tag_configure(self._HL,  background='#4B4B00', foreground='#FFE066')
        self.text.tag_configure(self._CUR, background='#FF8C00', foreground='#FFFFFF')

    # ── public API ────────────────────────────────────────────────────────

    def load(self, filename: str, content: str) -> None:
        self._current_file = filename
        self._file_lbl.config(text=filename)
        lines = content.count('\n') + 1
        chars = len(content)
        self._info_lbl.config(text=f'{lines} lines  {chars} chars')

        self._matches.clear()
        self._cur = -1

        self.text.config(state=tk.NORMAL)
        self.text.delete('1.0', tk.END)
        self.text.insert('1.0', content)
        self.text.config(state=tk.DISABLED)
        self._match_lbl.config(text='')

        # re-run active search
        if self._find_var.get():
            self._run_find()

    def highlight_term(self, term: str) -> None:
        """Pre-populate the find bar with a search term."""
        self._find_var.set(term)
        self._find_entry.delete(0, tk.END)
        self._find_entry.insert(0, term)
        self._run_find()

    # ── find mechanics ────────────────────────────────────────────────────

    def _run_find(self):
        query = self._find_var.get()
        self.text.tag_remove(self._HL,  '1.0', tk.END)
        self.text.tag_remove(self._CUR, '1.0', tk.END)
        self._matches.clear()
        self._cur = -1

        if not query:
            self._match_lbl.config(text='')
            return

        content = self.text.get('1.0', tk.END)
        for m in re.finditer(re.escape(query), content, re.IGNORECASE):
            s = f'1.0 + {m.start()} chars'
            e = f'1.0 + {m.end()} chars'
            self.text.tag_add(self._HL, s, e)
            self._matches.append((s, e))

        if self._matches:
            self._match_lbl.config(text=f'0/{len(self._matches)}')
            self._next()
        else:
            self._match_lbl.config(text='No matches', fg='#FF6B6B')

    def _next(self):
        if not self._matches:
            return
        self._deselect_current()
        self._cur = (self._cur + 1) % len(self._matches)
        self._select_current()

    def _prev(self):
        if not self._matches:
            return
        self._deselect_current()
        self._cur = (self._cur - 1) % len(self._matches)
        self._select_current()

    def _deselect_current(self):
        if self._cur >= 0:
            s, e = self._matches[self._cur]
            self.text.tag_remove(self._CUR, s, e)
            self.text.tag_add(self._HL, s, e)

    def _select_current(self):
        s, e = self._matches[self._cur]
        self.text.tag_remove(self._HL, s, e)
        self.text.tag_add(self._CUR, s, e)
        self.text.see(s)
        self._match_lbl.config(text=f'{self._cur+1}/{len(self._matches)}', fg='#A9B7C6')

    def _clear_find(self):
        self._find_var.set('')
        self.text.tag_remove(self._HL,  '1.0', tk.END)
        self.text.tag_remove(self._CUR, '1.0', tk.END)
        self._matches.clear()
        self._cur = -1
        self._match_lbl.config(text='')


# ─────────────────────────────────────────────────────────────────────────────
# TEMPLATE EDITOR  –  load JSON, edit key-value pairs, save
# ─────────────────────────────────────────────────────────────────────────────

class TemplateEditor(tk.Frame):
    """
    Load a JSON template and fill / extend its key-value pairs.

    Features:
        - Load templates from JSON files in the working directory
        - Edit key-value pairs with add/remove functionality
        - Save to new file (preserves original template)
        - Overwrite original template
        - Auto-save work-in-progress (persists your edits)

    The auto-save feature stores your current edits to a separate file
    (~/.file_dictionary_autosave.json) so you don't lose work if you
    close the app. This is restored automatically on startup.
    """

    def __init__(self, parent, index: FileIndex, on_save=None, **kw):
        super().__init__(parent, **kw)
        self._index   = index
        self._on_save = on_save
        self._rows: List[Dict] = []
        self._current_template: Optional[str] = None  # Track which template is loaded
        self._build()
        self._load_autosave()  # Restore work-in-progress on startup

    # ── UI ────────────────────────────────────────────────────────────────

    def _build(self):
        # Top controls
        ctrl = tk.Frame(self, bg='#313335', padx=8, pady=6)
        ctrl.pack(fill=tk.X)

        tk.Label(ctrl, text='Template (.json):', bg='#313335', fg='#A9B7C6',
                 font=('Arial', 10)).grid(row=0, column=0, sticky='w', padx=4)

        self._tmpl_var = tk.StringVar()
        self._tmpl_combo = ttk.Combobox(ctrl, textvariable=self._tmpl_var,
                                         width=32, state='readonly', font=('Arial', 10))
        self._tmpl_combo.grid(row=0, column=1, padx=6)

        _btn = dict(relief=tk.FLAT, padx=8, pady=3, font=('Arial', 9), cursor='hand2')
        tk.Button(ctrl, text='Load Template', command=self._load,
                  bg='#4A90D9', fg='white', **_btn).grid(row=0, column=2, padx=4)
        tk.Button(ctrl, text='⟳ Refresh List', command=self._refresh,
                  bg='#555', fg='white', **_btn).grid(row=0, column=3, padx=4)

        # Column headers
        hdr = tk.Frame(self, bg='#252526', padx=8, pady=4)
        hdr.pack(fill=tk.X)
        tk.Label(hdr, text='Key', width=26, anchor='w', bg='#252526', fg='#888',
                 font=('Arial', 9, 'bold')).pack(side=tk.LEFT)
        tk.Label(hdr, text='Value', anchor='w', bg='#252526', fg='#888',
                 font=('Arial', 9, 'bold')).pack(side=tk.LEFT, fill=tk.X, expand=True)

        # Scrollable rows canvas
        cf = tk.Frame(self, bg='#1E1E1E')
        cf.pack(fill=tk.BOTH, expand=True)

        self._canvas = tk.Canvas(cf, bg='#1E1E1E', highlightthickness=0)
        sb = tk.Scrollbar(cf, orient=tk.VERTICAL, command=self._canvas.yview)
        self._canvas.configure(yscrollcommand=sb.set)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        self._canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self._rows_frame = tk.Frame(self._canvas, bg='#1E1E1E')
        self._cwin = self._canvas.create_window((0, 0), window=self._rows_frame, anchor='nw')

        self._rows_frame.bind('<Configure>', lambda _: self._canvas.configure(
            scrollregion=self._canvas.bbox('all')))
        self._canvas.bind('<Configure>', lambda e: self._canvas.itemconfig(
            self._cwin, width=e.width))
        # Mouse-wheel scrolling
        for seq in ('<MouseWheel>', '<Button-4>', '<Button-5>'):
            self._canvas.bind(seq, self._on_scroll)

        # Bottom buttons
        bot = tk.Frame(self, bg='#313335', padx=8, pady=6)
        bot.pack(fill=tk.X, side=tk.BOTTOM)

        tk.Button(bot, text='+ Add Row', command=lambda: self._add_row(),
                  bg='#6AAE56', fg='white', relief=tk.FLAT, padx=8, pady=3,
                  font=('Arial', 9), cursor='hand2').pack(side=tk.LEFT, padx=4)
        tk.Button(bot, text='Clear All', command=self._clear,
                  bg='#7B3F3F', fg='white', relief=tk.FLAT, padx=8, pady=3,
                  font=('Arial', 9), cursor='hand2').pack(side=tk.LEFT, padx=4)

        tk.Button(bot, text='Overwrite Template', command=self._overwrite,
                  bg='#CC7700', fg='white', relief=tk.FLAT, padx=8, pady=3,
                  font=('Arial', 9), cursor='hand2').pack(side=tk.RIGHT, padx=4)
        tk.Button(bot, text='Save as New File', command=self._save_as,
                  bg='#4A90D9', fg='white', relief=tk.FLAT, padx=10, pady=3,
                  font=('Arial', 9, 'bold'), cursor='hand2').pack(side=tk.RIGHT, padx=4)

    def _on_scroll(self, event):
        if event.num == 4 or event.delta > 0:
            self._canvas.yview_scroll(-1, 'units')
        else:
            self._canvas.yview_scroll(1, 'units')

    # ── public ────────────────────────────────────────────────────────────

    def refresh_list(self):
        if self._index.directory:
            jsons = [f for f in self._index.files if f.endswith('.json')]
            self._tmpl_combo['values'] = jsons

    # ── row management ────────────────────────────────────────────────────

    def _add_row(self, key: str = '', value: str = ''):
        idx = len(self._rows)
        bg  = '#1E1E1E' if idx % 2 == 0 else '#252526'

        row = tk.Frame(self._rows_frame, bg=bg, pady=3)
        row.pack(fill=tk.X, padx=4, pady=1)

        _e = dict(bg='#3C3F41', fg='#A9B7C6', insertbackground='#A9B7C6',
                  relief=tk.FLAT, font=('Consolas', 10))

        key_e = tk.Entry(row, width=26, **_e)
        key_e.pack(side=tk.LEFT, padx=(6, 2), ipady=3)
        key_e.insert(0, key)

        tk.Label(row, text=':', bg=bg, fg='#A9B7C6').pack(side=tk.LEFT)

        val_e = tk.Entry(row, **_e)
        val_e.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 6), ipady=3)
        val_e.insert(0, value)

        rec = {'frame': row, 'key': key_e, 'value': val_e}

        def _remove():
            self._rows.remove(rec)
            row.destroy()
            self._canvas.configure(scrollregion=self._canvas.bbox('all'))

        tk.Button(row, text='✕', command=_remove,
                  bg='#5C2B2B', fg='#FF6B6B', relief=tk.FLAT,
                  font=('Arial', 9), cursor='hand2', padx=4,
                  ).pack(side=tk.RIGHT, padx=4)

        self._rows.append(rec)
        self._canvas.configure(scrollregion=self._canvas.bbox('all'))
        # scroll to bottom so new row is visible
        self._canvas.yview_moveto(1.0)

    def _clear(self):
        for r in self._rows:
            r['frame'].destroy()
        self._rows.clear()

    def _collect(self) -> dict:
        result = {}
        for r in self._rows:
            k = r['key'].get().strip()
            v = r['value'].get()
            if k:
                result[k] = v
        return result

    # ── template load / save ──────────────────────────────────────────────

    def _refresh(self):
        if self._index.directory:
            self._index.rebuild()
        self.refresh_list()

    def _load(self):
        """Load the template selected in the dropdown."""
        name = self._tmpl_var.get()
        if not name:
            messagebox.showinfo('No Template', 'Please select a JSON template from the list.')
            return
        self._load_template(name)

    def load_template_by_name(self, name: str) -> bool:
        """
        Load a template by filename (public method for external use).

        Args:
            name: The filename of the template (e.g., "my_template.json")

        Returns:
            True if loaded successfully, False otherwise
        """
        # Set the dropdown to this template
        self._tmpl_var.set(name)
        return self._load_template(name, show_errors=False)

    def _load_template(self, name: str, show_errors: bool = True) -> bool:
        """
        Internal method to load a template by name.

        Args:
            name: Template filename
            show_errors: Whether to show error dialogs

        Returns:
            True if loaded successfully
        """
        raw = self._index.file_contents.get(name, '')
        if not raw and self._index.directory:
            try:
                raw = Path(os.path.join(self._index.directory, name)).read_text(encoding='utf-8')
            except OSError as exc:
                if show_errors:
                    messagebox.showerror('Error', str(exc))
                return False
        try:
            data = json.loads(raw)
        except json.JSONDecodeError as exc:
            if show_errors:
                messagebox.showerror('Invalid JSON', f'Cannot parse template:\n{exc}')
            return False
        if not isinstance(data, dict):
            if show_errors:
                messagebox.showerror('Invalid Template',
                                     'Template must be a JSON object (key-value pairs at the top level).')
            return False
        self._clear()
        self._current_template = name
        self._load_dict(data)
        self._clear_autosave()  # Clear autosave when loading fresh template
        return True

    def _load_dict(self, data: dict, prefix: str = ''):
        for k, v in data.items():
            full_key = f'{prefix}{k}' if prefix else k
            if isinstance(v, dict):
                self._load_dict(v, prefix=full_key + '.')
            else:
                self._add_row(full_key, str(v) if v is not None else '')

    def _save_as(self):
        """Save current key-value pairs to a NEW file (preserves original template)."""
        if not self._index.directory:
            messagebox.showinfo('No Directory', 'Please select a working directory first.')
            return
        data = self._collect()
        if not data:
            messagebox.showinfo('Empty', 'Add at least one key-value pair before saving.')
            return
        fp = filedialog.asksaveasfilename(
            defaultextension='.json',
            filetypes=[('JSON files', '*.json'), ('All files', '*.*')],
            initialdir=self._index.directory,
        )
        if fp:
            content = json.dumps(data, indent=2)
            Path(fp).write_text(content, encoding='utf-8')
            fname = os.path.basename(fp)
            self._index.save_file(fname, content)
            if self._on_save:
                self._on_save()
            self._clear_autosave()  # Clear autosave after successful save
            messagebox.showinfo('Saved', f'Saved as  {fname}')

    def _overwrite(self):
        name = self._tmpl_var.get()
        if not name:
            messagebox.showinfo('No Template', 'Load a template first.')
            return
        if not messagebox.askyesno('Overwrite', f"Overwrite  '{name}'  with the current values?"):
            return
        data    = self._collect()
        content = json.dumps(data, indent=2)
        self._index.save_file(name, content)
        if self._on_save:
            self._on_save()
        self._clear_autosave()  # Clear autosave after successful save
        messagebox.showinfo('Saved', f"'{name}' has been updated.")

    # ── auto-save persistence ─────────────────────────────────────────────

    def _load_autosave(self) -> None:
        """Load work-in-progress from autosave file on startup."""
        if not Config.TEMPLATE_AUTOSAVE_ENABLED:
            return
        if not os.path.exists(Config.TEMPLATE_AUTOSAVE_PATH):
            return
        try:
            with open(Config.TEMPLATE_AUTOSAVE_PATH, 'r', encoding='utf-8') as f:
                data = json.load(f)
            if not isinstance(data, dict):
                return
            # Restore the template name if it was saved
            template_name = data.pop('_template_name_', None)
            if template_name:
                self._tmpl_var.set(template_name)
                self._current_template = template_name
            # Restore the key-value pairs
            for k, v in data.items():
                self._add_row(k, str(v) if v is not None else '')
        except (json.JSONDecodeError, OSError):
            pass  # Silently ignore corrupt autosave

    def do_autosave(self) -> None:
        """
        Save current work-in-progress to autosave file.
        Call this when the app is closing or periodically.
        """
        if not Config.TEMPLATE_AUTOSAVE_ENABLED:
            return
        if not self._rows:
            self._clear_autosave()
            return
        try:
            data = self._collect()
            # Also save which template was being edited
            if self._current_template:
                data['_template_name_'] = self._current_template
            with open(Config.TEMPLATE_AUTOSAVE_PATH, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)
        except OSError:
            pass  # Silently ignore write errors

    def _clear_autosave(self) -> None:
        """Remove the autosave file after successful save."""
        try:
            if os.path.exists(Config.TEMPLATE_AUTOSAVE_PATH):
                os.remove(Config.TEMPLATE_AUTOSAVE_PATH)
        except OSError:
            pass


# ─────────────────────────────────────────────────────────────────────────────
# SEARCH PANEL  –  left sidebar: search bar, mode toggle, results list
# ─────────────────────────────────────────────────────────────────────────────

class SearchPanel(tk.Frame):
    """Left sidebar with search controls and file results list."""

    def __init__(self, parent, index: FileIndex, on_file_select, **kw):
        super().__init__(parent, **kw)
        self._index          = index
        self._on_file_select = on_file_select
        self._build()

    def _build(self):
        # Search mode toggle
        mode_row = tk.Frame(self, bg=self.cget('bg'), padx=8, pady=6)
        mode_row.pack(fill=tk.X)

        tk.Label(mode_row, text='Search by:', bg=self.cget('bg'), fg='#A9B7C6',
                 font=('Arial', 9)).pack(side=tk.LEFT)

        self.mode = tk.StringVar(value='filename')
        _rb = dict(bg=self.cget('bg'), fg='#A9B7C6',
                   selectcolor='#4A90D9', activebackground=self.cget('bg'),
                   font=('Arial', 9))
        tk.Radiobutton(mode_row, text='Filename', variable=self.mode,
                       value='filename', **_rb).pack(side=tk.LEFT, padx=4)
        tk.Radiobutton(mode_row, text='Content',  variable=self.mode,
                       value='content',  **_rb).pack(side=tk.LEFT)

        # Search entry + clear button
        row2 = tk.Frame(self, bg=self.cget('bg'), padx=8, pady=0)
        row2.pack(fill=tk.X)

        self.entry = AutocompleteEntry(
            row2,
            suggestion_callback=self._suggest,
            on_select=self._do_search,
            bg='#3C3F41', fg='#A9B7C6',
            insertbackground='#A9B7C6',
            relief=tk.FLAT,
            font=('Arial', 11),
        )
        self.entry.pack(side=tk.LEFT, fill=tk.X, expand=True, ipady=4)

        tk.Button(row2, text='✕', command=self._clear,
                  bg='#3C3F41', fg='#888', relief=tk.FLAT,
                  padx=4, cursor='hand2').pack(side=tk.LEFT, padx=2)

        # Search button
        tk.Button(self, text='Search', command=self._do_search,
                  bg='#4A90D9', fg='white', relief=tk.FLAT,
                  padx=6, pady=5, font=('Arial', 10, 'bold'),
                  cursor='hand2').pack(fill=tk.X, padx=8, pady=4)

        # Files label
        lbl_row = tk.Frame(self, bg=self.cget('bg'))
        lbl_row.pack(fill=tk.X, padx=8)
        tk.Label(lbl_row, text='Files', bg=self.cget('bg'), fg='#666',
                 font=('Arial', 9, 'bold')).pack(side=tk.LEFT)
        self._count_lbl = tk.Label(lbl_row, text='', bg=self.cget('bg'),
                                    fg='#555', font=('Arial', 9))
        self._count_lbl.pack(side=tk.RIGHT)

        # File listbox
        lf = tk.Frame(self, bg=self.cget('bg'))
        lf.pack(fill=tk.BOTH, expand=True, padx=8, pady=(2, 8))

        vsb = tk.Scrollbar(lf, orient=tk.VERTICAL)
        self._listbox = tk.Listbox(
            lf,
            yscrollcommand=vsb.set,
            bg='#1E1E1E', fg='#A9B7C6',
            selectbackground='#4A90D9', selectforeground='#FFFFFF',
            font=('Consolas', 10),
            relief=tk.FLAT, activestyle='dotbox',
            cursor='hand2',
        )
        vsb.config(command=self._listbox.yview)
        vsb.pack(side=tk.RIGHT, fill=tk.Y)
        self._listbox.pack(fill=tk.BOTH, expand=True)

        self._listbox.bind('<<ListboxSelect>>', self._on_select)
        self._listbox.bind('<Double-Button-1>',  self._on_select)

    # ── public ────────────────────────────────────────────────────────────

    def populate(self, files: List[str]):
        self._listbox.delete(0, tk.END)
        for f in files:
            self._listbox.insert(tk.END, f'  {f}')
        self._count_lbl.config(text=f'{len(files)} file(s)')

    def get_search_term(self) -> str:
        return self.entry.get().strip()

    # ── internal ──────────────────────────────────────────────────────────

    def _suggest(self, text: str) -> List[str]:
        return self._index.autocomplete(text, self.mode.get())

    def _do_search(self, _val: str = ''):
        query = self.entry.get().strip()
        if not query:
            self.populate(self._index.files)
            return
        if self.mode.get() == 'filename':
            results = self._index.search_filenames(query)
        else:
            results = self._index.search_content(query)
        self.populate(results)

    def _clear(self):
        self.entry.clear()
        self.populate(self._index.files)

    def _on_select(self, _=None):
        sel = self._listbox.curselection()
        if sel:
            name = self._listbox.get(sel[0]).strip()
            self._on_file_select(name, self.entry.get().strip(),
                                 self.mode.get())


# ─────────────────────────────────────────────────────────────────────────────
# MAIN APPLICATION
# ─────────────────────────────────────────────────────────────────────────────

class FileDictionaryApp:
    """
    Main application class for the File Dictionary GUI.

    This class orchestrates all the UI components and handles the main
    application logic including directory selection, file browsing, and
    coordination between the search panel, content viewer, and template editor.

    Args:
        root: The tkinter root window
        initial_directory: Optional path to open on startup
    """

    def __init__(self, root: tk.Tk, initial_directory: Optional[str] = None):
        self.root = root
        self.index = FileIndex()
        self._initial_directory = initial_directory

        # Configure window
        root.title('File Dictionary')
        root.geometry(f'{Config.WINDOW_WIDTH}x{Config.WINDOW_HEIGHT}')
        root.minsize(Config.WINDOW_MIN_WIDTH, Config.WINDOW_MIN_HEIGHT)
        root.configure(bg='#1E1E1E')

        self._style()
        self._build()

        # Register close handler to save work-in-progress
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        # If an initial directory was provided, open it
        if self._initial_directory:
            self._load_directory(self._initial_directory)

    # ── styles ────────────────────────────────────────────────────────────

    def _style(self):
        s = ttk.Style()
        try:
            s.theme_use('clam')
        except tk.TclError:
            pass
        s.configure('TNotebook',     background='#1E1E1E', borderwidth=0)
        s.configure('TNotebook.Tab', background='#2D2D2D', foreground='#A9B7C6',
                    padding=[14, 5], font=('Arial', 10))
        s.map('TNotebook.Tab',
              background=[('selected', '#4A90D9')],
              foreground=[('selected', '#FFFFFF')])
        s.configure('TPanedwindow', background='#1E1E1E')
        s.configure('Sash', sashthickness=4, sashpad=2)
        s.configure('TCombobox',
                    fieldbackground='#3C3F41', foreground='#A9B7C6',
                    background='#3C3F41', selectbackground='#4A90D9')

    # ── layout ────────────────────────────────────────────────────────────

    def _build(self):
        # ── top bar
        top = tk.Frame(self.root, bg='#252526', padx=10, pady=6)
        top.pack(fill=tk.X)

        tk.Label(top, text='File Dictionary', bg='#252526', fg='#FFFFFF',
                 font=('Arial', 13, 'bold')).pack(side=tk.LEFT, padx=4)

        tk.Button(top, text='Open Directory', command=self._choose_dir,
                  bg='#4A90D9', fg='white', relief=tk.FLAT,
                  padx=10, pady=3, font=('Arial', 9), cursor='hand2',
                  ).pack(side=tk.LEFT, padx=12)

        self._dir_lbl = tk.Label(top, text='No directory selected',
                                  bg='#252526', fg='#555', font=('Arial', 9))
        self._dir_lbl.pack(side=tk.LEFT)

        tk.Button(top, text='⟳ Refresh', command=self._refresh,
                  bg='#3C3F41', fg='#A9B7C6', relief=tk.FLAT,
                  padx=8, pady=3, font=('Arial', 9), cursor='hand2',
                  ).pack(side=tk.RIGHT, padx=4)

        # ── notebook
        self._nb = ttk.Notebook(self.root)
        self._nb.pack(fill=tk.BOTH, expand=True)

        # Tab 1 – File Browser
        browser = tk.Frame(self._nb, bg='#1E1E1E')
        self._nb.add(browser, text='  File Browser  ')
        self._build_browser(browser)

        # Tab 2 – Template Editor
        tmpl_tab = tk.Frame(self._nb, bg='#1E1E1E')
        self._nb.add(tmpl_tab, text='  Template Editor  ')
        self._tmpl_editor = TemplateEditor(
            tmpl_tab, self.index, on_save=self._refresh, bg='#1E1E1E',
        )
        self._tmpl_editor.pack(fill=tk.BOTH, expand=True)

        self._nb.bind('<<NotebookTabChanged>>', self._on_tab)

        # ── status bar
        self._status = tk.Label(
            self.root, text='Select a directory to begin.',
            bg='#007ACC', fg='white', anchor='w', padx=8, pady=2,
            font=('Arial', 9),
        )
        self._status.pack(fill=tk.X, side=tk.BOTTOM)

    def _build_browser(self, parent):
        pane = ttk.PanedWindow(parent, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        # Left: search panel
        left = tk.Frame(pane, bg='#252526', width=280)
        self._search_panel = SearchPanel(
            left, self.index,
            on_file_select=self._open_file,
            bg='#252526',
        )
        self._search_panel.pack(fill=tk.BOTH, expand=True, pady=4)
        pane.add(left, weight=1)

        # Right: content viewer
        self._viewer = ContentViewer(pane, bg='#1E1E1E')
        pane.add(self._viewer, weight=3)

    # ── callbacks ─────────────────────────────────────────────────────────

    def _open_file(self, filename: str, search_term: str, mode: str):
        content = self.index.file_contents.get(filename, '')
        if not content and self.index.directory:
            try:
                content = Path(os.path.join(self.index.directory, filename)
                               ).read_text(encoding='utf-8', errors='replace')
            except OSError:
                content = '[Unable to read file]'
        self._viewer.load(filename, content)
        # If this was a content search, highlight the term in the viewer
        if mode == 'content' and search_term:
            self._viewer.highlight_term(search_term)
        self._status.config(text=f'Viewing  {filename}')

    def _choose_dir(self):
        """Open a directory picker dialog and load the selected directory."""
        d = filedialog.askdirectory(title='Select Working Directory')
        if d:
            self._load_directory(d)

    def _load_directory(self, directory: str):
        """
        Load and index a directory.

        Args:
            directory: Path to the directory to load
        """
        self.index.set_directory(directory)
        short = directory if len(directory) < 60 else '…' + directory[-57:]
        self._dir_lbl.config(text=short, fg='#A9B7C6')
        self._search_panel.populate(self.index.files)
        self._tmpl_editor.refresh_list()
        self.root.title(f'File Dictionary — {os.path.basename(directory)}')
        self._status.config(text=f'Loaded {len(self.index.files)} files from {directory}')

        # Load default template if configured and exists in this directory
        if Config.DEFAULT_TEMPLATE and Config.DEFAULT_TEMPLATE in self.index.files:
            self._tmpl_editor.load_template_by_name(Config.DEFAULT_TEMPLATE)
            self._status.config(
                text=f'Loaded {len(self.index.files)} files — Template: {Config.DEFAULT_TEMPLATE}'
            )

    def _refresh(self):
        if self.index.directory:
            self.index.rebuild()
            self._search_panel.populate(self.index.files)
            self._tmpl_editor.refresh_list()
            self._status.config(text=f'Refreshed — {len(self.index.files)} files')

    def _on_tab(self, _=None):
        if self._nb.index('current') == 1:
            self._tmpl_editor.refresh_list()

    def _on_close(self):
        """Handle window close: save work-in-progress and exit."""
        self._tmpl_editor.do_autosave()
        self.root.destroy()

    def run(self):
        self.root.mainloop()


# ═══════════════════════════════════════════════════════════════════════════════
# COMMAND-LINE INTERFACE
# ═══════════════════════════════════════════════════════════════════════════════

def parse_arguments() -> argparse.Namespace:
    """
    Parse command-line arguments.

    Returns:
        Namespace containing the parsed arguments
    """
    parser = argparse.ArgumentParser(
        description='File Dictionary - Browse, search, and manage text files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    %(prog)s                          # Open with directory picker
    %(prog)s /path/to/files           # Open specific directory
    %(prog)s -d /path/to/files        # Same, using flag
    %(prog)s --show-sample-config     # Print sample config file

Configuration:
    Create ~/.file_dictionary_config.json to set defaults.
    Run with --show-sample-config to see the format.
        """
    )

    parser.add_argument(
        'directory',
        nargs='?',
        default=None,
        help='Directory to open (overrides config file setting)'
    )

    parser.add_argument(
        '-d', '--directory',
        dest='directory_flag',
        metavar='PATH',
        help='Directory to open (alternative to positional argument)'
    )

    parser.add_argument(
        '--show-sample-config',
        action='store_true',
        help='Print a sample configuration file and exit'
    )

    return parser.parse_args()


def main():
    """
    Application entry point.

    Handles configuration loading and command-line argument parsing,
    then launches the GUI application.
    """
    args = parse_arguments()

    # Handle --show-sample-config
    if args.show_sample_config:
        print("Sample configuration file (~/.file_dictionary_config.json):")
        print("-" * 60)
        print(Config.create_sample_config_file())
        print("-" * 60)
        print(f"\nSave this to: {Config.CONFIG_FILE_PATH}")
        sys.exit(0)

    # Load configuration from file (if exists)
    Config.load_from_file()

    # Command-line directory overrides config file
    directory = args.directory_flag or args.directory or Config.DEFAULT_DIRECTORY

    # Validate directory if provided
    if directory and not os.path.isdir(directory):
        print(f"Error: Directory not found: {directory}", file=sys.stderr)
        sys.exit(1)

    # Launch the application
    root = tk.Tk()
    app = FileDictionaryApp(root, initial_directory=directory)
    app.run()


if __name__ == '__main__':
    main()
