"""
Sensor modules for Sentinel Nano S1.

Provides:
- UWB ranging model (Two-Way Time of Flight)
- IMU motion model (dead reckoning)
- Barometric altimeter (vertical constraint)
"""

from .uwb_sensor import UWBRangingModel
from .imu_sensor import IMUMotionModel
from .barometer import BarometricAltimeter

__all__ = [
    'UWBRangingModel',
    'IMUMotionModel',
    'BarometricAltimeter'
]
