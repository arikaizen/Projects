#!/usr/bin/env python3
"""
================================================================================
FILE DICTIONARY - WEB APPLICATION
================================================================================

A local web application for managing, searching, and viewing text files
with JSON template support. Runs in your browser at http://localhost:5000

QUICK START:
    pip install flask
    python file_dictionary_web.py

Then open http://localhost:5000 in your browser.

================================================================================
"""

import json
import os
import re
import sys
import argparse
from pathlib import Path
from typing import Optional, List, Dict, Set

try:
    from flask import Flask, render_template_string, request, jsonify
except ImportError:
    print("Flask is required. Install it with:")
    print("  pip install flask")
    sys.exit(1)


# ═══════════════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════════

class Config:
    DEFAULT_DIRECTORY: Optional[str] = None
    DEFAULT_TEMPLATE: Optional[str] = None

    SUPPORTED_EXTENSIONS: Set[str] = {
        '.txt', '.md', '.json', '.csv', '.py', '.yaml', '.yml',
        '.html', '.css', '.js', '.xml', '.rst', '.ini', '.cfg',
        '.toml', '.sh', '.bat', '.log', '.text', '.sql',
    }

    CONFIG_FILE_PATH: str = os.path.expanduser("~/.file_dictionary_config.json")
    AUTOSAVE_PATH: str = os.path.expanduser("~/.file_dictionary_web_autosave.json")

    @classmethod
    def load_from_file(cls):
        if os.path.exists(cls.CONFIG_FILE_PATH):
            try:
                with open(cls.CONFIG_FILE_PATH, 'r') as f:
                    data = json.load(f)
                if 'default_directory' in data:
                    cls.DEFAULT_DIRECTORY = data['default_directory']
                if 'default_template' in data:
                    cls.DEFAULT_TEMPLATE = data['default_template']
                if 'supported_extensions' in data:
                    cls.SUPPORTED_EXTENSIONS = set(data['supported_extensions'])
            except (json.JSONDecodeError, OSError):
                pass


# ═══════════════════════════════════════════════════════════════════════════════
# FILE INDEX
# ═══════════════════════════════════════════════════════════════════════════════

class FileIndex:
    def __init__(self):
        self.directory: Optional[str] = None
        self.files: List[str] = []
        self.word_index: Dict[str, Set[str]] = {}
        self.file_contents: Dict[str, str] = {}

    def set_directory(self, directory: str) -> bool:
        if not os.path.isdir(directory):
            return False
        self.directory = directory
        self.rebuild()
        return True

    def rebuild(self):
        self.files.clear()
        self.word_index.clear()
        self.file_contents.clear()
        if not self.directory:
            return
        try:
            entries = sorted(os.scandir(self.directory), key=lambda e: e.name.lower())
            for entry in entries:
                if entry.is_file() and Path(entry.name).suffix.lower() in Config.SUPPORTED_EXTENSIONS:
                    self._index_file(entry.name, entry.path)
        except OSError:
            pass

    def _index_file(self, name: str, path: str):
        try:
            content = Path(path).read_text(encoding='utf-8', errors='replace')
        except OSError:
            return
        self.file_contents[name] = content
        if name not in self.files:
            self.files.append(name)
        for word in set(re.findall(r'\b\w{2,}\b', content.lower())):
            self.word_index.setdefault(word, set()).add(name)

    def search_filenames(self, query: str) -> List[str]:
        q = query.lower()
        return [f for f in self.files if q in f.lower()]

    def search_content(self, query: str) -> List[str]:
        q = query.lower()
        if q in self.word_index:
            return sorted(self.word_index[q])
        hits: Set[str] = set()
        for word, files in self.word_index.items():
            if q in word:
                hits.update(files)
        return sorted(hits)

    def autocomplete(self, prefix: str, mode: str) -> List[str]:
        if not prefix:
            return []
        p = prefix.lower()
        if mode == 'filename':
            exact = [f for f in self.files if f.lower().startswith(p)]
            partial = [f for f in self.files if p in f.lower() and f not in exact]
            return (exact + partial)[:15]
        else:
            words = sorted(w for w in self.word_index if w.startswith(p))
            return words[:20]

    def save_file(self, filename: str, content: str) -> bool:
        if not self.directory:
            return False
        try:
            path = os.path.join(self.directory, filename)
            Path(path).write_text(content, encoding='utf-8')
            self._index_file(filename, path)
            return True
        except OSError:
            return False


# ═══════════════════════════════════════════════════════════════════════════════
# FLASK APPLICATION
# ═══════════════════════════════════════════════════════════════════════════════

app = Flask(__name__)
file_index = FileIndex()


# ═══════════════════════════════════════════════════════════════════════════════
# HTML TEMPLATE
# ═══════════════════════════════════════════════════════════════════════════════

HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>File Dictionary</title>
    <style>
        :root {
            --bg-dark: #1e1e1e;
            --bg-panel: #252526;
            --bg-input: #3c3f41;
            --bg-hover: #2a2d2e;
            --text-primary: #d4d4d4;
            --text-secondary: #858585;
            --accent: #4a90d9;
            --accent-hover: #5a9fe9;
            --success: #6aae56;
            --warning: #d19a66;
            --danger: #e06c75;
            --highlight: #4b4b00;
            --highlight-current: #ff8c00;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-dark);
            color: var(--text-primary);
            height: 100vh;
            display: flex;
            flex-direction: column;
        }

        /* Header */
        .header {
            background: var(--bg-panel);
            padding: 12px 20px;
            display: flex;
            align-items: center;
            gap: 20px;
            border-bottom: 1px solid #333;
        }

        .header h1 {
            font-size: 18px;
            font-weight: 600;
            color: #fff;
        }

        .header-controls {
            display: flex;
            gap: 10px;
            align-items: center;
        }

        .btn {
            padding: 8px 16px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 13px;
            font-weight: 500;
            transition: background 0.2s;
        }

        .btn-primary { background: var(--accent); color: #fff; }
        .btn-primary:hover { background: var(--accent-hover); }
        .btn-secondary { background: var(--bg-input); color: var(--text-primary); }
        .btn-secondary:hover { background: #4a4d4e; }
        .btn-success { background: var(--success); color: #fff; }
        .btn-warning { background: var(--warning); color: #000; }
        .btn-danger { background: var(--danger); color: #fff; }

        .dir-display {
            color: var(--text-secondary);
            font-size: 13px;
            max-width: 400px;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }

        /* Tabs */
        .tabs {
            display: flex;
            background: var(--bg-panel);
            border-bottom: 1px solid #333;
        }

        .tab {
            padding: 12px 24px;
            cursor: pointer;
            color: var(--text-secondary);
            font-size: 14px;
            border-bottom: 2px solid transparent;
            transition: all 0.2s;
        }

        .tab:hover { color: var(--text-primary); }
        .tab.active {
            color: #fff;
            border-bottom-color: var(--accent);
        }

        /* Main Content */
        .main {
            flex: 1;
            display: flex;
            overflow: hidden;
        }

        .tab-content {
            display: none;
            flex: 1;
            overflow: hidden;
        }

        .tab-content.active {
            display: flex;
        }

        /* File Browser Tab */
        .sidebar {
            width: 280px;
            background: var(--bg-panel);
            border-right: 1px solid #333;
            display: flex;
            flex-direction: column;
            overflow: hidden;
        }

        .search-section {
            padding: 16px;
            border-bottom: 1px solid #333;
        }

        .search-mode {
            display: flex;
            gap: 16px;
            margin-bottom: 12px;
        }

        .search-mode label {
            display: flex;
            align-items: center;
            gap: 6px;
            font-size: 13px;
            color: var(--text-secondary);
            cursor: pointer;
        }

        .search-mode input[type="radio"] {
            accent-color: var(--accent);
        }

        .search-input-wrapper {
            position: relative;
        }

        .search-input {
            width: 100%;
            padding: 10px 12px;
            background: var(--bg-input);
            border: 1px solid transparent;
            border-radius: 4px;
            color: var(--text-primary);
            font-size: 14px;
            outline: none;
        }

        .search-input:focus {
            border-color: var(--accent);
        }

        .autocomplete-dropdown {
            position: absolute;
            top: 100%;
            left: 0;
            right: 0;
            background: var(--bg-input);
            border: 1px solid #555;
            border-radius: 4px;
            max-height: 200px;
            overflow-y: auto;
            z-index: 100;
            display: none;
        }

        .autocomplete-dropdown.show { display: block; }

        .autocomplete-item {
            padding: 8px 12px;
            cursor: pointer;
            font-size: 13px;
        }

        .autocomplete-item:hover,
        .autocomplete-item.selected {
            background: var(--accent);
            color: #fff;
        }

        .file-list-header {
            padding: 12px 16px;
            font-size: 12px;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 0.5px;
            display: flex;
            justify-content: space-between;
        }

        .file-list {
            flex: 1;
            overflow-y: auto;
            padding: 0 8px;
        }

        .file-item {
            padding: 10px 12px;
            cursor: pointer;
            border-radius: 4px;
            font-size: 13px;
            font-family: 'Consolas', 'Monaco', monospace;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }

        .file-item:hover { background: var(--bg-hover); }
        .file-item.active { background: var(--accent); color: #fff; }

        /* Content Viewer */
        .content-area {
            flex: 1;
            display: flex;
            flex-direction: column;
            overflow: hidden;
        }

        .content-header {
            padding: 12px 20px;
            background: var(--bg-panel);
            border-bottom: 1px solid #333;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .content-filename {
            font-weight: 600;
            font-size: 14px;
        }

        .content-info {
            font-size: 12px;
            color: var(--text-secondary);
        }

        .find-bar {
            padding: 10px 20px;
            background: #1a1a1a;
            display: flex;
            align-items: center;
            gap: 12px;
        }

        .find-bar label {
            font-size: 13px;
            color: var(--text-secondary);
        }

        .find-bar input {
            padding: 6px 10px;
            background: var(--bg-input);
            border: 1px solid transparent;
            border-radius: 4px;
            color: var(--text-primary);
            font-size: 13px;
            width: 200px;
            outline: none;
        }

        .find-bar input:focus { border-color: var(--accent); }

        .find-bar .btn { padding: 6px 12px; font-size: 12px; }

        .match-info {
            font-size: 12px;
            color: var(--text-secondary);
            min-width: 80px;
        }

        .content-viewer {
            flex: 1;
            overflow: auto;
            padding: 20px;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
            font-size: 14px;
            line-height: 1.6;
            white-space: pre-wrap;
            word-wrap: break-word;
        }

        .content-viewer .highlight {
            background: var(--highlight);
            color: #ffe066;
            padding: 1px 2px;
            border-radius: 2px;
        }

        .content-viewer .highlight.current {
            background: var(--highlight-current);
            color: #fff;
        }

        .placeholder {
            color: var(--text-secondary);
            text-align: center;
            padding: 60px 20px;
            font-size: 15px;
        }

        /* Template Editor Tab */
        .template-editor {
            flex: 1;
            display: flex;
            flex-direction: column;
            padding: 20px;
            gap: 16px;
            overflow: hidden;
        }

        .template-controls {
            display: flex;
            gap: 12px;
            align-items: center;
            flex-wrap: wrap;
        }

        .template-controls select {
            padding: 8px 12px;
            background: var(--bg-input);
            border: 1px solid #555;
            border-radius: 4px;
            color: var(--text-primary);
            font-size: 14px;
            min-width: 250px;
        }

        .template-rows-container {
            flex: 1;
            overflow-y: auto;
            background: var(--bg-panel);
            border-radius: 8px;
            padding: 16px;
        }

        .template-row-header {
            display: grid;
            grid-template-columns: 250px 1fr 40px;
            gap: 12px;
            padding: 8px 0;
            font-size: 12px;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 0.5px;
            border-bottom: 1px solid #333;
            margin-bottom: 8px;
        }

        .template-row {
            display: grid;
            grid-template-columns: 250px 1fr 40px;
            gap: 12px;
            padding: 8px 0;
            align-items: center;
        }

        .template-row:nth-child(odd) {
            background: rgba(255,255,255,0.02);
            border-radius: 4px;
            padding: 8px;
            margin: 0 -8px;
        }

        .template-row input {
            padding: 10px 12px;
            background: var(--bg-input);
            border: 1px solid transparent;
            border-radius: 4px;
            color: var(--text-primary);
            font-size: 14px;
            font-family: 'Consolas', monospace;
            outline: none;
        }

        .template-row input:focus { border-color: var(--accent); }

        .template-row .btn-remove {
            width: 32px;
            height: 32px;
            padding: 0;
            background: transparent;
            color: var(--danger);
            border: 1px solid var(--danger);
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
        }

        .template-row .btn-remove:hover {
            background: var(--danger);
            color: #fff;
        }

        .template-actions {
            display: flex;
            gap: 12px;
            padding-top: 16px;
            border-top: 1px solid #333;
        }

        /* Status Bar */
        .status-bar {
            padding: 8px 20px;
            background: var(--accent);
            color: #fff;
            font-size: 13px;
        }

        /* Modal */
        .modal-overlay {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0,0,0,0.7);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 1000;
        }

        .modal {
            background: var(--bg-panel);
            border-radius: 8px;
            padding: 24px;
            min-width: 400px;
            max-width: 90%;
        }

        .modal h2 {
            margin-bottom: 16px;
            font-size: 18px;
        }

        .modal input {
            width: 100%;
            padding: 10px 12px;
            background: var(--bg-input);
            border: 1px solid #555;
            border-radius: 4px;
            color: var(--text-primary);
            font-size: 14px;
            margin-bottom: 16px;
        }

        .modal-actions {
            display: flex;
            gap: 12px;
            justify-content: flex-end;
        }

        .hidden { display: none !important; }
    </style>
</head>
<body>
    <div class="header">
        <h1>File Dictionary</h1>
        <div class="header-controls">
            <button class="btn btn-primary" onclick="chooseDirectory()">Open Directory</button>
            <button class="btn btn-secondary" onclick="refresh()">Refresh</button>
        </div>
        <div class="dir-display" id="dirDisplay">No directory selected</div>
    </div>

    <div class="tabs">
        <div class="tab active" onclick="switchTab('browser')">File Browser</div>
        <div class="tab" onclick="switchTab('template')">Template Editor</div>
    </div>

    <div class="main">
        <!-- File Browser Tab -->
        <div class="tab-content active" id="browserTab">
            <div class="sidebar">
                <div class="search-section">
                    <div class="search-mode">
                        <label>
                            <input type="radio" name="searchMode" value="filename" checked>
                            Filename
                        </label>
                        <label>
                            <input type="radio" name="searchMode" value="content">
                            Content
                        </label>
                    </div>
                    <div class="search-input-wrapper">
                        <input type="text" class="search-input" id="searchInput"
                               placeholder="Search files..." autocomplete="off">
                        <div class="autocomplete-dropdown" id="autocomplete"></div>
                    </div>
                </div>
                <div class="file-list-header">
                    <span>Files</span>
                    <span id="fileCount">0</span>
                </div>
                <div class="file-list" id="fileList"></div>
            </div>

            <div class="content-area">
                <div class="content-header">
                    <span class="content-filename" id="contentFilename">No file selected</span>
                    <span class="content-info" id="contentInfo"></span>
                </div>
                <div class="find-bar">
                    <label>Find:</label>
                    <input type="text" id="findInput" placeholder="Search in file...">
                    <button class="btn btn-secondary" onclick="findPrev()">Prev</button>
                    <button class="btn btn-secondary" onclick="findNext()">Next</button>
                    <span class="match-info" id="matchInfo"></span>
                    <button class="btn btn-secondary" onclick="clearFind()">Clear</button>
                </div>
                <div class="content-viewer" id="contentViewer">
                    <div class="placeholder">
                        Select a file from the list to view its contents.<br><br>
                        Or click "Open Directory" to browse your files.
                    </div>
                </div>
            </div>
        </div>

        <!-- Template Editor Tab -->
        <div class="tab-content" id="templateTab">
            <div class="template-editor">
                <div class="template-controls">
                    <select id="templateSelect">
                        <option value="">-- Select a template --</option>
                    </select>
                    <button class="btn btn-primary" onclick="loadTemplate()">Load Template</button>
                    <button class="btn btn-secondary" onclick="refreshTemplates()">Refresh List</button>
                </div>

                <div class="template-rows-container">
                    <div class="template-row-header">
                        <span>Key</span>
                        <span>Value</span>
                        <span></span>
                    </div>
                    <div id="templateRows"></div>
                </div>

                <div class="template-actions">
                    <button class="btn btn-success" onclick="addTemplateRow()">+ Add Row</button>
                    <button class="btn btn-secondary" onclick="clearTemplateRows()">Clear All</button>
                    <div style="flex:1"></div>
                    <button class="btn btn-warning" onclick="overwriteTemplate()">Overwrite Template</button>
                    <button class="btn btn-primary" onclick="saveAsNewFile()">Save as New File</button>
                </div>
            </div>
        </div>
    </div>

    <div class="status-bar" id="statusBar">Select a directory to begin.</div>

    <!-- Directory Input Modal -->
    <div class="modal-overlay hidden" id="dirModal">
        <div class="modal">
            <h2>Enter Directory Path</h2>
            <input type="text" id="dirInput" placeholder="/path/to/your/files">
            <div class="modal-actions">
                <button class="btn btn-secondary" onclick="closeDirModal()">Cancel</button>
                <button class="btn btn-primary" onclick="submitDirectory()">Open</button>
            </div>
        </div>
    </div>

    <!-- Save As Modal -->
    <div class="modal-overlay hidden" id="saveModal">
        <div class="modal">
            <h2>Save as New File</h2>
            <input type="text" id="saveFilename" placeholder="filename.json">
            <div class="modal-actions">
                <button class="btn btn-secondary" onclick="closeSaveModal()">Cancel</button>
                <button class="btn btn-primary" onclick="confirmSave()">Save</button>
            </div>
        </div>
    </div>

    <script>
        // State
        let currentFile = null;
        let currentContent = '';
        let matches = [];
        let currentMatchIndex = -1;
        let currentTemplate = null;

        // =====================================================================
        // TAB SWITCHING
        // =====================================================================

        function switchTab(tabName) {
            document.querySelectorAll('.tab').forEach((t, i) => {
                t.classList.toggle('active', (tabName === 'browser' && i === 0) || (tabName === 'template' && i === 1));
            });
            document.getElementById('browserTab').classList.toggle('active', tabName === 'browser');
            document.getElementById('templateTab').classList.toggle('active', tabName === 'template');

            if (tabName === 'template') {
                refreshTemplates();
            }
        }

        // =====================================================================
        // DIRECTORY MANAGEMENT
        // =====================================================================

        function chooseDirectory() {
            document.getElementById('dirModal').classList.remove('hidden');
            document.getElementById('dirInput').focus();
        }

        function closeDirModal() {
            document.getElementById('dirModal').classList.add('hidden');
        }

        function submitDirectory() {
            const dir = document.getElementById('dirInput').value.trim();
            if (!dir) return;

            fetch('/api/set-directory', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({directory: dir})
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    closeDirModal();
                    document.getElementById('dirDisplay').textContent = dir;
                    loadFileList();
                    refreshTemplates();
                    setStatus('Loaded ' + data.file_count + ' files from ' + dir);
                } else {
                    alert('Error: ' + data.error);
                }
            });
        }

        function refresh() {
            fetch('/api/refresh', {method: 'POST'})
            .then(r => r.json())
            .then(data => {
                loadFileList();
                refreshTemplates();
                setStatus('Refreshed - ' + data.file_count + ' files');
            });
        }

        // =====================================================================
        // FILE LIST
        // =====================================================================

        function loadFileList() {
            fetch('/api/files')
            .then(r => r.json())
            .then(files => {
                renderFileList(files);
            });
        }

        function renderFileList(files) {
            const list = document.getElementById('fileList');
            list.innerHTML = files.map(f =>
                `<div class="file-item ${f === currentFile ? 'active' : ''}" onclick="selectFile('${f}')">${f}</div>`
            ).join('');
            document.getElementById('fileCount').textContent = files.length;
        }

        function selectFile(filename) {
            currentFile = filename;
            fetch('/api/file-content?name=' + encodeURIComponent(filename))
            .then(r => r.json())
            .then(data => {
                currentContent = data.content;
                document.getElementById('contentFilename').textContent = filename;
                const lines = (currentContent.match(/\\n/g) || []).length + 1;
                document.getElementById('contentInfo').textContent = lines + ' lines, ' + currentContent.length + ' chars';
                document.getElementById('contentViewer').textContent = currentContent;

                // Highlight active file in list
                document.querySelectorAll('.file-item').forEach(el => {
                    el.classList.toggle('active', el.textContent === filename);
                });

                // Re-run find if there's a query
                const findQuery = document.getElementById('findInput').value;
                if (findQuery) {
                    runFind();
                }
            });
        }

        // =====================================================================
        // SEARCH & AUTOCOMPLETE
        // =====================================================================

        document.getElementById('searchInput').addEventListener('input', function(e) {
            const query = e.target.value;
            const mode = document.querySelector('input[name="searchMode"]:checked').value;

            if (query.length > 0) {
                fetch('/api/autocomplete?q=' + encodeURIComponent(query) + '&mode=' + mode)
                .then(r => r.json())
                .then(suggestions => {
                    const dropdown = document.getElementById('autocomplete');
                    if (suggestions.length > 0) {
                        dropdown.innerHTML = suggestions.map(s =>
                            `<div class="autocomplete-item" onclick="selectSuggestion('${s}')">${s}</div>`
                        ).join('');
                        dropdown.classList.add('show');
                    } else {
                        dropdown.classList.remove('show');
                    }
                });
            } else {
                document.getElementById('autocomplete').classList.remove('show');
                loadFileList();
            }
        });

        document.getElementById('searchInput').addEventListener('keydown', function(e) {
            if (e.key === 'Enter') {
                doSearch();
                document.getElementById('autocomplete').classList.remove('show');
            } else if (e.key === 'Escape') {
                document.getElementById('autocomplete').classList.remove('show');
            }
        });

        function selectSuggestion(value) {
            document.getElementById('searchInput').value = value;
            document.getElementById('autocomplete').classList.remove('show');
            doSearch();
        }

        function doSearch() {
            const query = document.getElementById('searchInput').value;
            const mode = document.querySelector('input[name="searchMode"]:checked').value;

            fetch('/api/search?q=' + encodeURIComponent(query) + '&mode=' + mode)
            .then(r => r.json())
            .then(files => {
                renderFileList(files);
                setStatus('Found ' + files.length + ' files for "' + query + '"');
            });
        }

        // Close autocomplete when clicking outside
        document.addEventListener('click', function(e) {
            if (!e.target.closest('.search-input-wrapper')) {
                document.getElementById('autocomplete').classList.remove('show');
            }
        });

        // =====================================================================
        // FIND IN FILE
        // =====================================================================

        document.getElementById('findInput').addEventListener('input', runFind);
        document.getElementById('findInput').addEventListener('keydown', function(e) {
            if (e.key === 'Enter') findNext();
        });

        function runFind() {
            const query = document.getElementById('findInput').value;
            matches = [];
            currentMatchIndex = -1;

            if (!query || !currentContent) {
                document.getElementById('contentViewer').textContent = currentContent;
                document.getElementById('matchInfo').textContent = '';
                return;
            }

            const regex = new RegExp(escapeRegex(query), 'gi');
            let match;
            while ((match = regex.exec(currentContent)) !== null) {
                matches.push({start: match.index, end: match.index + match[0].length, text: match[0]});
            }

            if (matches.length > 0) {
                currentMatchIndex = 0;
                renderHighlights();
                document.getElementById('matchInfo').textContent = '1/' + matches.length;
            } else {
                document.getElementById('contentViewer').textContent = currentContent;
                document.getElementById('matchInfo').textContent = 'No matches';
            }
        }

        function renderHighlights() {
            let html = '';
            let lastEnd = 0;

            matches.forEach((m, i) => {
                html += escapeHtml(currentContent.slice(lastEnd, m.start));
                const cls = i === currentMatchIndex ? 'highlight current' : 'highlight';
                html += '<span class="' + cls + '">' + escapeHtml(m.text) + '</span>';
                lastEnd = m.end;
            });
            html += escapeHtml(currentContent.slice(lastEnd));

            document.getElementById('contentViewer').innerHTML = html;

            // Scroll to current match
            const current = document.querySelector('.highlight.current');
            if (current) current.scrollIntoView({block: 'center'});
        }

        function findNext() {
            if (matches.length === 0) return;
            currentMatchIndex = (currentMatchIndex + 1) % matches.length;
            renderHighlights();
            document.getElementById('matchInfo').textContent = (currentMatchIndex + 1) + '/' + matches.length;
        }

        function findPrev() {
            if (matches.length === 0) return;
            currentMatchIndex = (currentMatchIndex - 1 + matches.length) % matches.length;
            renderHighlights();
            document.getElementById('matchInfo').textContent = (currentMatchIndex + 1) + '/' + matches.length;
        }

        function clearFind() {
            document.getElementById('findInput').value = '';
            matches = [];
            currentMatchIndex = -1;
            document.getElementById('contentViewer').textContent = currentContent;
            document.getElementById('matchInfo').textContent = '';
        }

        // =====================================================================
        // TEMPLATE EDITOR
        // =====================================================================

        function refreshTemplates() {
            fetch('/api/templates')
            .then(r => r.json())
            .then(templates => {
                const select = document.getElementById('templateSelect');
                select.innerHTML = '<option value="">-- Select a template --</option>' +
                    templates.map(t => '<option value="' + t + '">' + t + '</option>').join('');
            });
        }

        function loadTemplate() {
            const name = document.getElementById('templateSelect').value;
            if (!name) {
                alert('Please select a template first.');
                return;
            }

            fetch('/api/load-template?name=' + encodeURIComponent(name))
            .then(r => r.json())
            .then(data => {
                if (data.error) {
                    alert(data.error);
                    return;
                }
                currentTemplate = name;
                renderTemplateRows(data.data);
                setStatus('Loaded template: ' + name);
            });
        }

        function renderTemplateRows(data) {
            const container = document.getElementById('templateRows');
            container.innerHTML = '';

            Object.entries(data).forEach(([key, value]) => {
                addTemplateRow(key, value);
            });
        }

        function addTemplateRow(key = '', value = '') {
            const container = document.getElementById('templateRows');
            const row = document.createElement('div');
            row.className = 'template-row';
            row.innerHTML = `
                <input type="text" class="template-key" placeholder="key" value="${escapeHtml(key)}">
                <input type="text" class="template-value" placeholder="value" value="${escapeHtml(value)}">
                <button class="btn-remove" onclick="this.parentElement.remove()">×</button>
            `;
            container.appendChild(row);
        }

        function clearTemplateRows() {
            document.getElementById('templateRows').innerHTML = '';
            currentTemplate = null;
        }

        function collectTemplateData() {
            const data = {};
            document.querySelectorAll('.template-row').forEach(row => {
                const key = row.querySelector('.template-key').value.trim();
                const value = row.querySelector('.template-value').value;
                if (key) data[key] = value;
            });
            return data;
        }

        function saveAsNewFile() {
            const data = collectTemplateData();
            if (Object.keys(data).length === 0) {
                alert('Add at least one key-value pair first.');
                return;
            }
            document.getElementById('saveModal').classList.remove('hidden');
            document.getElementById('saveFilename').focus();
        }

        function closeSaveModal() {
            document.getElementById('saveModal').classList.add('hidden');
        }

        function confirmSave() {
            let filename = document.getElementById('saveFilename').value.trim();
            if (!filename) return;
            if (!filename.endsWith('.json')) filename += '.json';

            const data = collectTemplateData();

            fetch('/api/save-file', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({filename: filename, data: data})
            })
            .then(r => r.json())
            .then(result => {
                if (result.success) {
                    closeSaveModal();
                    setStatus('Saved as ' + filename);
                    refreshTemplates();
                    loadFileList();
                } else {
                    alert('Error: ' + result.error);
                }
            });
        }

        function overwriteTemplate() {
            const name = document.getElementById('templateSelect').value;
            if (!name) {
                alert('Load a template first.');
                return;
            }

            if (!confirm('Overwrite "' + name + '" with current values?')) return;

            const data = collectTemplateData();

            fetch('/api/save-file', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({filename: name, data: data})
            })
            .then(r => r.json())
            .then(result => {
                if (result.success) {
                    setStatus('Updated ' + name);
                } else {
                    alert('Error: ' + result.error);
                }
            });
        }

        // =====================================================================
        // UTILITIES
        // =====================================================================

        function setStatus(msg) {
            document.getElementById('statusBar').textContent = msg;
        }

        function escapeHtml(str) {
            return str.replace(/&/g, '&amp;')
                      .replace(/</g, '&lt;')
                      .replace(/>/g, '&gt;')
                      .replace(/"/g, '&quot;');
        }

        function escapeRegex(str) {
            return str.replace(/[.*+?^${}()|[\\]\\\\]/g, '\\\\$&');
        }

        // Enter key in modals
        document.getElementById('dirInput').addEventListener('keydown', e => {
            if (e.key === 'Enter') submitDirectory();
            if (e.key === 'Escape') closeDirModal();
        });

        document.getElementById('saveFilename').addEventListener('keydown', e => {
            if (e.key === 'Enter') confirmSave();
            if (e.key === 'Escape') closeSaveModal();
        });

        // Load initial state
        fetch('/api/current-state')
        .then(r => r.json())
        .then(data => {
            if (data.directory) {
                document.getElementById('dirDisplay').textContent = data.directory;
                loadFileList();
                refreshTemplates();
                setStatus('Loaded ' + data.file_count + ' files');
            }
        });
    </script>
</body>
</html>
'''


# ═══════════════════════════════════════════════════════════════════════════════
# API ROUTES
# ═══════════════════════════════════════════════════════════════════════════════

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)


@app.route('/api/current-state')
def current_state():
    return jsonify({
        'directory': file_index.directory,
        'file_count': len(file_index.files)
    })


@app.route('/api/set-directory', methods=['POST'])
def set_directory():
    data = request.get_json()
    directory = data.get('directory', '').strip()

    if not directory:
        return jsonify({'success': False, 'error': 'No directory specified'})

    # Expand user home directory
    directory = os.path.expanduser(directory)

    if not os.path.isdir(directory):
        return jsonify({'success': False, 'error': f'Directory not found: {directory}'})

    file_index.set_directory(directory)
    return jsonify({
        'success': True,
        'file_count': len(file_index.files)
    })


@app.route('/api/refresh', methods=['POST'])
def refresh():
    file_index.rebuild()
    return jsonify({'file_count': len(file_index.files)})


@app.route('/api/files')
def get_files():
    return jsonify(file_index.files)


@app.route('/api/file-content')
def get_file_content():
    name = request.args.get('name', '')
    content = file_index.file_contents.get(name, '')
    return jsonify({'content': content})


@app.route('/api/search')
def search():
    query = request.args.get('q', '')
    mode = request.args.get('mode', 'filename')

    if not query:
        return jsonify(file_index.files)

    if mode == 'filename':
        results = file_index.search_filenames(query)
    else:
        results = file_index.search_content(query)

    return jsonify(results)


@app.route('/api/autocomplete')
def autocomplete():
    query = request.args.get('q', '')
    mode = request.args.get('mode', 'filename')
    suggestions = file_index.autocomplete(query, mode)
    return jsonify(suggestions)


@app.route('/api/templates')
def get_templates():
    templates = [f for f in file_index.files if f.endswith('.json')]
    return jsonify(templates)


@app.route('/api/load-template')
def load_template():
    name = request.args.get('name', '')
    content = file_index.file_contents.get(name, '')

    if not content:
        return jsonify({'error': 'Template not found'})

    try:
        data = json.loads(content)
        if not isinstance(data, dict):
            return jsonify({'error': 'Template must be a JSON object'})
        # Flatten nested dicts
        flat = {}
        def flatten(d, prefix=''):
            for k, v in d.items():
                key = f'{prefix}{k}' if prefix else k
                if isinstance(v, dict):
                    flatten(v, key + '.')
                else:
                    flat[key] = str(v) if v is not None else ''
        flatten(data)
        return jsonify({'data': flat})
    except json.JSONDecodeError as e:
        return jsonify({'error': f'Invalid JSON: {e}'})


@app.route('/api/save-file', methods=['POST'])
def save_file():
    data = request.get_json()
    filename = data.get('filename', '')
    content = data.get('data', {})

    if not filename:
        return jsonify({'success': False, 'error': 'No filename specified'})

    if not file_index.directory:
        return jsonify({'success': False, 'error': 'No directory selected'})

    try:
        json_content = json.dumps(content, indent=2)
        success = file_index.save_file(filename, json_content)
        return jsonify({'success': success})
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)})


# ═══════════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description='File Dictionary Web Application')
    parser.add_argument('directory', nargs='?', help='Directory to open')
    parser.add_argument('-d', '--directory', dest='dir_flag', help='Directory to open')
    parser.add_argument('-p', '--port', type=int, default=5000, help='Port to run on (default: 5000)')
    parser.add_argument('--host', default='127.0.0.1', help='Host to bind to (default: 127.0.0.1)')
    args = parser.parse_args()

    # Load config
    Config.load_from_file()

    # Set initial directory
    directory = args.dir_flag or args.directory or Config.DEFAULT_DIRECTORY
    if directory:
        directory = os.path.expanduser(directory)
        if os.path.isdir(directory):
            file_index.set_directory(directory)
            print(f"Loaded {len(file_index.files)} files from {directory}")

    print(f"\n{'='*60}")
    print(f"  File Dictionary Web Application")
    print(f"  Open http://{args.host}:{args.port} in your browser")
    print(f"{'='*60}\n")

    app.run(host=args.host, port=args.port, debug=False)


if __name__ == '__main__':
    main()
