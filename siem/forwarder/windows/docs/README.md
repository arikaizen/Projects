# Windows Event Log Forwarder - Documentation

This directory contains comprehensive UML diagrams and architecture documentation for the Windows Event Log Forwarder project.

## Available Diagrams

### 1. [Class Diagram](class_diagram.md)
**Purpose**: Shows the object-oriented structure of the application

**Contains**:
- All classes (Logger, LogForwarder)
- Enumerations (LogLevel)
- Utility modules (EventLogReader, JsonUtils, ForwarderAPI)
- Class relationships and dependencies
- Methods and attributes for each class
- Design patterns used

**View this to understand**:
- How components are structured
- What methods and properties each class has
- Relationships between components

### 2. [Sequence Diagram](sequence_diagram.md)
**Purpose**: Shows the runtime flow and interactions between components

**Contains**:
- Complete event forwarding lifecycle
- Initialization phase (logger setup, argument parsing)
- Connection phase (TCP connection with retry logic)
- Event monitoring phase (Windows Event Log subscription)
- Main event loop (read → format → send → log)
- Shutdown phase (graceful cleanup)
- Error handling and reconnection flows

**View this to understand**:
- How the application behaves at runtime
- The order of operations
- How components communicate
- Error handling and recovery

### 3. [Architecture Diagram](architecture_diagram.md)
**Purpose**: Shows the high-level system architecture and deployment

**Contains**:
- System architecture overview
- Component architecture (C4 model)
- Deployment view (Windows → Network → SIEM)
- Layer breakdown (Entry Point, API, Core Services, Utility)
- Technology stack
- File structure
- Design principles

**View this to understand**:
- Overall system design
- How components are organized into layers
- How the system deploys in production
- Technology choices

## Diagram Format

All diagrams use **Mermaid** syntax, which means:
- ✅ They render automatically on GitHub
- ✅ They work in VS Code with the [Mermaid Preview](https://marketplace.visualstudio.com/items?itemName=bierner.markdown-mermaid) extension
- ✅ They can be viewed at [mermaid.live](https://mermaid.live) (copy/paste the code blocks)
- ✅ They're version-controlled as text (no binary image files)
- ✅ They can be exported to PNG/SVG using [Mermaid CLI](https://github.com/mermaid-js/mermaid-cli)

## How to View

### On GitHub
Simply click on any of the `.md` files above - Mermaid diagrams render automatically in GitHub's markdown viewer.

### In VS Code
1. Install the "Markdown Preview Mermaid Support" extension
2. Open any diagram file
3. Press `Ctrl+Shift+V` (or `Cmd+Shift+V` on Mac) to preview

### Online
1. Copy the Mermaid code block from any diagram
2. Go to https://mermaid.live
3. Paste the code
4. View, edit, and export as PNG/SVG

## Quick Links

| Diagram | File | Purpose |
|---------|------|---------|
| **Class Diagram** | [class_diagram.md](class_diagram.md) | Understand the code structure |
| **Sequence Diagram** | [sequence_diagram.md](sequence_diagram.md) | Understand runtime behavior |
| **Architecture Diagram** | [architecture_diagram.md](architecture_diagram.md) | Understand system design |

## Updating Diagrams

When you make changes to the code:
1. Update the relevant diagram(s) to reflect changes
2. Keep diagrams in sync with implementation
3. Add new diagrams for new features as needed

## Additional Resources

- **Source Code**: `../src/` and `../inc/`
- **Build Script**: `../build.bat`
- **Runtime Logs**: `../forwarder_logs.csv` (generated when running)
