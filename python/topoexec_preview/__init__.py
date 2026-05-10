"""TopoExec Python preview client.

This package is an unstable, CLI-backed helper for config/test automation. It is
not a native extension and does not provide a high-throughput payload path.
"""

from .client import (
    CommandResult,
    GraphDocument,
    TopoExecClient,
    TopoExecCommandError,
    TopoExecError,
)

__all__ = [
    "CommandResult",
    "GraphDocument",
    "TopoExecClient",
    "TopoExecCommandError",
    "TopoExecError",
]
