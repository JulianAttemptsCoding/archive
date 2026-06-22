"""
Utility modules for Sentinel Nano S1.

Provides:
- Type definitions (State, Anchor, Measurement, etc.)
- Physical constants and parameters
"""

from .types import (
    State,
    Anchor,
    UWBMeasurement,
    IMUMeasurement,
    BarometerMeasurement,
    Tag,
    ConfidenceLevel,
    PositionEstimate
)

from .constants import (
    SPEED_OF_LIGHT,
    UWB_RANGE_VARIANCE,
    IMU_ACCEL_VARIANCE,
    BAROMETER_VARIANCE,
    MIN_ANCHORS_3D,
    CONFIDENCE_DECAY_ALPHA
)

__all__ = [
    # Types
    'State',
    'Anchor',
    'UWBMeasurement',
    'IMUMeasurement',
    'BarometerMeasurement',
    'Tag',
    'ConfidenceLevel',
    'PositionEstimate',
    # Constants
    'SPEED_OF_LIGHT',
    'UWB_RANGE_VARIANCE',
    'IMU_ACCEL_VARIANCE',
    'BAROMETER_VARIANCE',
    'MIN_ANCHORS_3D',
    'CONFIDENCE_DECAY_ALPHA'
]
