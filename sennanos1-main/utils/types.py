"""
Core data structures and type definitions for Sentinel Nano S1.

This module defines the fundamental types used throughout the localization system:
- State vectors (position and velocity)
- Anchor positions (fixed and mobile)
- Sensor measurements
- Tag identification
"""

import numpy as np
from dataclasses import dataclass
from typing import Optional
from enum import Enum


@dataclass
class State:
    """
    6-DOF state vector for a firefighter tag.
    
    State vector: x = [x, y, z, vx, vy, vz]^T
    - Position in building frame (meters)
    - Velocity in building frame (m/s)
    """
    position: np.ndarray  # [x, y, z] in meters
    velocity: np.ndarray  # [vx, vy, vz] in m/s
    timestamp: float       # Unix timestamp in seconds
    
    def __post_init__(self):
        """Validate dimensions of state components."""
        assert self.position.shape == (3,), "Position must be 3D vector"
        assert self.velocity.shape == (3,), "Velocity must be 3D vector"
    
    @property
    def vector(self) -> np.ndarray:
        """Return state as 6D column vector [x, y, z, vx, vy, vz]^T."""
        return np.concatenate([self.position, self.velocity])
    
    @staticmethod
    def from_vector(vec: np.ndarray, timestamp: float) -> 'State':
        """Construct State from 6D vector."""
        assert vec.shape == (6,), "State vector must be 6D"
        return State(
            position=vec[:3].copy(),
            velocity=vec[3:].copy(),
            timestamp=timestamp
        )
    
    def copy(self) -> 'State':
        """Create deep copy of state."""
        return State(
            position=self.position.copy(),
            velocity=self.velocity.copy(),
            timestamp=self.timestamp
        )


@dataclass
class Anchor:
    """
    UWB anchor with known 3D position.
    
    Anchors are assumed to have known positions in the building frame.
    Positions are surveyed a priori or established during deployment.
    """
    id: int               # Unique anchor identifier
    position: np.ndarray  # [x, y, z] in building frame (meters)
    is_mobile: bool = False  # True for drone, False for fixed anchors
    
    def __post_init__(self):
        """Validate anchor position dimension."""
        assert self.position.shape == (3,), "Anchor position must be 3D vector"
    
    def distance_to(self, point: np.ndarray) -> float:
        """
        Compute Euclidean distance from anchor to a point.
        
        Args:
            point: 3D position vector [x, y, z]
            
        Returns:
            Distance in meters
        """
        assert point.shape == (3,), "Point must be 3D vector"
        return np.linalg.norm(self.position - point)


@dataclass
class UWBMeasurement:
    """
    UWB ranging measurement from tag to anchor.
    
    Based on Two-Way Time of Flight (TW-ToF):
    d = (c/2) * (t_rx - t_tx)
    
    Noise model: d_measured = d_true + ε, ε ~ N(0, σ_d^2)
    """
    anchor_id: int        # ID of anchor that provided this range
    distance: float       # Measured distance in meters
    timestamp: float      # Measurement timestamp (seconds)
    variance: float       # Measurement variance σ_d^2
    is_valid: bool = True # False if measurement rejected (e.g., NLOS detection)
    
    def __post_init__(self):
        """Validate measurement values."""
        assert self.distance >= 0, "Distance must be non-negative"
        assert self.variance > 0, "Variance must be positive"


@dataclass
class IMUMeasurement:
    """
    Inertial measurement from onboard IMU.
    
    Provides specific force (acceleration minus gravity) and angular velocity.
    Used for dead reckoning between UWB updates.
    """
    acceleration: np.ndarray  # [ax, ay, az] in body frame (m/s^2)
    angular_velocity: np.ndarray  # [wx, wy, wz] in body frame (rad/s)
    timestamp: float          # Measurement timestamp (seconds)
    
    def __post_init__(self):
        """Validate IMU measurement dimensions."""
        assert self.acceleration.shape == (3,), "Acceleration must be 3D"
        assert self.angular_velocity.shape == (3,), "Angular velocity must be 3D"


@dataclass
class BarometerMeasurement:
    """
    Barometric pressure measurement for altitude estimation.
    
    Relates to altitude via:
    z = (RT/Mg) * ln(P0/P)
    
    Provides strong vertical constraint, reducing 3D problem to quasi-2.5D.
    """
    pressure: float       # Atmospheric pressure (Pascals)
    timestamp: float      # Measurement timestamp (seconds)
    reference_pressure: float  # Sea level or ground floor reference (Pascals)
    variance: float       # Measurement variance
    
    def __post_init__(self):
        """Validate barometer values."""
        assert self.pressure > 0, "Pressure must be positive"
        assert self.reference_pressure > 0, "Reference pressure must be positive"
        assert self.variance > 0, "Variance must be positive"


@dataclass
class Tag:
    """
    Wearable tag representing a firefighter.
    
    Each tag has unique ID and independent state estimation.
    Tags operate in TDMA slots to avoid collisions.
    """
    id: int               # Unique tag identifier (k in LaTeX)
    tdma_slot: int        # Assigned time slot for transmission
    state: State          # Current estimated state
    covariance: np.ndarray  # 6x6 state covariance matrix P
    last_uwb_update: float  # Timestamp of last UWB correction
    
    def __post_init__(self):
        """Validate tag state and covariance."""
        assert self.covariance.shape == (6, 6), "Covariance must be 6x6"
        assert np.allclose(self.covariance, self.covariance.T), "Covariance must be symmetric"


class ConfidenceLevel(Enum):
    """
    Confidence level for position estimate.
    
    Based on covariance-based confidence score:
    C = exp(-α * trace(P))
    """
    HIGH = "green"      # C > 0.7
    MEDIUM = "yellow"   # 0.4 < C ≤ 0.7
    LOW = "red"         # C ≤ 0.4


@dataclass
class PositionEstimate:
    """
    Position estimate with associated uncertainty and confidence.
    
    This is the output type for the tracking system.
    """
    tag_id: int           # Tag identifier
    state: State          # Estimated state
    covariance: np.ndarray  # 6x6 covariance matrix
    confidence: float     # Confidence score [0, 1]
    confidence_level: ConfidenceLevel  # Color-coded confidence
    num_anchors: int      # Number of anchors used
    gdop: Optional[float] # Geometric dilution of precision (if computed)
    
    def __post_init__(self):
        """Validate position estimate."""
        assert 0 <= self.confidence <= 1, "Confidence must be in [0, 1]"
        assert self.covariance.shape == (6, 6), "Covariance must be 6x6"
        assert self.num_anchors >= 0, "Number of anchors cannot be negative"
