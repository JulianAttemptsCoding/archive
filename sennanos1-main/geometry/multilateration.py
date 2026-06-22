"""
Multilateration solver for UWB-based position estimation.

Implements least-squares solution to the multilateration problem:
    ||p - a_i|| = d_i  for i = 1, ..., N

Uses linearization approach from LaTeX document:
    Subtract first equation to create linear system A*p = b
    Solve via least squares: p = (A^T A)^{-1} A^T b

Handles:
- Minimum 4 anchors for 3D positioning
- Graceful degradation with anchor loss
- Numerical stability checks
- Outlier rejection
"""

import numpy as np
from typing import List, Tuple, Optional
import logging

from utils.types import Anchor, UWBMeasurement
from utils.constants import (
    MIN_ANCHORS_3D,
    MAX_CONDITION_NUMBER,
    REGULARIZATION_LAMBDA,
    MAX_BUILDING_ALTITUDE,
    MAX_REASONABLE_VELOCITY
)

logger = logging.getLogger(__name__)


class MultilaterationSolver:
    """
    Solves for 3D position given distance measurements to multiple anchors.
    
    Based on linearized least-squares formulation from LaTeX Section 4.
    """
    
    def __init__(self, regularization: float = REGULARIZATION_LAMBDA):
        """
        Initialize multilateration solver.
        
        Args:
            regularization: Regularization parameter for ill-conditioned systems
        """
        self.regularization = regularization
    
    def solve(
        self,
        anchors: List[Anchor],
        measurements: List[UWBMeasurement],
        initial_guess: Optional[np.ndarray] = None
    ) -> Tuple[Optional[np.ndarray], Optional[np.ndarray], dict]:
        """
        Solve for position using multilateration.
        
        Args:
            anchors: List of Anchor objects with known positions
            measurements: List of UWBMeasurement objects (distance to each anchor)
            initial_guess: Optional initial position guess [x, y, z]
            
        Returns:
            position: Estimated position [x, y, z] or None if solution fails
            covariance: Position covariance matrix (3x3) or None
            info: Dictionary with diagnostic information
        """
        # Validate inputs
        if len(anchors) < MIN_ANCHORS_3D:
            logger.warning(f"Insufficient anchors: {len(anchors)} < {MIN_ANCHORS_3D}")
            return None, None, {"error": "insufficient_anchors", "num_anchors": len(anchors)}
        
        if len(measurements) != len(anchors):
            logger.error("Anchor and measurement count mismatch")
            return None, None, {"error": "count_mismatch"}
        
        # Filter valid measurements
        valid_indices = [i for i, m in enumerate(measurements) if m.is_valid]
        if len(valid_indices) < MIN_ANCHORS_3D:
            logger.warning(f"Insufficient valid measurements: {len(valid_indices)}")
            return None, None, {"error": "insufficient_valid_measurements"}
        
        # Extract valid anchors and measurements
        valid_anchors = [anchors[i] for i in valid_indices]
        valid_measurements = [measurements[i] for i in valid_indices]
        
        # Build and solve linear system
        A, b = self._build_linear_system(valid_anchors, valid_measurements)
        
        if A is None or b is None:
            return None, None, {"error": "system_construction_failed"}
        
        # Check condition number
        try:
            AtA = A.T @ A
            cond = np.linalg.cond(AtA)
            
            if cond > MAX_CONDITION_NUMBER:
                logger.warning(f"Ill-conditioned system: cond = {cond:.2e}")
                # Apply regularization
                AtA += self.regularization * np.eye(3)
        except np.linalg.LinAlgError:
            logger.error("Singular matrix encountered")
            return None, None, {"error": "singular_matrix"}
        
        # Solve least squares: p = (A^T A)^{-1} A^T b
        try:
            position = np.linalg.solve(AtA, A.T @ b)
        except np.linalg.LinAlgError:
            logger.error("Failed to solve linear system")
            return None, None, {"error": "solve_failed"}
        
        # Compute residuals and covariance
        residuals = self._compute_residuals(position, valid_anchors, valid_measurements)
        covariance = self._estimate_covariance(A, valid_measurements)
        
        # Sanity checks
        if not self._is_solution_reasonable(position):
            logger.warning(f"Unreasonable solution: {position}")
            return None, None, {"error": "unreasonable_solution", "position": position}
        
        info = {
            "num_anchors": len(valid_anchors),
            "residuals": residuals,
            "mean_residual": np.mean(np.abs(residuals)),
            "max_residual": np.max(np.abs(residuals)),
            "condition_number": cond
        }
        
        logger.debug(f"Multilateration success: pos={position}, residual={info['mean_residual']:.3f}m")
        
        return position, covariance, info
    
    def _build_linear_system(
        self,
        anchors: List[Anchor],
        measurements: List[UWBMeasurement]
    ) -> Tuple[Optional[np.ndarray], Optional[np.ndarray]]:
        """
        Build linearized system A*p = b from anchor positions and measurements.
        
        From LaTeX Section 4:
            (x - x_i)^2 + (y - y_i)^2 + (z - z_i)^2 = d_i^2
        
        Linearize by subtracting first equation:
            2(x_1 - x_i)x + 2(y_1 - y_i)y + 2(z_1 - z_i)z = 
                d_i^2 - d_1^2 + x_i^2 - x_1^2 + y_i^2 - y_1^2 + z_i^2 - z_1^2
        
        Args:
            anchors: List of valid anchors
            measurements: List of valid measurements
            
        Returns:
            A: (N-1) x 3 matrix
            b: (N-1) vector
        """
        N = len(anchors)
        
        # Reference anchor (first)
        a1 = anchors[0].position
        d1 = measurements[0].distance
        
        # Build system for remaining anchors
        A = np.zeros((N - 1, 3))
        b = np.zeros(N - 1)
        
        for i in range(1, N):
            ai = anchors[i].position
            di = measurements[i].distance
            
            # A matrix: 2 * (a1 - ai)
            A[i - 1] = 2 * (a1 - ai)
            
            # b vector: di^2 - d1^2 + ||a1||^2 - ||ai||^2
            b[i - 1] = (
                di**2 - d1**2 +
                np.dot(a1, a1) - np.dot(ai, ai)
            )
        
        return A, b
    
    def _compute_residuals(
        self,
        position: np.ndarray,
        anchors: List[Anchor],
        measurements: List[UWBMeasurement]
    ) -> np.ndarray:
        """
        Compute residuals between predicted and measured distances.
        
        residual_i = ||position - anchor_i|| - measured_distance_i
        
        Args:
            position: Estimated position [x, y, z]
            anchors: List of anchors
            measurements: List of measurements
            
        Returns:
            Array of residuals (meters)
        """
        residuals = np.zeros(len(anchors))
        
        for i, (anchor, meas) in enumerate(zip(anchors, measurements)):
            predicted_dist = np.linalg.norm(position - anchor.position)
            residuals[i] = predicted_dist - meas.distance
        
        return residuals
    
    def _estimate_covariance(
        self,
        A: np.ndarray,
        measurements: List[UWBMeasurement]
    ) -> np.ndarray:
        """
        Estimate position covariance matrix.
        
        From error propagation:
            Cov(p) = (A^T W A)^{-1}
        
        where W is the inverse measurement covariance (weight matrix).
        
        Args:
            A: Design matrix
            measurements: List of measurements with variances
            
        Returns:
            3x3 covariance matrix for position
        """
        # Construct weight matrix (inverse of measurement covariance)
        # Each measurement has variance σ_i^2
        N = len(measurements)
        W = np.zeros((N - 1, N - 1))
        
        for i in range(N - 1):
            # Weight is inverse variance
            # Skip first measurement (used as reference)
            variance = measurements[i + 1].variance
            W[i, i] = 1.0 / variance
        
        try:
            # Covariance: (A^T W A)^{-1}
            AtWA = A.T @ W @ A
            covariance = np.linalg.inv(AtWA + self.regularization * np.eye(3))
        except np.linalg.LinAlgError:
            # Fallback to unweighted covariance
            logger.warning("Failed to compute weighted covariance, using unweighted")
            try:
                covariance = np.linalg.inv(A.T @ A + self.regularization * np.eye(3))
            except np.linalg.LinAlgError:
                # Ultimate fallback: large uncertainty
                covariance = np.eye(3) * 100.0
        
        return covariance
    
    def _is_solution_reasonable(self, position: np.ndarray) -> bool:
        """
        Perform sanity checks on the computed position.
        
        Reject solutions that are physically impossible:
        - Above maximum building height
        - Contain NaN or Inf
        - Extremely large coordinates
        
        Args:
            position: Estimated position [x, y, z]
            
        Returns:
            True if solution passes sanity checks
        """
        # Check for NaN or Inf
        if not np.all(np.isfinite(position)):
            logger.warning("Position contains NaN or Inf")
            return False
        
        # Check altitude constraint
        if position[2] > MAX_BUILDING_ALTITUDE:
            logger.warning(f"Altitude {position[2]:.1f}m exceeds maximum {MAX_BUILDING_ALTITUDE}m")
            return False
        
        if position[2] < -10.0:  # Allow for basement
            logger.warning(f"Altitude {position[2]:.1f}m below reasonable minimum")
            return False
        
        # Check for extremely large coordinates (likely numerical error)
        if np.linalg.norm(position) > 1000.0:
            logger.warning(f"Position magnitude {np.linalg.norm(position):.1f}m unreasonably large")
            return False
        
        return True
    
    def solve_iterative(
        self,
        anchors: List[Anchor],
        measurements: List[UWBMeasurement],
        initial_guess: np.ndarray,
        max_iterations: int = 10,
        tolerance: float = 1e-4
    ) -> Tuple[Optional[np.ndarray], Optional[np.ndarray], dict]:
        """
        Iterative refinement using Gauss-Newton method.
        
        For cases where linearization error is significant,
        iterate to improve solution accuracy.
        
        Args:
            anchors: List of anchors
            measurements: List of measurements
            initial_guess: Initial position estimate
            max_iterations: Maximum iterations
            tolerance: Convergence tolerance (meters)
            
        Returns:
            position: Refined position estimate
            covariance: Position covariance
            info: Diagnostic information
        """
        position = initial_guess.copy()
        
        for iteration in range(max_iterations):
            # Compute Jacobian and residuals
            J, r = self._compute_jacobian_and_residuals(position, anchors, measurements)
            
            # Check convergence
            if np.linalg.norm(r) < tolerance:
                logger.debug(f"Converged in {iteration} iterations")
                break
            
            # Gauss-Newton update: Δp = -(J^T J)^{-1} J^T r
            try:
                JtJ = J.T @ J + self.regularization * np.eye(3)
                delta = np.linalg.solve(JtJ, -J.T @ r)
                position += delta
            except np.linalg.LinAlgError:
                logger.warning("Iteration failed, returning current estimate")
                break
        
        # Estimate covariance
        covariance = self._estimate_covariance_nonlinear(J, measurements)
        
        info = {
            "iterations": iteration + 1,
            "final_residual": np.linalg.norm(r),
            "converged": np.linalg.norm(r) < tolerance
        }
        
        return position, covariance, info
    
    def _compute_jacobian_and_residuals(
        self,
        position: np.ndarray,
        anchors: List[Anchor],
        measurements: List[UWBMeasurement]
    ) -> Tuple[np.ndarray, np.ndarray]:
        """
        Compute Jacobian matrix and residuals for nonlinear least squares.
        
        For each anchor i:
            r_i = ||p - a_i|| - d_i
            J_i = (p - a_i) / ||p - a_i||
        
        Args:
            position: Current position estimate
            anchors: List of anchors
            measurements: List of measurements
            
        Returns:
            J: N x 3 Jacobian matrix
            r: N residual vector
        """
        N = len(anchors)
        J = np.zeros((N, 3))
        r = np.zeros(N)
        
        for i, (anchor, meas) in enumerate(zip(anchors, measurements)):
            diff = position - anchor.position
            dist = np.linalg.norm(diff)
            
            # Residual
            r[i] = dist - meas.distance
            
            # Jacobian: ∂r/∂p = (p - a) / ||p - a||
            if dist > 1e-6:  # Avoid division by zero
                J[i] = diff / dist
            else:
                J[i] = np.zeros(3)
        
        return J, r
    
    def _estimate_covariance_nonlinear(
        self,
        J: np.ndarray,
        measurements: List[UWBMeasurement]
    ) -> np.ndarray:
        """
        Estimate covariance for nonlinear least squares.
        
        Cov(p) = (J^T W J)^{-1}
        
        Args:
            J: Jacobian matrix
            measurements: List of measurements
            
        Returns:
            3x3 covariance matrix
        """
        N = len(measurements)
        W = np.diag([1.0 / m.variance for m in measurements])
        
        try:
            JtWJ = J.T @ W @ J
            covariance = np.linalg.inv(JtWJ + self.regularization * np.eye(3))
        except np.linalg.LinAlgError:
            covariance = np.eye(3) * 10.0
        
        return covariance
