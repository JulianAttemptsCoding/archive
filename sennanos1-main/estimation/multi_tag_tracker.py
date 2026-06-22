"""
Multi-tag tracker for managing multiple firefighters.

Maintains independent state estimates for each tag using:
- Per-tag Kalman filters
- Shared anchor infrastructure
- TDMA collision avoidance
- Confidence monitoring

Provides centralized tracking interface for the full system.
"""

import numpy as np
from typing import Dict, List, Optional
import logging
import time

from utils.types import (
    State, Tag, Anchor, UWBMeasurement, IMUMeasurement,
    BarometerMeasurement, PositionEstimate
)
from geometry.multilateration import MultilaterationSolver
from geometry.anchor_geometry import AnchorGeometry
from estimation.kalman_filter import KalmanFilter
from estimation.state_predictor import StatePredictor
from estimation.confidence import ConfidenceEstimator
from sensors.barometer import BarometricAltimeter
from utils.constants import TDMA_SLOT_DURATION

logger = logging.getLogger(__name__)


class MultiTagTracker:
    """
    Tracks multiple firefighters simultaneously.
    
    Each tag has independent state, but shares:
    - Anchor infrastructure
    - Multilateration solver
    - Confidence estimator
    """
    
    def __init__(
        self,
        anchors: List[Anchor],
        initial_covariance: Optional[np.ndarray] = None
    ):
        """
        Initialize multi-tag tracker.
        
        Args:
            anchors: List of fixed and mobile anchors
            initial_covariance: Default initial covariance for new tags
        """
        self.anchors = {anchor.id: anchor for anchor in anchors}
        
        # Default initial covariance (moderate uncertainty)
        if initial_covariance is None:
            self.initial_covariance = np.diag([4.0, 4.0, 4.0, 1.0, 1.0, 1.0])  # [m^2, m^2, m^2, (m/s)^2, ...]
        else:
            self.initial_covariance = initial_covariance
        
        # Per-tag tracking state
        self.tags: Dict[int, Tag] = {}
        self.kalman_filters: Dict[int, KalmanFilter] = {}
        
        # Shared modules
        self.multilateration_solver = MultilaterationSolver()
        self.anchor_geometry = AnchorGeometry()
        self.state_predictor = StatePredictor()
        self.confidence_estimator = ConfidenceEstimator()
        self.barometric_altimeter = BarometricAltimeter()
        
        logger.info(f"Multi-tag tracker initialized with {len(anchors)} anchors")
    
    def add_tag(
        self,
        tag_id: int,
        initial_position: np.ndarray,
        initial_velocity: np.ndarray = None,
        tdma_slot: int = 0
    ) -> Tag:
        """
        Add new tag to tracking system.
        
        Args:
            tag_id: Unique tag identifier
            initial_position: Initial position [x, y, z]
            initial_velocity: Initial velocity [vx, vy, vz], default zero
            tdma_slot: Assigned TDMA slot for this tag
            
        Returns:
            Created Tag object
        """
        if tag_id in self.tags:
            logger.warning(f"Tag {tag_id} already exists, resetting")
            self.remove_tag(tag_id)
        
        # Default velocity to zero if not provided
        if initial_velocity is None:
            initial_velocity = np.zeros(3)
        
        # Create initial state
        initial_state = State(
            position=initial_position.copy(),
            velocity=initial_velocity.copy(),
            timestamp=time.time()
        )
        
        # Create tag
        tag = Tag(
            id=tag_id,
            tdma_slot=tdma_slot,
            state=initial_state,
            covariance=self.initial_covariance.copy(),
            last_uwb_update=time.time()
        )
        
        # Create Kalman filter for this tag
        kf = KalmanFilter(
            initial_state=initial_state,
            initial_covariance=self.initial_covariance.copy()
        )
        
        # Store
        self.tags[tag_id] = tag
        self.kalman_filters[tag_id] = kf
        
        logger.info(f"Added tag {tag_id} at position {initial_position}, slot {tdma_slot}")
        
        return tag
    
    def remove_tag(self, tag_id: int):
        """
        Remove tag from tracking system.
        
        Args:
            tag_id: Tag to remove
        """
        if tag_id in self.tags:
            del self.tags[tag_id]
            del self.kalman_filters[tag_id]
            logger.info(f"Removed tag {tag_id}")
        else:
            logger.warning(f"Tag {tag_id} not found")
    
    def update_anchor_position(self, anchor_id: int, position: np.ndarray):
        """
        Update anchor position (for mobile anchors like drone).
        
        Args:
            anchor_id: Anchor identifier
            position: New position [x, y, z]
        """
        if anchor_id in self.anchors:
            self.anchors[anchor_id].position = position.copy()
            logger.debug(f"Updated anchor {anchor_id} position to {position}")
        else:
            logger.warning(f"Anchor {anchor_id} not found")
    
    def add_anchor(self, anchor: Anchor):
        """
        Add new anchor to system.
        
        Args:
            anchor: Anchor object
        """
        self.anchors[anchor.id] = anchor
        logger.info(f"Added anchor {anchor.id} at position {anchor.position}")
    
    def remove_anchor(self, anchor_id: int):
        """
        Remove anchor from system.
        
        Args:
            anchor_id: Anchor to remove
        """
        if anchor_id in self.anchors:
            del self.anchors[anchor_id]
            logger.info(f"Removed anchor {anchor_id}")
        else:
            logger.warning(f"Anchor {anchor_id} not found")
    
    def predict(
        self,
        tag_id: int,
        imu_measurement: Optional[IMUMeasurement] = None,
        dt: Optional[float] = None
    ) -> Optional[State]:
        """
        Predict tag state using IMU or constant velocity.
        
        Args:
            tag_id: Tag identifier
            imu_measurement: IMU measurement (if available)
            dt: Time step (if no IMU, use constant velocity)
            
        Returns:
            Predicted state
        """
        if tag_id not in self.tags:
            logger.error(f"Tag {tag_id} not found")
            return None
        
        kf = self.kalman_filters[tag_id]
        current_state = kf.get_state()
        
        # Predict based on available information
        if imu_measurement is not None:
            # Use IMU for prediction
            predicted_state, F, Q = self.state_predictor.predict_from_imu(
                current_state, imu_measurement
            )
            timestamp = imu_measurement.timestamp
        elif dt is not None:
            # Constant velocity prediction
            predicted_state, F, Q = self.state_predictor.predict_constant_velocity(
                current_state, dt
            )
            timestamp = current_state.timestamp + dt
        else:
            logger.error("Must provide either IMU measurement or time step")
            return current_state
        
        # Update Kalman filter
        kf.predict(F, Q, timestamp)
        
        # Update tag state
        self.tags[tag_id].state = predicted_state
        self.tags[tag_id].covariance = kf.get_covariance()
        
        return predicted_state
    
    def correct_with_uwb(
        self,
        tag_id: int,
        measurements: List[UWBMeasurement],
        timestamp: float
    ) -> Optional[State]:
        """
        Correct tag state using UWB measurements.
        
        Args:
            tag_id: Tag identifier
            measurements: List of UWB measurements to anchors
            timestamp: Measurement timestamp
            
        Returns:
            Corrected state, or None if correction fails
        """
        if tag_id not in self.tags:
            logger.error(f"Tag {tag_id} not found")
            return None
        
        # Get corresponding anchors
        anchors = []
        for meas in measurements:
            if meas.anchor_id in self.anchors:
                anchors.append(self.anchors[meas.anchor_id])
            else:
                logger.warning(f"Measurement for unknown anchor {meas.anchor_id}")
        
        if len(anchors) != len(measurements):
            logger.warning("Anchor-measurement mismatch, using valid subset")
        
        # Solve multilateration
        position, position_cov, info = self.multilateration_solver.solve(
            anchors, measurements, initial_guess=self.tags[tag_id].state.position
        )
        
        if position is None:
            logger.warning(f"Multilateration failed for tag {tag_id}")
            return None
        
        # Create measurement update for Kalman filter
        # Measurement: position only [x, y, z]
        z = position
        
        # Measurement matrix: H = [I_3, 0_3]
        H = np.zeros((3, 6))
        H[:3, :3] = np.eye(3)
        
        # Measurement covariance from multilateration
        # Expand to include only position uncertainty
        R = position_cov
        
        # Apply Kalman update
        kf = self.kalman_filters[tag_id]
        updated_state, updated_covariance = kf.update(z, H, R, timestamp)
        
        # Update tag
        self.tags[tag_id].state = updated_state
        self.tags[tag_id].covariance = updated_covariance
        self.tags[tag_id].last_uwb_update = timestamp
        
        logger.debug(f"UWB correction for tag {tag_id}: pos={updated_state.position}, residual={info.get('mean_residual', 0):.3f}m")
        
        return updated_state
    
    def correct_with_barometer(
        self,
        tag_id: int,
        measurement: BarometerMeasurement
    ) -> Optional[State]:
        """
        Correct vertical state using barometer.
        
        Args:
            tag_id: Tag identifier
            measurement: Barometer measurement
            
        Returns:
            Corrected state
        """
        if tag_id not in self.tags:
            logger.error(f"Tag {tag_id} not found")
            return None
        
        # Convert pressure to altitude
        altitude, altitude_variance = self.barometric_altimeter.estimate_altitude(measurement)
        
        # Measurement update
        z = np.array([altitude])
        
        # Measurement matrix: observes z position only
        H, _ = self.barometric_altimeter.create_measurement_matrix()
        
        # Measurement noise
        R = np.array([[altitude_variance]])
        
        # Kalman update
        kf = self.kalman_filters[tag_id]
        updated_state, updated_covariance = kf.update(z, H, R, measurement.timestamp)
        
        # Update tag
        self.tags[tag_id].state = updated_state
        self.tags[tag_id].covariance = updated_covariance
        
        logger.debug(f"Barometer correction for tag {tag_id}: altitude={altitude:.2f}m")
        
        return updated_state
    
    def get_position_estimate(self, tag_id: int) -> Optional[PositionEstimate]:
        """
        Get full position estimate with confidence for a tag.
        
        Args:
            tag_id: Tag identifier
            
        Returns:
            PositionEstimate object, or None if tag not found
        """
        if tag_id not in self.tags:
            logger.error(f"Tag {tag_id} not found")
            return None
        
        tag = self.tags[tag_id]
        
        # Compute confidence
        time_since_update = time.time() - tag.last_uwb_update
        
        # Get anchors used (all visible anchors)
        visible_anchors = list(self.anchors.values())
        num_anchors = len(visible_anchors)
        
        # Compute GDOP
        gdop = self.anchor_geometry.compute_gdop(visible_anchors, tag.state.position)
        
        # Compute confidence
        confidence = self.confidence_estimator.compute_confidence(
            covariance=tag.covariance,
            num_anchors=num_anchors,
            time_since_update=time_since_update,
            gdop=gdop
        )
        
        confidence_level = self.confidence_estimator.classify_confidence(confidence)
        
        # Create position estimate
        estimate = PositionEstimate(
            tag_id=tag_id,
            state=tag.state.copy(),
            covariance=tag.covariance.copy(),
            confidence=confidence,
            confidence_level=confidence_level,
            num_anchors=num_anchors,
            gdop=gdop
        )
        
        return estimate
    
    def get_all_position_estimates(self) -> List[PositionEstimate]:
        """
        Get position estimates for all tracked tags.
        
        Returns:
            List of PositionEstimate objects
        """
        estimates = []
        for tag_id in self.tags.keys():
            estimate = self.get_position_estimate(tag_id)
            if estimate is not None:
                estimates.append(estimate)
        
        return estimates
    
    def process_measurement_batch(
        self,
        tag_id: int,
        uwb_measurements: Optional[List[UWBMeasurement]] = None,
        imu_measurement: Optional[IMUMeasurement] = None,
        barometer_measurement: Optional[BarometerMeasurement] = None,
        timestamp: Optional[float] = None
    ) -> Optional[PositionEstimate]:
        """
        Process a batch of measurements for a tag.
        
        Typical pipeline:
        1. Predict with IMU (if available)
        2. Correct with UWB (if available)
        3. Correct with barometer (if available)
        
        Args:
            tag_id: Tag identifier
            uwb_measurements: UWB measurements
            imu_measurement: IMU measurement
            barometer_measurement: Barometer measurement
            timestamp: Current timestamp
            
        Returns:
            Final position estimate
        """
        if timestamp is None:
            timestamp = time.time()
        
        # 1. Predict with IMU
        if imu_measurement is not None:
            self.predict(tag_id, imu_measurement=imu_measurement)
        
        # 2. Correct with UWB
        if uwb_measurements is not None and len(uwb_measurements) > 0:
            self.correct_with_uwb(tag_id, uwb_measurements, timestamp)
        
        # 3. Correct with barometer
        if barometer_measurement is not None:
            self.correct_with_barometer(tag_id, barometer_measurement)
        
        # Return final estimate
        return self.get_position_estimate(tag_id)
    
    def get_tracking_summary(self) -> dict:
        """
        Get summary of tracking status for all tags.
        
        Returns:
            Dictionary with tracking statistics
        """
        summary = {
            "num_tags": len(self.tags),
            "num_anchors": len(self.anchors),
            "tags": {}
        }
        
        for tag_id, tag in self.tags.items():
            estimate = self.get_position_estimate(tag_id)
            
            if estimate is not None:
                summary["tags"][tag_id] = {
                    "position": tag.state.position.tolist(),
                    "velocity": tag.state.velocity.tolist(),
                    "confidence": estimate.confidence,
                    "confidence_level": estimate.confidence_level.value,
                    "time_since_update": time.time() - tag.last_uwb_update,
                    "gdop": estimate.gdop
                }
        
        return summary
