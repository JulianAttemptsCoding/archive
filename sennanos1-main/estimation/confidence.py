"""
Confidence estimation for position tracking.

Implements confidence scoring from LaTeX Section 8:

    C = exp(-α * trace(P))

where:
    C = confidence score [0, 1]
    α = decay parameter
    P = state covariance matrix

Color mapping:
    C > 0.7     → Green (HIGH)
    0.4 < C ≤ 0.7 → Yellow (MEDIUM)
    C ≤ 0.4     → Red (LOW)

Confidence depends on:
- Covariance magnitude
- Number of anchors
- Time since last UWB update
- Measurement residuals
- GDOP
"""

import numpy as np
from typing import List, Optional
import logging

from utils.types import ConfidenceLevel, Anchor
from utils.constants import (
    CONFIDENCE_DECAY_ALPHA,
    CONFIDENCE_HIGH_THRESHOLD,
    CONFIDENCE_MEDIUM_THRESHOLD,
    MAX_TIME_WITHOUT_UPDATE,
    MIN_ANCHORS_3D
)

logger = logging.getLogger(__name__)


class ConfidenceEstimator:
    """
    Estimates confidence in position estimate.
    
    Provides real-time feedback on tracking quality for safety-critical decisions.
    """
    
    def __init__(
        self,
        alpha: float = CONFIDENCE_DECAY_ALPHA,
        high_threshold: float = CONFIDENCE_HIGH_THRESHOLD,
        medium_threshold: float = CONFIDENCE_MEDIUM_THRESHOLD
    ):
        """
        Initialize confidence estimator.
        
        Args:
            alpha: Decay parameter for covariance-based confidence
            high_threshold: Threshold for high confidence
            medium_threshold: Threshold for medium confidence
        """
        self.alpha = alpha
        self.high_threshold = high_threshold
        self.medium_threshold = medium_threshold
        
        logger.debug(f"Confidence estimator initialized: α={alpha}, thresholds=[{medium_threshold}, {high_threshold}]")
    
    def compute_confidence(
        self,
        covariance: np.ndarray,
        num_anchors: int,
        time_since_update: float,
        mean_residual: Optional[float] = None,
        gdop: Optional[float] = None
    ) -> float:
        """
        Compute overall confidence score.
        
        Combines multiple factors:
        1. Covariance-based confidence (primary)
        2. Number of anchors
        3. Time since last update
        4. Measurement residuals
        5. Geometry quality (GDOP)
        
        Args:
            covariance: State covariance matrix (6x6)
            num_anchors: Number of anchors used
            time_since_update: Time since last UWB correction (seconds)
            mean_residual: Mean measurement residual (meters), optional
            gdop: GDOP value, optional
            
        Returns:
            Confidence score [0, 1]
        """
        # 1. Covariance-based confidence (primary factor)
        C_cov = self._covariance_confidence(covariance)
        
        # 2. Anchor count factor
        C_anchors = self._anchor_confidence(num_anchors)
        
        # 3. Time decay factor
        C_time = self._time_confidence(time_since_update)
        
        # 4. Residual factor (if available)
        C_residual = 1.0
        if mean_residual is not None:
            C_residual = self._residual_confidence(mean_residual)
        
        # 5. Geometry factor (if available)
        C_gdop = 1.0
        if gdop is not None:
            C_gdop = self._gdop_confidence(gdop)
        
        # Combine factors (multiplicative)
        # Each factor reduces confidence if problematic
        confidence = C_cov * C_anchors * C_time * C_residual * C_gdop
        
        # Clamp to [0, 1]
        confidence = np.clip(confidence, 0.0, 1.0)
        
        logger.debug(
            f"Confidence: {confidence:.3f} "
            f"(cov={C_cov:.3f}, anch={C_anchors:.3f}, time={C_time:.3f}, "
            f"res={C_residual:.3f}, gdop={C_gdop:.3f})"
        )
        
        return confidence
    
    def _covariance_confidence(self, covariance: np.ndarray) -> float:
        """
        Compute confidence from covariance magnitude.
        
        From LaTeX: C = exp(-α * trace(P))
        
        Args:
            covariance: State covariance (6x6)
            
        Returns:
            Covariance-based confidence [0, 1]
        """
        # Use only position covariance (first 3x3 block)
        position_covariance = covariance[:3, :3]
        trace_P = np.trace(position_covariance)
        
        # Exponential decay: C = exp(-α * trace(P))
        confidence = np.exp(-self.alpha * trace_P)
        
        return confidence
    
    def _anchor_confidence(self, num_anchors: int) -> float:
        """
        Compute confidence from number of anchors.
        
        More anchors → better redundancy → higher confidence
        
        Args:
            num_anchors: Number of visible anchors
            
        Returns:
            Anchor-based confidence [0, 1]
        """
        if num_anchors < MIN_ANCHORS_3D:
            return 0.0  # Cannot solve
        elif num_anchors == MIN_ANCHORS_3D:
            return 0.6  # Minimum viable
        elif num_anchors == MIN_ANCHORS_3D + 1:
            return 0.8  # Good
        else:
            return 1.0  # Excellent redundancy
    
    def _time_confidence(self, time_since_update: float) -> float:
        """
        Compute confidence decay over time without UWB updates.
        
        Prediction uncertainty grows with time → confidence decays
        
        Args:
            time_since_update: Time since last UWB update (seconds)
            
        Returns:
            Time-based confidence [0, 1]
        """
        if time_since_update <= 0:
            return 1.0
        
        # Exponential decay with time
        # At MAX_TIME_WITHOUT_UPDATE, confidence drops to ~0.1
        decay_rate = 2.3 / MAX_TIME_WITHOUT_UPDATE  # ln(10) / max_time
        confidence = np.exp(-decay_rate * time_since_update)
        
        return confidence
    
    def _residual_confidence(self, mean_residual: float) -> float:
        """
        Compute confidence from measurement residuals.
        
        Large residuals indicate:
        - Model mismatch
        - NLOS conditions
        - Outliers
        
        Args:
            mean_residual: Mean absolute residual (meters)
            
        Returns:
            Residual-based confidence [0, 1]
        """
        # Residual thresholds
        good_residual = 0.5  # meters
        poor_residual = 2.0  # meters
        
        if mean_residual <= good_residual:
            return 1.0
        elif mean_residual >= poor_residual:
            return 0.3
        else:
            # Linear interpolation between thresholds
            confidence = 1.0 - 0.7 * (mean_residual - good_residual) / (poor_residual - good_residual)
            return confidence
    
    def _gdop_confidence(self, gdop: float) -> float:
        """
        Compute confidence from GDOP.
        
        Lower GDOP → better geometry → higher confidence
        
        Args:
            gdop: Geometric dilution of precision
            
        Returns:
            GDOP-based confidence [0, 1]
        """
        # GDOP thresholds
        excellent_gdop = 2.0
        poor_gdop = 10.0
        
        if gdop <= excellent_gdop:
            return 1.0
        elif gdop >= poor_gdop:
            return 0.4
        else:
            # Logarithmic decay
            confidence = 1.0 - 0.6 * np.log(gdop / excellent_gdop) / np.log(poor_gdop / excellent_gdop)
            return confidence
    
    def classify_confidence(self, confidence: float) -> ConfidenceLevel:
        """
        Classify confidence score into color-coded level.
        
        From LaTeX Section 8:
            C > 0.7     → Green
            0.4 < C ≤ 0.7 → Yellow
            C ≤ 0.4     → Red
        
        Args:
            confidence: Confidence score [0, 1]
            
        Returns:
            ConfidenceLevel enum
        """
        if confidence > self.high_threshold:
            return ConfidenceLevel.HIGH
        elif confidence > self.medium_threshold:
            return ConfidenceLevel.MEDIUM
        else:
            return ConfidenceLevel.LOW
    
    def should_trigger_alert(
        self,
        confidence: float,
        alert_threshold: float = 0.4
    ) -> bool:
        """
        Determine if low confidence should trigger alert.
        
        Alert indicates:
        - Possible tag loss
        - Need for drone assistance
        - Increased position uncertainty
        
        Args:
            confidence: Current confidence score
            alert_threshold: Threshold for triggering alert
            
        Returns:
            True if alert should be triggered
        """
        return confidence <= alert_threshold
    
    def estimate_position_error_bound(
        self,
        covariance: np.ndarray,
        confidence_level: float = 0.95
    ) -> float:
        """
        Estimate position error bound at specified confidence level.
        
        Uses covariance to compute error ellipsoid radius.
        
        Args:
            covariance: State covariance (6x6)
            confidence_level: Statistical confidence level (e.g., 0.95)
            
        Returns:
            Error bound radius (meters)
        """
        # Extract position covariance
        position_covariance = covariance[:3, :3]
        
        # Compute eigenvalues (principal axes of error ellipsoid)
        eigenvalues = np.linalg.eigvalsh(position_covariance)
        
        # Maximum eigenvalue gives worst-case uncertainty
        max_std = np.sqrt(np.max(eigenvalues))
        
        # Scale by confidence level (Gaussian assumption)
        # 95% → 1.96σ, 99% → 2.58σ
        if confidence_level == 0.95:
            scale = 1.96
        elif confidence_level == 0.99:
            scale = 2.58
        else:
            # Use inverse CDF (approximation)
            from scipy.stats import norm
            try:
                scale = norm.ppf((1 + confidence_level) / 2)
            except ImportError:
                scale = 2.0  # Default
        
        error_bound = scale * max_std
        
        return error_bound
    
    def get_confidence_report(
        self,
        covariance: np.ndarray,
        num_anchors: int,
        time_since_update: float,
        mean_residual: Optional[float] = None,
        gdop: Optional[float] = None
    ) -> dict:
        """
        Generate comprehensive confidence report.
        
        Useful for logging and diagnostics.
        
        Args:
            covariance: State covariance
            num_anchors: Number of anchors
            time_since_update: Time since last update
            mean_residual: Mean residual (optional)
            gdop: GDOP value (optional)
            
        Returns:
            Dictionary with confidence metrics
        """
        confidence = self.compute_confidence(
            covariance, num_anchors, time_since_update, mean_residual, gdop
        )
        
        level = self.classify_confidence(confidence)
        
        position_uncertainty = np.sqrt(np.trace(covariance[:3, :3]))
        error_bound_95 = self.estimate_position_error_bound(covariance, 0.95)
        
        report = {
            "confidence": confidence,
            "confidence_level": level.value,
            "num_anchors": num_anchors,
            "time_since_update": time_since_update,
            "position_uncertainty_rms": position_uncertainty,
            "error_bound_95": error_bound_95,
            "should_alert": self.should_trigger_alert(confidence),
            "factors": {
                "covariance": self._covariance_confidence(covariance),
                "anchors": self._anchor_confidence(num_anchors),
                "time": self._time_confidence(time_since_update),
                "residual": self._residual_confidence(mean_residual) if mean_residual else None,
                "gdop": self._gdop_confidence(gdop) if gdop else None
            }
        }
        
        return report
