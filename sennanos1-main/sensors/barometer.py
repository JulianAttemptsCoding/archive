"""
Barometric altitude estimation.

Implements atmospheric pressure model from LaTeX Section 6:

    z = (RT/Mg) * ln(P0/P)

where:
    R = specific gas constant
    T = temperature
    M = molar mass of air
    g = gravitational acceleration
    P0 = reference pressure
    P = measured pressure

Provides strong vertical constraint, reducing 3D problem to quasi-2.5D.
"""

import numpy as np
from typing import Tuple
import logging

from utils.types import BarometerMeasurement
from utils.constants import (
    R_SPECIFIC,
    STANDARD_TEMP,
    GRAVITY,
    BAROMETER_VARIANCE,
    P0_STANDARD
)

logger = logging.getLogger(__name__)


class BarometricAltimeter:
    """
    Converts barometric pressure to altitude estimate.
    
    Provides vertical constraint that significantly improves
    3D localization accuracy.
    """
    
    def __init__(
        self,
        temperature: float = STANDARD_TEMP,
        reference_pressure: float = P0_STANDARD
    ):
        """
        Initialize barometric altimeter.
        
        Args:
            temperature: Ambient temperature (Kelvin)
            reference_pressure: Reference pressure at ground level (Pascals)
        """
        self.temperature = temperature
        self.reference_pressure = reference_pressure
        
        # Precompute scale height: H = RT/Mg
        self.scale_height = (R_SPECIFIC * self.temperature) / GRAVITY
        
        logger.debug(f"Barometric altimeter initialized: H={self.scale_height:.1f}m, P0={reference_pressure:.0f}Pa")
    
    def pressure_to_altitude(self, pressure: float) -> float:
        """
        Convert pressure to altitude.
        
        From LaTeX Section 6:
            z = (RT/Mg) * ln(P0/P)
        
        Args:
            pressure: Measured pressure (Pascals)
            
        Returns:
            Altitude (meters)
        """
        if pressure <= 0:
            logger.error(f"Invalid pressure: {pressure} Pa")
            return 0.0
        
        if pressure > self.reference_pressure:
            logger.warning(f"Pressure {pressure} Pa exceeds reference {self.reference_pressure} Pa")
        
        # z = H * ln(P0/P)
        altitude = self.scale_height * np.log(self.reference_pressure / pressure)
        
        return altitude
    
    def altitude_to_pressure(self, altitude: float) -> float:
        """
        Convert altitude to expected pressure.
        
        Inverse of pressure_to_altitude:
            P = P0 * exp(-z/H)
        
        Args:
            altitude: Altitude (meters)
            
        Returns:
            Expected pressure (Pascals)
        """
        pressure = self.reference_pressure * np.exp(-altitude / self.scale_height)
        return pressure
    
    def estimate_altitude(
        self,
        measurement: BarometerMeasurement
    ) -> Tuple[float, float]:
        """
        Estimate altitude from barometer measurement.
        
        Args:
            measurement: BarometerMeasurement object
            
        Returns:
            altitude: Estimated altitude (meters)
            variance: Altitude estimate variance (m^2)
        """
        # Convert pressure to altitude
        altitude = self.pressure_to_altitude(measurement.pressure)
        
        # Propagate uncertainty: σ_z^2 = (∂z/∂P)^2 * σ_P^2
        # ∂z/∂P = -H/P
        dz_dP = -self.scale_height / measurement.pressure
        altitude_variance = (dz_dP ** 2) * measurement.variance
        
        return altitude, altitude_variance
    
    def calibrate_reference_pressure(
        self,
        measurements: list,
        known_altitude: float
    ) -> float:
        """
        Calibrate reference pressure using known altitude.
        
        Useful for setting ground floor reference during system setup.
        
        Args:
            measurements: List of BarometerMeasurement at known altitude
            known_altitude: Known altitude (meters)
            
        Returns:
            Calibrated reference pressure (Pascals)
        """
        if len(measurements) == 0:
            logger.error("No measurements provided for calibration")
            return self.reference_pressure
        
        # Average measured pressures
        pressures = [m.pressure for m in measurements]
        avg_pressure = np.mean(pressures)
        
        # Compute reference: P0 = P * exp(z/H)
        reference_pressure = avg_pressure * np.exp(known_altitude / self.scale_height)
        
        logger.info(f"Calibrated reference pressure: {reference_pressure:.0f} Pa at altitude {known_altitude:.1f}m")
        
        self.reference_pressure = reference_pressure
        return reference_pressure
    
    def estimate_floor_number(
        self,
        altitude: float,
        floor_height: float = 3.5
    ) -> int:
        """
        Estimate floor number from altitude.
        
        Useful for displaying floor information to incident commander.
        
        Args:
            altitude: Altitude above ground (meters)
            floor_height: Typical floor-to-floor height (meters)
            
        Returns:
            Floor number (0 = ground, 1 = first floor, etc.)
        """
        floor = int(np.round(altitude / floor_height))
        return max(0, floor)  # Cannot be below ground (unless basement)
    
    def apply_temperature_correction(
        self,
        altitude_raw: float,
        measured_temperature: float
    ) -> float:
        """
        Correct altitude for temperature deviation from standard.
        
        Altitude depends on temperature via scale height.
        If actual temperature differs from standard, correct accordingly.
        
        Args:
            altitude_raw: Raw altitude computed with standard temperature
            measured_temperature: Actual temperature (Kelvin)
            
        Returns:
            Temperature-corrected altitude (meters)
        """
        # Scale height ratio
        temp_ratio = measured_temperature / self.temperature
        
        # Corrected altitude
        altitude_corrected = altitude_raw * temp_ratio
        
        return altitude_corrected
    
    def fuse_with_prior(
        self,
        barometer_altitude: float,
        barometer_variance: float,
        prior_altitude: float,
        prior_variance: float
    ) -> Tuple[float, float]:
        """
        Fuse barometric altitude with prior estimate.
        
        Uses optimal weighted average based on variances.
        
        Args:
            barometer_altitude: Altitude from barometer (meters)
            barometer_variance: Barometer variance (m^2)
            prior_altitude: Prior altitude estimate (meters)
            prior_variance: Prior variance (m^2)
            
        Returns:
            fused_altitude: Fused altitude estimate (meters)
            fused_variance: Fused variance (m^2)
        """
        # Compute Kalman gain
        K = prior_variance / (prior_variance + barometer_variance)
        
        # Fused estimate
        fused_altitude = prior_altitude + K * (barometer_altitude - prior_altitude)
        
        # Fused variance
        fused_variance = (1 - K) * prior_variance
        
        return fused_altitude, fused_variance
    
    def detect_floor_change(
        self,
        altitude_current: float,
        altitude_previous: float,
        threshold: float = 2.0
    ) -> int:
        """
        Detect floor change (stairs/elevator).
        
        Useful for alerting incident commander to vertical movement.
        
        Args:
            altitude_current: Current altitude (meters)
            altitude_previous: Previous altitude (meters)
            threshold: Threshold for floor change detection (meters)
            
        Returns:
            Floor change: +1 (up), -1 (down), 0 (no change)
        """
        delta = altitude_current - altitude_previous
        
        if delta > threshold:
            return 1  # Moved up
        elif delta < -threshold:
            return -1  # Moved down
        else:
            return 0  # No significant change
    
    def create_measurement_matrix(self) -> Tuple[np.ndarray, np.ndarray]:
        """
        Create measurement matrix for Kalman filter integration.
        
        Barometer measures only z-component of state.
        
        Returns:
            H: 1x6 measurement matrix [0, 0, 1, 0, 0, 0]
            R: 1x1 measurement noise covariance
        """
        # Measurement matrix: observes z position only
        H = np.zeros((1, 6))
        H[0, 2] = 1.0  # Measure z component
        
        # Measurement noise
        R = np.array([[BAROMETER_VARIANCE]])
        
        return H, R
    
    def estimate_vertical_velocity(
        self,
        altitude_current: float,
        altitude_previous: float,
        dt: float
    ) -> float:
        """
        Estimate vertical velocity from altitude difference.
        
        Args:
            altitude_current: Current altitude (meters)
            altitude_previous: Previous altitude (meters)
            dt: Time difference (seconds)
            
        Returns:
            Vertical velocity (m/s, positive = upward)
        """
        if dt < 1e-6:
            logger.warning("Time step too small for velocity estimation")
            return 0.0
        
        vz = (altitude_current - altitude_previous) / dt
        return vz
