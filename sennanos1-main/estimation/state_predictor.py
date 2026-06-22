"""
State predictor module for IMU-based dead reckoning.

Provides high-level interface for predicting tag state between
UWB measurement updates using IMU data.

Combines:
- IMU motion model
- Kalman filter prediction
- Process noise computation
"""

import numpy as np
from typing import Tuple, Optional
import logging

from utils.types import State, IMUMeasurement
from sensors.imu_sensor import IMUMotionModel
from utils.constants import get_process_noise_matrix

logger = logging.getLogger(__name__)


class StatePredictor:
    """
    Predicts tag state using IMU measurements.
    
    Wraps IMU motion model for use in tracking pipeline.
    """
    
    def __init__(self):
        """Initialize state predictor."""
        self.imu_model = IMUMotionModel()
        logger.debug("State predictor initialized")
    
    def predict_from_imu(
        self,
        current_state: State,
        imu_measurement: IMUMeasurement
    ) -> Tuple[State, np.ndarray, np.ndarray]:
        """
        Predict next state from IMU measurement.
        
        Args:
            current_state: Current state estimate
            imu_measurement: IMU measurement
            
        Returns:
            predicted_state: Predicted state
            F: State transition matrix
            Q: Process noise covariance
        """
        # Compute time step
        dt = imu_measurement.timestamp - current_state.timestamp
        
        if dt <= 0:
            logger.warning(f"Invalid time step: dt={dt:.3f}s")
            dt = 0.01  # Use small default
        
        # Predict using IMU motion model
        predicted_state, Q = self.imu_model.predict(
            current_state,
            imu_measurement,
            dt
        )
        
        # Get state transition matrix
        F = self.imu_model._get_state_transition_matrix(dt)
        
        return predicted_state, F, Q
    
    def predict_constant_velocity(
        self,
        current_state: State,
        dt: float
    ) -> Tuple[State, np.ndarray, np.ndarray]:
        """
        Predict next state assuming constant velocity.
        
        Used when IMU measurements are unavailable.
        
        Args:
            current_state: Current state
            dt: Time step (seconds)
            
        Returns:
            predicted_state: Predicted state
            F: State transition matrix
            Q: Process noise covariance
        """
        # State transition matrix (constant velocity)
        F = self.imu_model._get_state_transition_matrix(dt)
        
        # Predict state: x_{k+1} = F * x_k
        predicted_vector = F @ current_state.vector
        predicted_state = State.from_vector(
            predicted_vector,
            timestamp=current_state.timestamp + dt
        )
        
        # Process noise (larger uncertainty without IMU)
        Q = get_process_noise_matrix(dt) * 2.0  # Inflate uncertainty
        
        logger.debug("Constant velocity prediction (no IMU)")
        
        return predicted_state, F, Q
    
    def predict_with_acceleration(
        self,
        current_state: State,
        acceleration: np.ndarray,
        dt: float,
        timestamp: float
    ) -> Tuple[State, np.ndarray, np.ndarray]:
        """
        Predict state with known acceleration.
        
        Args:
            current_state: Current state
            acceleration: Acceleration vector [ax, ay, az] (m/s^2)
            dt: Time step (seconds)
            timestamp: Target timestamp
            
        Returns:
            predicted_state: Predicted state
            F: State transition matrix
            Q: Process noise covariance
        """
        # Get matrices
        F = self.imu_model._get_state_transition_matrix(dt)
        G = self.imu_model._get_acceleration_matrix(dt)
        
        # Predict: x_{k+1} = F * x_k + G * a
        predicted_vector = F @ current_state.vector + G @ acceleration
        predicted_state = State.from_vector(predicted_vector, timestamp)
        
        # Process noise
        Q = self.imu_model._compute_process_noise(dt, G)
        
        return predicted_state, F, Q
    
    def predict_multi_step(
        self,
        initial_state: State,
        imu_measurements: list,
        dt: float
    ) -> list:
        """
        Predict trajectory over multiple IMU measurements.
        
        Useful for dead reckoning during UWB outages.
        
        Args:
            initial_state: Starting state
            imu_measurements: List of IMU measurements
            dt: Time step between measurements
            
        Returns:
            List of predicted states
        """
        states = [initial_state]
        current_state = initial_state
        
        for imu_meas in imu_measurements:
            predicted_state, _, _ = self.predict_from_imu(current_state, imu_meas)
            states.append(predicted_state)
            current_state = predicted_state
        
        return states
    
    def estimate_prediction_uncertainty(
        self,
        dt: float,
        num_steps: int = 1
    ) -> float:
        """
        Estimate position uncertainty after prediction.
        
        Useful for determining when UWB update is needed.
        
        Args:
            dt: Time step per prediction
            num_steps: Number of prediction steps
            
        Returns:
            Expected position uncertainty (meters)
        """
        # Covariance grows with each prediction step
        # Simplified: assume independence and add variances
        Q = get_process_noise_matrix(dt)
        position_variance = np.trace(Q[:3, :3])
        
        # Accumulate over steps
        total_variance = position_variance * num_steps
        uncertainty = np.sqrt(total_variance)
        
        return uncertainty
    
    def detect_motion_mode(
        self,
        imu_measurement: IMUMeasurement,
        stationary_threshold: float = 0.5,
        running_threshold: float = 3.0
    ) -> str:
        """
        Detect firefighter motion mode from IMU.
        
        Modes:
        - "stationary": Not moving
        - "walking": Normal walking
        - "running": Fast motion or climbing
        
        Args:
            imu_measurement: IMU measurement
            stationary_threshold: Acceleration threshold for stationary (m/s^2)
            running_threshold: Acceleration threshold for running (m/s^2)
            
        Returns:
            Motion mode string
        """
        accel_magnitude = np.linalg.norm(imu_measurement.acceleration)
        
        if accel_magnitude < stationary_threshold:
            return "stationary"
        elif accel_magnitude > running_threshold:
            return "running"
        else:
            return "walking"
    
    def check_prediction_validity(
        self,
        predicted_state: State,
        time_since_update: float,
        max_time: float = 10.0,
        max_velocity: float = 10.0
    ) -> bool:
        """
        Check if predicted state is still reliable.
        
        Prediction degrades over time without corrections.
        
        Args:
            predicted_state: Predicted state
            time_since_update: Time since last UWB update (seconds)
            max_time: Maximum time for reliable prediction
            max_velocity: Maximum reasonable velocity (m/s)
            
        Returns:
            True if prediction is still valid
        """
        # Check time since update
        if time_since_update > max_time:
            logger.warning(f"Prediction stale: {time_since_update:.1f}s since update")
            return False
        
        # Check velocity magnitude
        velocity_magnitude = np.linalg.norm(predicted_state.velocity)
        if velocity_magnitude > max_velocity:
            logger.warning(f"Unreasonable velocity: {velocity_magnitude:.1f} m/s")
            return False
        
        return True
