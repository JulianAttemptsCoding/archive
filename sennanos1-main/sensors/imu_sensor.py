"""
IMU sensor model and motion integration.

Implements discrete-time kinematic model from LaTeX Section 5:

    x_{k+1} = F * x_k + G * a_k

where:
    F = state transition matrix (position-velocity coupling)
    G = acceleration input matrix
    a_k = measured acceleration (with noise)

Handles:
- Dead reckoning between UWB updates
- Acceleration integration
- Noise modeling
"""

import numpy as np
from typing import Tuple
import logging

from utils.types import IMUMeasurement, State
from utils.constants import (
    IMU_ACCEL_VARIANCE,
    IMU_GYRO_VARIANCE,
    GRAVITY
)

logger = logging.getLogger(__name__)


class IMUMotionModel:
    """
    IMU-based motion model for dead reckoning.
    
    Integrates acceleration to predict position and velocity
    between UWB measurement updates.
    """
    
    def __init__(
        self,
        accel_noise_variance: float = IMU_ACCEL_VARIANCE,
        gravity_vector: np.ndarray = np.array([0, 0, -GRAVITY])
    ):
        """
        Initialize IMU motion model.
        
        Args:
            accel_noise_variance: Acceleration measurement noise variance
            gravity_vector: Gravity vector in navigation frame (m/s^2)
        """
        self.accel_noise_variance = accel_noise_variance
        self.gravity_vector = gravity_vector
    
    def predict(
        self,
        state: State,
        imu_measurement: IMUMeasurement,
        dt: float
    ) -> Tuple[State, np.ndarray]:
        """
        Predict next state using IMU measurement.
        
        From LaTeX Section 5:
            x_{k+1} = [I_3, Δt*I_3; 0, I_3] * x_k + [0.5*Δt^2*I_3; Δt*I_3] * a_k
        
        Args:
            state: Current state
            imu_measurement: IMU acceleration measurement
            dt: Time step (seconds)
            
        Returns:
            predicted_state: Predicted state after dt
            process_noise: Process noise covariance Q
        """
        # State transition matrix F
        F = self._get_state_transition_matrix(dt)
        
        # Acceleration input matrix G
        G = self._get_acceleration_matrix(dt)
        
        # Transform acceleration from body frame to navigation frame
        # Simplified: assume body frame ≈ navigation frame (no rotation)
        # In full implementation, would use orientation quaternion
        accel_nav = imu_measurement.acceleration.copy()
        
        # Remove gravity (IMU measures specific force, not acceleration)
        accel_nav = accel_nav - self.gravity_vector
        
        # Predict state: x_{k+1} = F*x_k + G*a_k
        current_vector = state.vector
        predicted_vector = F @ current_vector + G @ accel_nav
        
        # Create predicted state
        predicted_state = State.from_vector(
            predicted_vector,
            timestamp=imu_measurement.timestamp
        )
        
        # Compute process noise covariance
        Q = self._compute_process_noise(dt, G)
        
        return predicted_state, Q
    
    def _get_state_transition_matrix(self, dt: float) -> np.ndarray:
        """
        Construct 6x6 state transition matrix F.
        
        From LaTeX:
            F = [I_3, Δt*I_3]
                [0,   I_3    ]
        
        This models constant-velocity motion with acceleration input.
        
        Args:
            dt: Time step (seconds)
            
        Returns:
            6x6 state transition matrix
        """
        F = np.eye(6)
        
        # Position-velocity coupling: p_{k+1} = p_k + v_k * dt
        F[0, 3] = dt
        F[1, 4] = dt
        F[2, 5] = dt
        
        return F
    
    def _get_acceleration_matrix(self, dt: float) -> np.ndarray:
        """
        Construct 6x3 acceleration input matrix G.
        
        From LaTeX:
            G = [0.5*Δt^2 * I_3]
                [Δt * I_3      ]
        
        Maps acceleration to position and velocity changes.
        
        Args:
            dt: Time step (seconds)
            
        Returns:
            6x3 acceleration matrix
        """
        G = np.zeros((6, 3))
        
        # Position update from acceleration: p += 0.5 * a * dt^2
        G[0:3, 0:3] = 0.5 * dt**2 * np.eye(3)
        
        # Velocity update from acceleration: v += a * dt
        G[3:6, 0:3] = dt * np.eye(3)
        
        return G
    
    def _compute_process_noise(self, dt: float, G: np.ndarray) -> np.ndarray:
        """
        Compute process noise covariance matrix Q.
        
        Accounts for uncertainty in IMU measurements propagating through
        the kinematic model.
        
        From continuous-discrete conversion:
            Q = G * σ_a^2 * G^T
        
        Args:
            dt: Time step (seconds)
            G: Acceleration matrix (6x3)
            
        Returns:
            6x6 process noise covariance
        """
        # Acceleration noise covariance (3x3)
        Q_accel = self.accel_noise_variance * np.eye(3)
        
        # Propagate through G: Q = G * Q_accel * G^T
        Q = G @ Q_accel @ G.T
        
        return Q
    
    def integrate_trajectory(
        self,
        initial_state: State,
        imu_measurements: list,
        dt: float
    ) -> list:
        """
        Integrate IMU measurements over time to produce trajectory.
        
        Used for dead reckoning when UWB is unavailable.
        
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
            predicted_state, _ = self.predict(current_state, imu_meas, dt)
            states.append(predicted_state)
            current_state = predicted_state
        
        return states
    
    def compute_velocity_from_positions(
        self,
        position_current: np.ndarray,
        position_previous: np.ndarray,
        dt: float
    ) -> np.ndarray:
        """
        Estimate velocity from position difference.
        
        Simple finite difference:
            v = (p_k - p_{k-1}) / dt
        
        Args:
            position_current: Current position [x, y, z]
            position_previous: Previous position [x, y, z]
            dt: Time difference (seconds)
            
        Returns:
            Velocity estimate [vx, vy, vz]
        """
        if dt < 1e-6:
            logger.warning("Time step too small for velocity estimation")
            return np.zeros(3)
        
        velocity = (position_current - position_previous) / dt
        
        return velocity
    
    def zero_velocity_update(
        self,
        state: State,
        covariance: np.ndarray,
        velocity_noise: float = 0.01
    ) -> Tuple[State, np.ndarray]:
        """
        Apply zero-velocity update (ZUPT) when tag is stationary.
        
        Firefighter may be stationary during operations.
        Detecting this allows correcting velocity drift.
        
        Args:
            state: Current state
            covariance: Current covariance
            velocity_noise: Measurement noise for zero velocity
            
        Returns:
            updated_state: State with velocity corrected to zero
            updated_covariance: Updated covariance
        """
        # Measurement model: z = v = 0
        H = np.zeros((3, 6))
        H[0:3, 3:6] = np.eye(3)  # Measure velocity components
        
        # Measurement noise
        R = velocity_noise**2 * np.eye(3)
        
        # Kalman update
        innovation = np.zeros(3) - state.velocity
        S = H @ covariance @ H.T + R
        K = covariance @ H.T @ np.linalg.inv(S)
        
        # Update state
        state_vector = state.vector
        updated_vector = state_vector + K @ innovation
        
        updated_state = State.from_vector(updated_vector, state.timestamp)
        
        # Update covariance: P = (I - K*H) * P
        I_KH = np.eye(6) - K @ H
        updated_covariance = I_KH @ covariance @ I_KH.T + K @ R @ K.T
        
        logger.debug("Applied zero-velocity update")
        
        return updated_state, updated_covariance
