"""
Kalman filter implementation for sensor fusion.

Implements the prediction-correction cycle from LaTeX Section 7:

PREDICTION:
    x̂_{k|k-1} = F * x̂_{k-1|k-1}
    P_{k|k-1} = F * P_{k-1|k-1} * F^T + Q

CORRECTION (Measurement Update):
    K_k = P_{k|k-1} * H^T * (H * P_{k|k-1} * H^T + R)^{-1}
    x̂_{k|k} = x̂_{k|k-1} + K_k * (z_k - H * x̂_{k|k-1})
    P_{k|k} = (I - K_k * H) * P_{k|k-1}

Fuses:
- IMU predictions (motion model)
- UWB measurements (position)
- Barometer measurements (altitude)
"""

import numpy as np
from typing import Optional, Tuple
import logging

from utils.types import State
from utils.constants import REGULARIZATION_LAMBDA

logger = logging.getLogger(__name__)


class KalmanFilter:
    """
    Extended Kalman Filter for state estimation.
    
    Maintains state vector x = [x, y, z, vx, vy, vz]^T
    and covariance matrix P (6x6).
    """
    
    def __init__(
        self,
        initial_state: State,
        initial_covariance: np.ndarray,
        regularization: float = REGULARIZATION_LAMBDA
    ):
        """
        Initialize Kalman filter.
        
        Args:
            initial_state: Initial state estimate
            initial_covariance: Initial covariance matrix (6x6)
            regularization: Regularization parameter for numerical stability
        """
        assert initial_covariance.shape == (6, 6), "Covariance must be 6x6"
        
        self.state = initial_state.copy()
        self.covariance = initial_covariance.copy()
        self.regularization = regularization
        
        logger.debug(f"Kalman filter initialized at position {self.state.position}")
    
    def predict(
        self,
        F: np.ndarray,
        Q: np.ndarray,
        timestamp: float
    ) -> Tuple[State, np.ndarray]:
        """
        Prediction step using motion model.
        
        From LaTeX Section 7:
            x̂_{k|k-1} = F * x̂_{k-1|k-1}
            P_{k|k-1} = F * P_{k-1|k-1} * F^T + Q
        
        Args:
            F: State transition matrix (6x6)
            Q: Process noise covariance (6x6)
            timestamp: Prediction timestamp
            
        Returns:
            predicted_state: Predicted state
            predicted_covariance: Predicted covariance
        """
        # Predict state: x̂_{k|k-1} = F * x̂_{k-1|k-1}
        state_vector = self.state.vector
        predicted_vector = F @ state_vector
        
        # Predict covariance: P_{k|k-1} = F * P * F^T + Q
        predicted_covariance = F @ self.covariance @ F.T + Q
        
        # Ensure symmetry (numerical stability)
        predicted_covariance = 0.5 * (predicted_covariance + predicted_covariance.T)
        
        # Update internal state
        self.state = State.from_vector(predicted_vector, timestamp)
        self.covariance = predicted_covariance
        
        logger.debug(f"Prediction: pos={self.state.position}, trace(P)={np.trace(self.covariance):.3f}")
        
        return self.state.copy(), self.covariance.copy()
    
    def update(
        self,
        z: np.ndarray,
        H: np.ndarray,
        R: np.ndarray,
        timestamp: float
    ) -> Tuple[State, np.ndarray]:
        """
        Measurement update (correction) step.
        
        From LaTeX Section 7:
            K = P * H^T * (H * P * H^T + R)^{-1}
            x̂ = x̂ + K * (z - H * x̂)
            P = (I - K * H) * P
        
        Args:
            z: Measurement vector
            H: Measurement matrix (maps state to measurement)
            R: Measurement noise covariance
            timestamp: Measurement timestamp
            
        Returns:
            updated_state: Updated state
            updated_covariance: Updated covariance
        """
        # Predicted measurement: ẑ = H * x̂
        state_vector = self.state.vector
        z_pred = H @ state_vector
        
        # Innovation: y = z - ẑ
        innovation = z - z_pred
        
        # Innovation covariance: S = H * P * H^T + R
        S = H @ self.covariance @ H.T + R
        
        # Add regularization for numerical stability
        S += self.regularization * np.eye(S.shape[0])
        
        # Kalman gain: K = P * H^T * S^{-1}
        try:
            K = self.covariance @ H.T @ np.linalg.inv(S)
        except np.linalg.LinAlgError:
            logger.error("Failed to invert innovation covariance")
            return self.state.copy(), self.covariance.copy()
        
        # Update state: x̂ = x̂ + K * y
        state_vector_updated = state_vector + K @ innovation
        
        # Update covariance: P = (I - K * H) * P
        # Joseph form for numerical stability:
        # P = (I - K*H) * P * (I - K*H)^T + K * R * K^T
        I_KH = np.eye(6) - K @ H
        covariance_updated = I_KH @ self.covariance @ I_KH.T + K @ R @ K.T
        
        # Ensure symmetry
        covariance_updated = 0.5 * (covariance_updated + covariance_updated.T)
        
        # Update internal state
        self.state = State.from_vector(state_vector_updated, timestamp)
        self.covariance = covariance_updated
        
        logger.debug(f"Update: innovation_norm={np.linalg.norm(innovation):.3f}, trace(P)={np.trace(self.covariance):.3f}")
        
        return self.state.copy(), self.covariance.copy()
    
    def predict_and_update(
        self,
        F: np.ndarray,
        Q: np.ndarray,
        z: np.ndarray,
        H: np.ndarray,
        R: np.ndarray,
        timestamp: float
    ) -> Tuple[State, np.ndarray]:
        """
        Combined predict and update step.
        
        Convenience method for single-step filtering.
        
        Args:
            F: State transition matrix
            Q: Process noise covariance
            z: Measurement vector
            H: Measurement matrix
            R: Measurement noise covariance
            timestamp: Timestamp
            
        Returns:
            state: Updated state
            covariance: Updated covariance
        """
        self.predict(F, Q, timestamp)
        return self.update(z, H, R, timestamp)
    
    def get_state(self) -> State:
        """Get current state estimate."""
        return self.state.copy()
    
    def get_covariance(self) -> np.ndarray:
        """Get current covariance estimate."""
        return self.covariance.copy()
    
    def get_position_uncertainty(self) -> np.ndarray:
        """
        Get position uncertainty (standard deviations).
        
        Returns:
            [σ_x, σ_y, σ_z] in meters
        """
        position_covariance = self.covariance[:3, :3]
        position_std = np.sqrt(np.diag(position_covariance))
        return position_std
    
    def get_velocity_uncertainty(self) -> np.ndarray:
        """
        Get velocity uncertainty (standard deviations).
        
        Returns:
            [σ_vx, σ_vy, σ_vz] in m/s
        """
        velocity_covariance = self.covariance[3:6, 3:6]
        velocity_std = np.sqrt(np.diag(velocity_covariance))
        return velocity_std
    
    def reset(self, state: State, covariance: np.ndarray):
        """
        Reset filter to new state and covariance.
        
        Used when position is re-initialized or track is lost.
        
        Args:
            state: New state
            covariance: New covariance
        """
        self.state = state.copy()
        self.covariance = covariance.copy()
        logger.info(f"Kalman filter reset to position {self.state.position}")
    
    def check_consistency(
        self,
        z: np.ndarray,
        H: np.ndarray,
        R: np.ndarray,
        threshold: float = 3.0
    ) -> bool:
        """
        Check measurement consistency using chi-squared test.
        
        Normalized innovation squared (NIS):
            ε = (z - ẑ)^T * S^{-1} * (z - ẑ)
        
        If ε > threshold^2, measurement is inconsistent (outlier).
        
        Args:
            z: Measurement
            H: Measurement matrix
            R: Measurement covariance
            threshold: Threshold in standard deviations
            
        Returns:
            True if measurement is consistent
        """
        # Predicted measurement
        z_pred = H @ self.state.vector
        
        # Innovation
        innovation = z - z_pred
        
        # Innovation covariance
        S = H @ self.covariance @ H.T + R
        
        # Normalized innovation squared
        try:
            nis = innovation.T @ np.linalg.inv(S) @ innovation
            
            # Chi-squared test
            threshold_squared = threshold ** 2
            is_consistent = nis < threshold_squared
            
            if not is_consistent:
                logger.warning(f"Measurement inconsistent: NIS={nis:.2f} > {threshold_squared:.2f}")
            
            return is_consistent
        
        except np.linalg.LinAlgError:
            logger.error("Failed to compute NIS")
            return True  # Accept measurement by default
    
    def adaptive_update(
        self,
        z: np.ndarray,
        H: np.ndarray,
        R: np.ndarray,
        timestamp: float,
        alpha: float = 0.5
    ) -> Tuple[State, np.ndarray]:
        """
        Adaptive measurement update with innovation-based adjustment.
        
        Reduces gain when innovation is large (outlier protection).
        
        Args:
            z: Measurement
            H: Measurement matrix
            R: Measurement covariance
            timestamp: Timestamp
            alpha: Adaptation parameter [0, 1]
            
        Returns:
            updated_state: Updated state
            updated_covariance: Updated covariance
        """
        # Check consistency
        is_consistent = self.check_consistency(z, H, R)
        
        if is_consistent:
            # Standard update
            return self.update(z, H, R, timestamp)
        else:
            # Reduce measurement weight by inflating R
            R_inflated = R / alpha
            logger.debug(f"Applying adaptive update with alpha={alpha}")
            return self.update(z, H, R_inflated, timestamp)
    
    def get_innovation_statistics(
        self,
        z: np.ndarray,
        H: np.ndarray
    ) -> dict:
        """
        Compute innovation statistics for diagnostics.
        
        Args:
            z: Measurement
            H: Measurement matrix
            
        Returns:
            Dictionary with innovation statistics
        """
        z_pred = H @ self.state.vector
        innovation = z - z_pred
        
        stats = {
            "innovation": innovation,
            "innovation_norm": np.linalg.norm(innovation),
            "predicted_measurement": z_pred,
            "actual_measurement": z
        }
        
        return stats
