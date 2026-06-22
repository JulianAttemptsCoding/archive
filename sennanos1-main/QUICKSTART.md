# Quick Start Guide - Sentinel Nano S1

Get started with the indoor localization system in 5 minutes.

## Installation

```bash
# Clone or navigate to the repository
cd sennanos1

# Install dependencies
pip install -r requirements.txt
```

## Run the Example

```bash
python example_usage.py
```

You should see output like:
```
[SETUP] Initializing system...
Deployed 6 anchors (5 fixed, 1 mobile)
Tag 1 initialized at [5. 5. 0.]

[SIMULATION] Starting tracking simulation...
t=0.0s | Pos: [5.02 5.01 0.03] | Error: 0.04m | Confidence: 0.892 (green)
t=1.0s | Pos: [5.35 5.34 0.12] | Error: 0.05m | Confidence: 0.885 (green)
...

[RESULTS] Analyzing tracking performance...
Position Error Statistics:
  Mean Error:   0.150 m
  Std Dev:      0.080 m
  Max Error:    0.450 m
  RMSE:         0.170 m
```

## Basic Usage

### 1. Setup Anchors

```python
from utils.types import Anchor
import numpy as np

# Create anchor network
anchors = [
    Anchor(id=1, position=np.array([0.0, 0.0, 2.5])),    # Corner 1
    Anchor(id=2, position=np.array([20.0, 0.0, 2.5])),   # Corner 2
    Anchor(id=3, position=np.array([20.0, 20.0, 2.5])),  # Corner 3
    Anchor(id=4, position=np.array([0.0, 20.0, 2.5])),   # Corner 4
]
```

### 2. Initialize Tracker

```python
from estimation.multi_tag_tracker import MultiTagTracker

tracker = MultiTagTracker(anchors=anchors)
```

### 3. Add a Tag (Firefighter)

```python
tag_id = 1
initial_position = np.array([10.0, 10.0, 1.5])  # x, y, z in meters

tracker.add_tag(
    tag_id=tag_id,
    initial_position=initial_position,
    tdma_slot=0
)
```

### 4. Process Measurements

```python
from utils.types import UWBMeasurement

# Create UWB measurements (normally from hardware)
measurements = [
    UWBMeasurement(anchor_id=1, distance=14.1, timestamp=t, variance=0.0225),
    UWBMeasurement(anchor_id=2, distance=10.8, timestamp=t, variance=0.0225),
    UWBMeasurement(anchor_id=3, distance=11.2, timestamp=t, variance=0.0225),
    UWBMeasurement(anchor_id=4, distance=14.1, timestamp=t, variance=0.0225),
]

# Update tracking
estimate = tracker.process_measurement_batch(
    tag_id=tag_id,
    uwb_measurements=measurements,
    timestamp=t
)
```

### 5. Get Position and Confidence

```python
print(f"Position: {estimate.state.position}")        # [x, y, z] in meters
print(f"Velocity: {estimate.state.velocity}")        # [vx, vy, vz] in m/s
print(f"Confidence: {estimate.confidence:.3f}")      # 0.0 to 1.0
print(f"Level: {estimate.confidence_level.value}")   # "green", "yellow", "red"
print(f"GDOP: {estimate.gdop:.2f}")                  # Geometry quality
```

## Common Workflows

### Continuous Tracking Loop

```python
import time

while True:
    # 1. Get measurements from hardware
    uwb_measurements = get_uwb_measurements()
    imu_measurement = get_imu_measurement()
    baro_measurement = get_barometer_measurement()
    
    # 2. Update tracker
    estimate = tracker.process_measurement_batch(
        tag_id=1,
        uwb_measurements=uwb_measurements,
        imu_measurement=imu_measurement,
        barometer_measurement=baro_measurement,
        timestamp=time.time()
    )
    
    # 3. Check confidence
    if estimate.confidence < 0.4:
        print("WARNING: Low confidence!")
    
    # 4. Send to display/logging
    send_to_command_center(estimate)
    
    # 5. Update at 10 Hz
    time.sleep(0.1)
```

### Multiple Firefighters

```python
# Add multiple tags
for i in range(5):
    tracker.add_tag(
        tag_id=i,
        initial_position=initial_positions[i],
        tdma_slot=i  # Each tag gets unique slot
    )

# Track all simultaneously
for tag_id in range(5):
    estimate = tracker.get_position_estimate(tag_id)
    print(f"Tag {tag_id}: {estimate.state.position}, confidence={estimate.confidence:.3f}")
```

### Mobile Drone Anchor

```python
# Add drone as mobile anchor
drone_anchor = Anchor(id=99, position=np.array([10, 10, 4]), is_mobile=True)
tracker.add_anchor(drone_anchor)

# Update drone position as it moves
while drone_flying:
    new_drone_position = get_drone_gps_position()
    tracker.update_anchor_position(99, new_drone_position)
    
    # Continue normal tracking...
```

## Key Concepts

### State Vector
- **Position**: [x, y, z] in meters (building frame)
- **Velocity**: [vx, vy, vz] in m/s
- Together: 6-DOF state

### Confidence Score
- **0.7 - 1.0 (Green)**: High confidence, accurate tracking
- **0.4 - 0.7 (Yellow)**: Medium confidence, acceptable
- **0.0 - 0.4 (Red)**: Low confidence, may be inaccurate

Factors affecting confidence:
- Number of anchors visible
- Time since last UWB update
- Measurement quality (residuals)
- Geometry quality (GDOP)

### GDOP (Geometric Dilution of Precision)
- Measures how anchor geometry affects accuracy
- Lower is better
- < 3: Excellent
- 3-6: Good
- 6-10: Fair
- \> 10: Poor

## Troubleshooting

### "Insufficient anchors" error
**Problem**: Fewer than 4 anchors visible  
**Solution**: Add more anchors or move to area with better coverage

### High position error
**Problem**: Large residuals, poor GDOP  
**Solution**: 
- Check anchor positions are correct
- Improve anchor geometry
- Add drone for better coverage

### Low confidence
**Problem**: Confidence < 0.4  
**Solution**:
- Ensure regular UWB updates (< 1 second apart)
- Check for anchor failures
- Verify measurement quality

### Tracking drift
**Problem**: Position slowly drifts without UWB  
**Solution**:
- Increase UWB update rate
- Calibrate IMU biases
- Use barometer for vertical constraint

## Next Steps

1. Read [README.md](README.md) for complete system overview
2. Review [TECHNICAL_DESIGN.md](TECHNICAL_DESIGN.md) for algorithm details
3. Study [example_usage.py](example_usage.py) for comprehensive examples
4. Integrate with your hardware drivers
5. Tune parameters in `utils/constants.py` for your environment

## Support

For issues or questions:
1. Check documentation in README and TECHNICAL_DESIGN
2. Review example code
3. Contact system engineers

---

**Remember**: This is a safety-critical system. Always validate performance before operational deployment!
