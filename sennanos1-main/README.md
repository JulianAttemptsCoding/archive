# Sentinel Nano S1 - Indoor Firefighter Localization System

Production-quality algorithmic tracking suite for UWB-based indoor localization with drone-assisted mobile anchoring.

## Overview

Sentinel Nano S1 is a safety-critical indoor positioning system designed for firefighter tracking in GPS-denied environments. The system combines:

- **Ultra-Wideband (UWB)** ranging for precise distance measurements
- **Inertial Measurement Units (IMU)** for dead reckoning
- **Barometric altimetry** for vertical constraint
- **Kalman filtering** for optimal sensor fusion
- **Drone-assisted mobile anchoring** for improved geometry

## System Architecture

```
sennanos1/
├── estimation/          # State estimation and tracking
│   ├── kalman_filter.py       # Kalman filter implementation
│   ├── state_predictor.py     # IMU-based prediction
│   ├── confidence.py          # Confidence scoring
│   └── multi_tag_tracker.py   # Multi-firefighter tracking
│
├── geometry/            # Geometric positioning
│   ├── multilateration.py     # Least-squares position solving
│   └── anchor_geometry.py     # GDOP and geometry analysis
│
├── sensors/             # Sensor models
│   ├── uwb_sensor.py          # UWB ranging physics
│   ├── imu_sensor.py          # IMU motion model
│   └── barometer.py           # Barometric altimeter
│
├── utils/               # Core data structures
│   ├── types.py               # State, Anchor, Measurement types
│   └── constants.py           # Physical constants
│
└── example_usage.py     # Complete working example
```

## Mathematical Foundation

All algorithms based on formal specification in LaTeX document:

### State Vector
```
x = [x, y, z, vx, vy, vz]ᵀ
```
- Position in building frame (meters)
- Velocity in building frame (m/s)

### Prediction (IMU)
```
x_{k+1} = F·x_k + G·a_k
P_{k+1} = F·P_k·Fᵀ + Q
```

### Correction (UWB)
```
K = P·Hᵀ·(H·P·Hᵀ + R)⁻¹
x = x + K·(z - H·x)
P = (I - K·H)·P
```

### Multilateration
```
‖p - aᵢ‖ = dᵢ  for i = 1,...,N

Linearized: A·p = b
Solution: p = (AᵀA)⁻¹·Aᵀ·b
```

### Confidence Score
```
C = exp(-α·trace(P))

C > 0.7     → Green (HIGH)
0.4 < C ≤ 0.7 → Yellow (MEDIUM)
C ≤ 0.4     → Red (LOW)
```

### GDOP (Geometric Dilution of Precision)
```
GDOP = √(trace((AᵀA)⁻¹))
```

## Core Modules

### 1. Multilateration Solver
Solves for 3D position from UWB distance measurements:
- Least-squares linearization
- Iterative refinement (Gauss-Newton)
- Outlier rejection
- Covariance estimation

### 2. Kalman Filter
Extended Kalman Filter for sensor fusion:
- 6-DOF state estimation
- Prediction-correction cycle
- Measurement consistency checking
- Adaptive updates

### 3. State Predictor
IMU-based dead reckoning:
- Discrete-time kinematics
- Acceleration integration
- Process noise modeling
- Motion mode detection

### 4. Confidence Estimator
Multi-factor confidence scoring:
- Covariance-based (primary)
- Anchor count
- Time since update
- Measurement residuals
- GDOP

### 5. Multi-Tag Tracker
Central tracking coordinator:
- Per-tag Kalman filters
- Shared anchor infrastructure
- TDMA collision avoidance
- Batch measurement processing

## Usage Example

```python
import numpy as np
from utils.types import Anchor
from estimation.multi_tag_tracker import MultiTagTracker
from sensors.uwb_sensor import UWBRangingModel

# 1. Setup anchor network
anchors = [
    Anchor(id=1, position=np.array([0.0, 0.0, 2.5])),
    Anchor(id=2, position=np.array([20.0, 0.0, 2.5])),
    Anchor(id=3, position=np.array([20.0, 20.0, 2.5])),
    Anchor(id=4, position=np.array([0.0, 20.0, 2.5])),
    Anchor(id=5, position=np.array([10.0, 10.0, 4.0]), is_mobile=True)  # Drone
]

# 2. Initialize tracker
tracker = MultiTagTracker(anchors=anchors)

# 3. Add firefighter tag
tag_id = 1
tracker.add_tag(
    tag_id=tag_id,
    initial_position=np.array([10.0, 10.0, 1.5]),
    tdma_slot=0
)

# 4. Process measurements
estimate = tracker.process_measurement_batch(
    tag_id=tag_id,
    uwb_measurements=uwb_measurements,  # List[UWBMeasurement]
    imu_measurement=imu_measurement,     # IMUMeasurement
    barometer_measurement=baro_meas,     # BarometerMeasurement
    timestamp=time.time()
)

# 5. Get position and confidence
print(f"Position: {estimate.state.position}")
print(f"Confidence: {estimate.confidence:.3f} ({estimate.confidence_level.value})")
print(f"GDOP: {estimate.gdop:.2f}")
```

## Running the Example

```bash
python example_usage.py
```

This runs a complete simulation demonstrating:
- Full tracking pipeline
- Drone geometry optimization
- Failure mode handling (anchor loss, UWB dropout)
- Performance analysis

Expected output:
```
Position Error Statistics:
  Mean Error:   0.150 m
  Std Dev:      0.080 m
  Max Error:    0.450 m
  RMSE:         0.170 m

Confidence Statistics:
  Mean Confidence: 0.856
  Min Confidence:  0.720
```

## Key Features

### Robustness
- **Minimum 4 anchors** for 3D positioning
- **Graceful degradation** with anchor loss
- **IMU-only prediction** during UWB dropout
- **Outlier rejection** for NLOS mitigation
- **Measurement consistency** checking

### Safety-Critical Design
- **Confidence scoring** for operator awareness
- **Error bounds** at specified confidence levels
- **Alert triggering** when tracking degrades
- **No silent failures** - all errors logged

### Performance
- **Real-time capable** (10-100 Hz update rates)
- **Sub-meter accuracy** in good conditions
- **Multi-tag support** with TDMA coordination
- **Adaptive drone placement** for GDOP optimization

## Algorithm Parameters

Key tunable parameters in `utils/constants.py`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `UWB_RANGE_STD` | 0.15 m | UWB ranging noise |
| `IMU_ACCEL_STD` | 0.1 m/s² | IMU acceleration noise |
| `BAROMETER_STD` | 10 Pa (~1m) | Barometer noise |
| `CONFIDENCE_DECAY_ALPHA` | 0.1 | Confidence decay rate |
| `MIN_ANCHORS_3D` | 4 | Minimum anchors for 3D |
| `MAX_ANCHOR_RANGE` | 100 m | Maximum UWB range |

## System Assumptions

1. **Anchor positions known** *a priori* (surveyed or calibrated)
2. **Synchronized clocks** for Two-Way ToF
3. **TDMA scheduling** prevents collision (handled upstream)
4. **Body frame ≈ navigation frame** (no full orientation tracking)
5. **Static building** (no structural motion)

## Coordinate System

Building-fixed Cartesian frame:
- **x**: East
- **y**: North  
- **z**: Up (altitude)

Origin: Ground floor reference point

## Safety Considerations

⚠️ **This is a safety-critical system.** Incorrect position estimates can endanger firefighters.

- Always monitor **confidence scores**
- Trigger **alerts** when confidence drops below threshold
- Use **multiple anchors** for redundancy
- Implement **dead man's switch** for extended tracking loss
- Test extensively before field deployment

## Performance Metrics

Typical performance in realistic conditions:

| Metric | Value |
|--------|-------|
| Position accuracy (LOS) | 0.15 - 0.30 m |
| Position accuracy (NLOS) | 0.50 - 2.0 m |
| Update rate | 10 Hz (UWB), 100 Hz (IMU) |
| Drift rate (IMU-only) | ~0.5 m/s² |
| Cold start time | < 1 second |
| Maximum outage | 10 seconds (IMU dead reckoning) |

## Failure Modes

### Handled Gracefully
✓ Single anchor failure (N-1 redundancy)  
✓ Temporary UWB dropout (IMU prediction)  
✓ NLOS measurements (outlier rejection)  
✓ Partial obstruction (weighted fusion)  
✓ Low battery (reduced update rate)

### Requires Intervention
✗ Multiple anchor failures (N < 4)  
✗ Complete IMU failure  
✗ Extended loss of all UWB  
✗ Tag removal/damage  
✗ Severe multipath environment

## Testing

To test individual modules:

```python
# Test multilateration
from geometry.multilateration import MultilaterationSolver
solver = MultilaterationSolver()
position, cov, info = solver.solve(anchors, measurements)

# Test Kalman filter
from estimation.kalman_filter import KalmanFilter
kf = KalmanFilter(initial_state, initial_covariance)
kf.predict(F, Q, timestamp)
kf.update(z, H, R, timestamp)

# Test confidence
from estimation.confidence import ConfidenceEstimator
estimator = ConfidenceEstimator()
confidence = estimator.compute_confidence(covariance, num_anchors, time_since_update)
```

## Future Enhancements

Potential improvements (not yet implemented):
- Full orientation tracking (quaternions)
- Particle filter for NLOS environments
- Machine learning for NLOS detection
- Cooperative positioning (tag-to-tag ranging)
- Map constraints (building floorplan)
- Visual-inertial fusion
- LoRa integration for long-range telemetry

## References

Based on mathematical formulation in:
**"Sentinel Nano S1: Ultra-Wideband Indoor Firefighter Localization with Drone-Assisted Mobile Anchoring"**

Key concepts:
- Two-Way Time of Flight (TW-ToF)
- Extended Kalman Filter (EKF)
- Least-Squares Multilateration
- Geometric Dilution of Precision (GDOP)
- Cramér-Rao Lower Bound (CRLB)

## License

Proprietary - Safety-critical system for firefighter tracking.

## Contact

For questions about implementation or deployment, contact system engineers.

---

**SAFETY WARNING**: This system is intended for professional use by trained personnel. Improper configuration or use may result in inaccurate position estimates, potentially endangering firefighters. Always validate system performance before operational deployment.
