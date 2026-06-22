"""
Physical constants and system parameters for Sentinel Nano S1.

All constants based on LaTeX specification document.
Values chosen for safety-critical indoor firefighter localization.
"""

import numpy as np

# ============================================================
# RF Physics Constants
# ============================================================

# Speed of light in vacuum (m/s)
# Used for UWB time-of-flight ranging: d = c/2 * (t_rx - t_tx)
SPEED_OF_LIGHT = 299792458.0  # m/s

# UWB frequency band (Hz)
# Sub-nanosecond pulses reduce multipath bias
UWB_CENTER_FREQ = 6.5e9  # 6.5 GHz
UWB_BANDWIDTH = 500e6    # 500 MHz

# ============================================================
# Measurement Noise Parameters
# ============================================================

# UWB ranging noise standard deviation (meters)
# Typical for DW1000/DW3000 in indoor environments
# NLOS conditions may increase this significantly
UWB_RANGE_STD = 0.15  # meters
UWB_RANGE_VARIANCE = UWB_RANGE_STD ** 2

# IMU noise parameters (body frame)
# Acceleration noise (m/s^2)
IMU_ACCEL_STD = 0.1  # m/s^2
IMU_ACCEL_VARIANCE = IMU_ACCEL_STD ** 2

# Gyroscope noise (rad/s)
IMU_GYRO_STD = 0.01  # rad/s
IMU_GYRO_VARIANCE = IMU_GYRO_STD ** 2

# IMU bias drift (per second)
IMU_ACCEL_BIAS_STD = 0.001  # m/s^2
IMU_GYRO_BIAS_STD = 0.0001  # rad/s

# Barometer noise standard deviation (Pascals)
BAROMETER_STD = 10.0  # Pa (roughly 1m altitude uncertainty)
BAROMETER_VARIANCE = BAROMETER_STD ** 2

# ============================================================
# Atmospheric Model (for barometric altitude)
# ============================================================

# Gas constant for dry air (J/(kg·K))
R_SPECIFIC = 287.05  # J/(kg·K)

# Standard temperature (Kelvin)
# T = 288.15 K = 15°C
STANDARD_TEMP = 288.15  # K

# Gravitational acceleration (m/s^2)
GRAVITY = 9.80665  # m/s^2

# Molar mass of Earth's air (kg/mol)
M_AIR = 0.0289644  # kg/mol

# Universal gas constant (J/(mol·K))
R_UNIVERSAL = 8.31447  # J/(mol·K)

# Standard sea level pressure (Pascals)
P0_STANDARD = 101325.0  # Pa

# ============================================================
# Kalman Filter Process Noise
# ============================================================

# Process noise for position (m^2)
# Accounts for model uncertainty in dead reckoning
PROCESS_NOISE_POSITION = 0.01  # m^2

# Process noise for velocity (m^2/s^2)
PROCESS_NOISE_VELOCITY = 0.1  # m^2/s^2

# Construct process noise covariance matrix Q
# Q is block diagonal for [position, velocity]
def get_process_noise_matrix(dt: float) -> np.ndarray:
    """
    Construct process noise covariance matrix Q.
    
    For discrete-time kinematics with constant acceleration noise,
    Q has the structure accounting for integrated noise over timestep dt.
    
    Args:
        dt: Time step (seconds)
        
    Returns:
        6x6 process noise covariance matrix
    """
    # Simplified Q matrix (can be refined with continuous-discrete conversion)
    q_pos = PROCESS_NOISE_POSITION * dt**2
    q_vel = PROCESS_NOISE_VELOCITY * dt
    
    Q = np.diag([q_pos, q_pos, q_pos, q_vel, q_vel, q_vel])
    return Q

# ============================================================
# Geometry and Anchor Configuration
# ============================================================

# Minimum number of anchors for 3D localization
MIN_ANCHORS_3D = 4

# Minimum number of anchors for robust operation
# Extra anchors provide redundancy against failures
MIN_ANCHORS_ROBUST = 5

# Maximum anchor distance (meters)
# Beyond this, UWB signal strength degrades significantly
MAX_ANCHOR_RANGE = 100.0  # meters

# ============================================================
# Confidence Estimation Parameters
# ============================================================

# Confidence decay factor α
# C = exp(-α * trace(P))
# Larger α → confidence decays faster with uncertainty
CONFIDENCE_DECAY_ALPHA = 0.1

# Confidence thresholds for color coding
CONFIDENCE_HIGH_THRESHOLD = 0.7   # Green
CONFIDENCE_MEDIUM_THRESHOLD = 0.4  # Yellow
# Below 0.4 → Red

# Maximum time without UWB update before confidence degrades (seconds)
MAX_TIME_WITHOUT_UPDATE = 5.0  # seconds

# ============================================================
# TDMA Scheduling
# ============================================================

# TDMA frame duration (seconds)
TDMA_FRAME_DURATION = 1.0  # seconds

# Time slot duration (milliseconds)
# T_slot = T_frame / M
TDMA_SLOT_DURATION = 0.1  # seconds (100 ms)

# Maximum number of slots per frame
MAX_TDMA_SLOTS = int(TDMA_FRAME_DURATION / TDMA_SLOT_DURATION)

# ============================================================
# Drone Mobility Constraints
# ============================================================

# Maximum drone velocity (m/s)
DRONE_MAX_VELOCITY = 5.0  # m/s

# Drone position update rate (Hz)
DRONE_UPDATE_RATE = 10.0  # Hz

# Drone altitude range (meters)
DRONE_MIN_ALTITUDE = 1.5  # meters
DRONE_MAX_ALTITUDE = 5.0  # meters

# ============================================================
# Building Constraints (for sanity checks)
# ============================================================

# Typical floor height (meters)
FLOOR_HEIGHT = 3.5  # meters

# Maximum building velocity (for rejecting impossible solutions)
# Firefighter max walking/running speed
MAX_REASONABLE_VELOCITY = 5.0  # m/s

# Maximum altitude (meters)
# Reject solutions above this
MAX_BUILDING_ALTITUDE = 100.0  # meters

# ============================================================
# Numerical Stability
# ============================================================

# Minimum condition number for matrix inversion
# If cond(A^T A) > MAX_CONDITION_NUMBER, geometry is degenerate
MAX_CONDITION_NUMBER = 1e10

# Regularization parameter for ill-conditioned matrices
REGULARIZATION_LAMBDA = 1e-6

# Convergence tolerance for iterative solvers
CONVERGENCE_TOL = 1e-6

# Maximum iterations for nonlinear solvers
MAX_ITERATIONS = 100
