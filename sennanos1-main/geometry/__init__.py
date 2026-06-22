"""
Geometry modules for Sentinel Nano S1.

Provides:
- Multilateration solver (position from distances)
- Anchor geometry analysis (GDOP, diversity, etc.)
"""

from .multilateration import MultilaterationSolver
from .anchor_geometry import AnchorGeometry

__all__ = [
    'MultilaterationSolver',
    'AnchorGeometry'
]
