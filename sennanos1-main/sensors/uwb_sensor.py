"""
UWB sensor model and ranging physics.

Implements Two-Way Time of Flight (TW-ToF) ranging from LaTeX Section 3:

    d = (c/2) * (t_rx - t_tx)

Measurement noise model:
    d_measured = d_true + ε, where ε ~ N(0, σ_d^2)

Handles:
- Distance computation from time-of-flight
- NLOS (Non-Line-of-Sight) detection
- Measurement validation
- Outlier rejection
"""

import numpy as np
from typing import List, Optional, Tuple
import logging

from utils.types import Anchor, UWBMeasurement
from utils.constants import (
    SPEED_OF_LIGHT,
    UWB_RANGE_VARIANCE,
    MAX_ANCHOR_RANGE
)

logger = logging.getLogger(__name__)


class UWBRangingModel:
    """
    Models UWB ranging physics and measurement characteristics.
    """
    
    def __init__(
        self,
        range_variance: float = UWB_RANGE_VARIANCE,
        max_range: float = MAX_ANCHOR_RANGE
    ):
        """
        Initialize UWB ranging model.
        
        Args:
            range_variance: Measurement noise variance (m^2)
            max_range: Maximum valid range (meters)
        """
        self.range_variance = range_variance
        self.max_range = max_range
        self.speed_of_light = SPEED_OF_LIGHT
    
    def time_to_distance(self, time_of_flight: float) -> float:
        """
        Convert time-of-flight to distance.
        
        From LaTeX Section 3:
            d = c/2 * (t_rx - t_tx)
        
        Factor of 1/2 accounts for round-trip time.
        
        Args:
            time_of_flight: Round-trip time (seconds)
            
        Returns:
            Distance (meters)
        """
        distance = (self.speed_of_light / 2.0) * time_of_flight
        return distance
    
    def distance_to_time(self, distance: float) -> float:
        """
        Convert distance to expected time-of-flight.
        
        Inverse of time_to_distance.
        
        Args:
            distance: Distance (meters)
            
        Returns:
            Round-trip time-of-flight (seconds)
        """
        time_of_flight = (2.0 * distance) / self.speed_of_light
        return time_of_flight
    
    def create_measurement(
        self,
        anchor_id: int,
        distance: float,
        timestamp: float,
        variance: Optional[float] = None
    ) -> UWBMeasurement:
        """
        Create UWBMeasurement object with validation.
        
        Args:
            anchor_id: Anchor identifier
            distance: Measured distance (meters)
            timestamp: Measurement time (seconds)
            variance: Measurement variance (m^2), uses default if None
            
        Returns:
            UWBMeasurement object
        """
        if variance is None:
            variance = self.range_variance
        
        # Validate measurement
        is_valid = self.validate_measurement(distance)
        
        measurement = UWBMeasurement(
            anchor_id=anchor_id,
            distance=distance,
            timestamp=timestamp,
            variance=variance,
            is_valid=is_valid
        )
        
        return measurement
    
    def validate_measurement(self, distance: float) -> bool:
        """
        Validate UWB distance measurement.
        
        Reject measurements that are:
        - Negative
        - Beyond maximum range
        - NaN or Inf
        
        Args:
            distance: Measured distance (meters)
            
        Returns:
            True if measurement is valid
        """
        if not np.isfinite(distance):
            logger.warning("Distance is NaN or Inf")
            return False
        
        if distance < 0:
            logger.warning(f"Negative distance: {distance:.2f}m")
            return False
        
        if distance > self.max_range:
            logger.warning(f"Distance {distance:.2f}m exceeds max range {self.max_range:.2f}m")
            return False
        
        return True
    
    def detect_nlos(
        self,
        measurements: List[UWBMeasurement],
        anchors: List[Anchor],
        estimated_position: np.ndarray,
        threshold: float = 2.0
    ) -> List[bool]:
        """
        Detect Non-Line-of-Sight (NLOS) measurements.
        
        NLOS occurs when signal reflects off obstacles, causing
        distance overestimation.
        
        Detection heuristic: measured distance > true distance + threshold
        
        Args:
            measurements: List of UWB measurements
            anchors: List of corresponding anchors
            estimated_position: Current position estimate
            threshold: NLOS detection threshold (meters)
            
        Returns:
            List of booleans indicating NLOS for each measurement
        """
        nlos_flags = []
        
        for measurement, anchor in zip(measurements, anchors):
            # Compute expected distance
            expected_dist = np.linalg.norm(estimated_position - anchor.position)
            
            # Residual
            residual = measurement.distance - expected_dist
            
            # NLOS typically causes positive bias
            is_nlos = residual > threshold
            
            if is_nlos:
                logger.debug(f"NLOS detected for anchor {anchor.id}: residual={residual:.2f}m")
            
            nlos_flags.append(is_nlos)
        
        return nlos_flags
    
    def reject_outliers(
        self,
        measurements: List[UWBMeasurement],
        anchors: List[Anchor],
        estimated_position: np.ndarray,
        threshold_sigma: float = 3.0
    ) -> List[UWBMeasurement]:
        """
        Reject outlier measurements using statistical test.
        
        Removes measurements that are >threshold_sigma standard deviations
        from expected distance.
        
        Args:
            measurements: List of measurements
            anchors: List of anchors
            estimated_position: Current position estimate
            threshold_sigma: Number of standard deviations for rejection
            
        Returns:
            List of measurements with outliers marked as invalid
        """
        filtered_measurements = []
        
        for measurement, anchor in zip(measurements, anchors):
            # Compute expected distance and residual
            expected_dist = np.linalg.norm(estimated_position - anchor.position)
            residual = abs(measurement.distance - expected_dist)
            
            # Compute threshold based on measurement variance
            threshold = threshold_sigma * np.sqrt(measurement.variance)
            
            # Mark as invalid if residual too large
            if residual > threshold:
                logger.debug(f"Outlier rejected for anchor {anchor.id}: residual={residual:.2f}m > {threshold:.2f}m")
                measurement.is_valid = False
            
            filtered_measurements.append(measurement)
        
        return filtered_measurements
    
    def estimate_nlos_bias(
        self,
        measurements: List[UWBMeasurement],
        anchors: List[Anchor],
        true_position: np.ndarray
    ) -> List[float]:
        """
        Estimate NLOS bias for each measurement.
        
        NLOS bias = measured distance - true distance
        
        Useful for analysis and calibration (requires known position).
        
        Args:
            measurements: List of measurements
            anchors: List of anchors
            true_position: Known true position
            
        Returns:
            List of bias values (meters)
        """
        biases = []
        
        for measurement, anchor in zip(measurements, anchors):
            true_dist = np.linalg.norm(true_position - anchor.position)
            bias = measurement.distance - true_dist
            biases.append(bias)
        
        return biases
    
    def apply_ranging_correction(
        self,
        distance_raw: float,
        anchor: Anchor,
        tag_position: np.ndarray
    ) -> float:
        """
        Apply calibration correction to raw distance.
        
        Can include:
        - Antenna delay correction
        - Environmental effects
        - NLOS mitigation
        
        Simplified implementation: identity (no correction).
        
        Args:
            distance_raw: Raw measured distance
            anchor: Anchor object
            tag_position: Estimated tag position
            
        Returns:
            Corrected distance
        """
        # In production, would apply learned corrections here
        # For now, return raw distance
        distance_corrected = distance_raw
        
        return distance_corrected
    
    def compute_cramer_rao_bound(
        self,
        anchors: List[Anchor],
        tag_position: np.ndarray
    ) -> np.ndarray:
        """
        Compute Cramér-Rao lower bound on position estimation.
        
        Theoretical best-case performance given anchor geometry
        and measurement noise.
        
        CRLB = (J^T J)^{-1}
        
        where J is Fisher information matrix.
        
        Args:
            anchors: List of anchors
            tag_position: Tag position
            
        Returns:
            3x3 covariance lower bound
        """
        N = len(anchors)
        
        # Build Fisher information matrix
        J = np.zeros((3, 3))
        
        for anchor in anchors:
            diff = tag_position - anchor.position
            dist = np.linalg.norm(diff)
            
            if dist < 1e-3:
                continue
            
            # Unit vector
            u = diff / dist
            
            # Add contribution: J += (1/σ^2) * u * u^T
            J += (1.0 / self.range_variance) * np.outer(u, u)
        
        # CRLB = J^{-1}
        try:
            crlb = np.linalg.inv(J)
        except np.linalg.LinAlgError:
            logger.error("Singular Fisher information matrix")
            crlb = np.eye(3) * np.inf
        
        return crlb
    
    def simulate_measurement(
        self,
        anchor: Anchor,
        true_position: np.ndarray,
        add_noise: bool = True,
        nlos_bias: float = 0.0
    ) -> UWBMeasurement:
        """
        Simulate UWB measurement for testing.
        
        Args:
            anchor: Anchor object
            true_position: True tag position
            add_noise: Whether to add measurement noise
            nlos_bias: Additional NLOS bias (meters)
            
        Returns:
            Simulated UWBMeasurement
        """
        # True distance
        true_distance = anchor.distance_to(true_position)
        
        # Add noise and bias
        noise = 0.0
        if add_noise:
            noise = np.random.normal(0, np.sqrt(self.range_variance))
        
        measured_distance = true_distance + noise + nlos_bias
        
        # Create measurement
        measurement = self.create_measurement(
            anchor_id=anchor.id,
            distance=measured_distance,
            timestamp=0.0
        )
        
        return measurement
