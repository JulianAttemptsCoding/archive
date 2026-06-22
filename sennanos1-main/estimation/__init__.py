"""
Estimation modules for Sentinel Nano S1.

Provides:
- Kalman filter (sensor fusion)
- State predictor (IMU-based prediction)
- Confidence estimator (tracking quality)
- Multi-tag tracker (multiple firefighters)
"""

from .kalman_filter import KalmanFilter
from .state_predictor import StatePredictor
from .confidence import ConfidenceEstimator
from .multi_tag_tracker import MultiTagTracker

__all__ = [
    'KalmanFilter',
    'StatePredictor',
    'ConfidenceEstimator',
    'MultiTagTracker'
]
