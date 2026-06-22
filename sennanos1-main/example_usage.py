"""
Sentinel Nano S1 - Example Usage

Demonstrates the complete tracking pipeline:
1. Setup anchors and tags
2. Simulate UWB measurements
3. Run prediction-correction cycle
4. Monitor confidence
5. Handle drone integration

This is a complete working example of the safety-critical
indoor localization system.
"""

import numpy as np
import logging
import time

# Import Sentinel Nano S1 modules
from utils.types import Anchor, State, UWBMeasurement, IMUMeasurement, BarometerMeasurement
from sensors.uwb_sensor import UWBRangingModel
from estimation.multi_tag_tracker import MultiTagTracker

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


def setup_anchor_network():
    """
    Create a realistic anchor deployment for a building.
    
    Layout: 4 fixed anchors at corners + 1 mobile drone
    Building: 20m x 20m x 10m (3 floors)
    """
    anchors = [
        # Fixed anchors at building corners
        Anchor(id=1, position=np.array([0.0, 0.0, 2.5]), is_mobile=False),
        Anchor(id=2, position=np.array([20.0, 0.0, 2.5]), is_mobile=False),
        Anchor(id=3, position=np.array([20.0, 20.0, 2.5]), is_mobile=False),
        Anchor(id=4, position=np.array([0.0, 20.0, 2.5]), is_mobile=False),
        
        # Additional fixed anchor (center, higher)
        Anchor(id=5, position=np.array([10.0, 10.0, 6.0]), is_mobile=False),
        
        # Mobile drone anchor (initially at center)
        Anchor(id=6, position=np.array([10.0, 10.0, 4.0]), is_mobile=True),
    ]
    
    logger.info(f"Deployed {len(anchors)} anchors ({len([a for a in anchors if not a.is_mobile])} fixed, {len([a for a in anchors if a.is_mobile])} mobile)")
    
    return anchors


def simulate_firefighter_trajectory(duration: float, dt: float):
    """
    Simulate a firefighter moving through a building.
    
    Trajectory: Walk from (5, 5, 0) to (15, 15, 3.5) over duration
    
    Args:
        duration: Total simulation time (seconds)
        dt: Time step (seconds)
        
    Returns:
        List of (position, velocity, timestamp) tuples
    """
    num_steps = int(duration / dt)
    
    # Start and end positions
    start_pos = np.array([5.0, 5.0, 0.0])
    end_pos = np.array([15.0, 15.0, 3.5])  # Moved up to first floor
    
    # Compute constant velocity
    velocity = (end_pos - start_pos) / duration
    
    trajectory = []
    
    for i in range(num_steps):
        t = i * dt
        position = start_pos + velocity * t
        
        # Add some noise to simulate realistic motion
        position += np.random.normal(0, 0.05, 3)
        
        trajectory.append((position, velocity, t))
    
    logger.info(f"Generated trajectory: {num_steps} steps over {duration}s")
    
    return trajectory


def simulate_uwb_measurements(
    anchors: list,
    true_position: np.ndarray,
    uwb_model: UWBRangingModel,
    timestamp: float
) -> list:
    """
    Simulate UWB range measurements from tag to all anchors.
    
    Args:
        anchors: List of anchors
        true_position: True tag position
        uwb_model: UWB ranging model
        timestamp: Measurement timestamp
        
    Returns:
        List of UWBMeasurement objects
    """
    measurements = []
    
    for anchor in anchors:
        # Simulate measurement with noise
        measurement = uwb_model.simulate_measurement(
            anchor=anchor,
            true_position=true_position,
            add_noise=True,
            nlos_bias=0.0  # Could add NLOS for realism
        )
        measurement.timestamp = timestamp
        measurements.append(measurement)
    
    return measurements


def simulate_imu_measurement(
    velocity: np.ndarray,
    timestamp: float
) -> IMUMeasurement:
    """
    Simulate IMU measurement.
    
    Args:
        velocity: Current velocity
        timestamp: Measurement timestamp
        
    Returns:
        IMUMeasurement object
    """
    # Assume constant velocity → zero acceleration (plus noise)
    acceleration = np.random.normal(0, 0.1, 3)
    angular_velocity = np.random.normal(0, 0.01, 3)
    
    return IMUMeasurement(
        acceleration=acceleration,
        angular_velocity=angular_velocity,
        timestamp=timestamp
    )


def simulate_barometer_measurement(
    altitude: float,
    timestamp: float
) -> BarometerMeasurement:
    """
    Simulate barometer measurement.
    
    Args:
        altitude: True altitude (meters)
        timestamp: Measurement timestamp
        
    Returns:
        BarometerMeasurement object
    """
    # Convert altitude to pressure (simplified)
    reference_pressure = 101325.0  # Pa (sea level)
    scale_height = 8400.0  # meters
    
    pressure = reference_pressure * np.exp(-altitude / scale_height)
    
    # Add noise
    pressure += np.random.normal(0, 10.0)
    
    return BarometerMeasurement(
        pressure=pressure,
        timestamp=timestamp,
        reference_pressure=reference_pressure,
        variance=100.0
    )


def run_simulation():
    """
    Run complete simulation of the tracking system.
    """
    logger.info("=" * 80)
    logger.info("SENTINEL NANO S1 - Indoor Firefighter Localization System")
    logger.info("=" * 80)
    
    # =========================================================================
    # 1. SETUP
    # =========================================================================
    logger.info("\n[SETUP] Initializing system...")
    
    # Create anchor network
    anchors = setup_anchor_network()
    
    # Initialize tracker
    tracker = MultiTagTracker(anchors=anchors)
    
    # Create UWB model
    uwb_model = UWBRangingModel()
    
    # Add firefighter tag
    tag_id = 1
    initial_position = np.array([5.0, 5.0, 0.0])
    tracker.add_tag(
        tag_id=tag_id,
        initial_position=initial_position,
        initial_velocity=np.zeros(3),
        tdma_slot=0
    )
    
    logger.info(f"Tag {tag_id} initialized at {initial_position}")
    
    # =========================================================================
    # 2. SIMULATION LOOP
    # =========================================================================
    logger.info("\n[SIMULATION] Starting tracking simulation...")
    
    # Simulation parameters
    duration = 30.0  # seconds
    dt = 0.1  # 10 Hz update rate
    
    # Generate trajectory
    trajectory = simulate_firefighter_trajectory(duration, dt)
    
    # Storage for results
    estimated_positions = []
    true_positions = []
    confidence_scores = []
    timestamps_list = []
    
    # Run simulation
    for i, (true_pos, true_vel, t) in enumerate(trajectory):
        timestamp = time.time() + t
        
        # Simulate measurements
        uwb_measurements = simulate_uwb_measurements(anchors, true_pos, uwb_model, timestamp)
        imu_measurement = simulate_imu_measurement(true_vel, timestamp)
        barometer_measurement = simulate_barometer_measurement(true_pos[2], timestamp)
        
        # Process measurements
        estimate = tracker.process_measurement_batch(
            tag_id=tag_id,
            uwb_measurements=uwb_measurements,
            imu_measurement=imu_measurement,
            barometer_measurement=barometer_measurement,
            timestamp=timestamp
        )
        
        # Store results
        if estimate is not None:
            estimated_positions.append(estimate.state.position.copy())
            true_positions.append(true_pos.copy())
            confidence_scores.append(estimate.confidence)
            timestamps_list.append(t)
            
            # Log every second
            if i % 10 == 0:
                error = np.linalg.norm(estimate.state.position - true_pos)
                logger.info(
                    f"t={t:.1f}s | Pos: {estimate.state.position} | "
                    f"Error: {error:.2f}m | Confidence: {estimate.confidence:.3f} ({estimate.confidence_level.value})"
                )
    
    # =========================================================================
    # 3. RESULTS ANALYSIS
    # =========================================================================
    logger.info("\n[RESULTS] Analyzing tracking performance...")
    
    estimated_positions = np.array(estimated_positions)
    true_positions = np.array(true_positions)
    confidence_scores = np.array(confidence_scores)
    
    # Compute errors
    errors = np.linalg.norm(estimated_positions - true_positions, axis=1)
    
    # Statistics
    mean_error = np.mean(errors)
    std_error = np.std(errors)
    max_error = np.max(errors)
    rmse = np.sqrt(np.mean(errors**2))
    
    mean_confidence = np.mean(confidence_scores)
    min_confidence = np.min(confidence_scores)
    
    logger.info(f"\nPosition Error Statistics:")
    logger.info(f"  Mean Error:   {mean_error:.3f} m")
    logger.info(f"  Std Dev:      {std_error:.3f} m")
    logger.info(f"  Max Error:    {max_error:.3f} m")
    logger.info(f"  RMSE:         {rmse:.3f} m")
    
    logger.info(f"\nConfidence Statistics:")
    logger.info(f"  Mean Confidence: {mean_confidence:.3f}")
    logger.info(f"  Min Confidence:  {min_confidence:.3f}")
    
    # Get final tracking summary
    summary = tracker.get_tracking_summary()
    logger.info(f"\nTracking Summary:")
    logger.info(f"  Tags: {summary['num_tags']}")
    logger.info(f"  Anchors: {summary['num_anchors']}")
    for tid, tag_info in summary['tags'].items():
        logger.info(f"  Tag {tid}:")
        logger.info(f"    Position: {tag_info['position']}")
        logger.info(f"    Confidence: {tag_info['confidence']:.3f} ({tag_info['confidence_level']})")
        logger.info(f"    GDOP: {tag_info['gdop']:.2f}")
    
    logger.info("\n" + "=" * 80)
    logger.info("SIMULATION COMPLETE")
    logger.info("=" * 80)
    
    return {
        'errors': errors,
        'confidence': confidence_scores,
        'timestamps': timestamps_list,
        'estimated_positions': estimated_positions,
        'true_positions': true_positions
    }


def demonstrate_drone_optimization():
    """
    Demonstrate drone position optimization for improved GDOP.
    """
    logger.info("\n[DRONE OPTIMIZATION] Demonstrating adaptive anchor placement...")
    
    from geometry.anchor_geometry import AnchorGeometry
    
    # Fixed anchors (suboptimal layout)
    fixed_anchors = [
        Anchor(id=1, position=np.array([0.0, 0.0, 2.5])),
        Anchor(id=2, position=np.array([20.0, 0.0, 2.5])),
        Anchor(id=3, position=np.array([10.0, 10.0, 2.5])),
        Anchor(id=4, position=np.array([15.0, 5.0, 2.5])),
    ]
    
    # Firefighter position
    tag_position = np.array([10.0, 5.0, 1.5])
    
    # Analyze geometry
    geom = AnchorGeometry()
    
    # Without drone
    gdop_without = geom.compute_gdop(fixed_anchors, tag_position)
    logger.info(f"GDOP without drone: {gdop_without:.2f}")
    
    # Suggest optimal drone position
    drone_pos = geom.suggest_drone_position(fixed_anchors, tag_position)
    logger.info(f"Suggested drone position: {drone_pos}")
    
    # With drone
    drone_anchor = Anchor(id=5, position=drone_pos, is_mobile=True)
    all_anchors = fixed_anchors + [drone_anchor]
    gdop_with = geom.compute_gdop(all_anchors, tag_position)
    logger.info(f"GDOP with drone: {gdop_with:.2f}")
    if gdop_without and gdop_with:
        logger.info(f"GDOP improvement: {((gdop_without - gdop_with) / gdop_without * 100):.1f}%")


def demonstrate_failure_modes():
    """
    Demonstrate system behavior under failure conditions.
    """
    logger.info("\n[FAILURE MODES] Testing robustness...")
    
    # Setup
    anchors = setup_anchor_network()
    tracker = MultiTagTracker(anchors=anchors)
    uwb_model = UWBRangingModel()
    
    tag_id = 1
    tracker.add_tag(tag_id, initial_position=np.array([10.0, 10.0, 1.5]))
    
    true_position = np.array([10.0, 10.0, 1.5])
    
    # Test 1: Anchor failure (lose 2 anchors)
    logger.info("\nTest 1: Simulating anchor failure...")
    reduced_anchors = anchors[:4]  # Keep only 4 anchors
    measurements = simulate_uwb_measurements(reduced_anchors, true_position, uwb_model, time.time())
    
    estimate = tracker.correct_with_uwb(tag_id, measurements, time.time())
    if estimate:
        logger.info(f"  Still tracking with {len(reduced_anchors)} anchors")
        pos_est = tracker.get_position_estimate(tag_id)
        if pos_est:
            logger.info(f"  Confidence: {pos_est.confidence:.3f}")
    
    # Test 2: UWB dropout (IMU-only prediction)
    logger.info("\nTest 2: UWB dropout - IMU-only dead reckoning...")
    for i in range(10):
        imu = simulate_imu_measurement(np.zeros(3), time.time() + i * 0.1)
        tracker.predict(tag_id, imu_measurement=imu)
    
    estimate = tracker.get_position_estimate(tag_id)
    if estimate:
        logger.info(f"  After 1s of IMU-only: Confidence: {estimate.confidence:.3f} ({estimate.confidence_level.value})")


if __name__ == "__main__":
    # Run main simulation
    results = run_simulation()
    
    # Run additional demonstrations
    demonstrate_drone_optimization()
    demonstrate_failure_modes()
    
    logger.info("\nAll demonstrations complete!")
