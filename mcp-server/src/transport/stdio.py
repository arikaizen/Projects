"""stdio transport — for local subprocess servers that skip network auth.

Usage:
    python -m mcp_server.stdio

The parent process writes JSON-RPC 2.0 messages (newline-delimited) to
stdin and reads responses from stdout.  No Bearer token is required
because the subprocess is launched by the MCP client itself and
communication never leaves the host.
"""
import asyncio
import json
import sys

from ..jsonrpc import JsonRpcDispatcher
from ..tools.registry import ALL_HANDLERS


async def run_stdio() -> None:
    dispatcher = JsonRpcDispatcher()
    for name, handler in ALL_HANDLERS.items():
        dispatcher.register(name, handler)

    loop = asyncio.get_event_loop()
    reader = asyncio.StreamReader()
    protocol = asyncio.StreamReaderProtocol(reader)
    await loop.connect_read_pipe(lambda: protocol, sys.stdin)

    writer_transport, writer_protocol = await loop.connect_write_pipe(
        asyncio.BaseProtocol, sys.stdout
    )
    writer = asyncio.StreamWriter(writer_transport, writer_protocol, reader, loop)

    while True:
        try:
            line = await reader.readline()
            if not line:
                break
            body = json.loads(line.decode().strip())
            result = await dispatcher.dispatch(body)
            out = json.dumps(result) + "\n"
            writer.write(out.encode())
            await writer.drain()
        except json.JSONDecodeError as exc:
            err = json.dumps({
                "jsonrpc": "2.0", "id": None,
                "error": {"code": -32700, "message": f"Parse error: {exc}"},
            }) + "\n"
            writer.write(err.encode())
            await writer.drain()
        except Exception:
            break


if __name__ == "__main__":
    asyncio.run(run_stdio())
