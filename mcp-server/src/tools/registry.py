"""Central tool registry — merges tool definitions and handlers from all tool modules."""
from .graph_tools import HANDLERS as GRAPH_HANDLERS, TOOL_DEFINITIONS as GRAPH_DEFS
from .siem_tools import HANDLERS as SIEM_HANDLERS, TOOL_DEFINITIONS as SIEM_DEFS
from .asset_tools import HANDLERS as ASSET_HANDLERS, TOOL_DEFINITIONS as ASSET_DEFS

# Combined schema list returned by tools/list
TOOL_SCHEMAS: list[dict] = GRAPH_DEFS + SIEM_DEFS + ASSET_DEFS

# Combined handler map used by tools/call dispatch
ALL_HANDLERS: dict = {**GRAPH_HANDLERS, **SIEM_HANDLERS, **ASSET_HANDLERS}
