"""
Anchor geometry analysis and GDOP computation.

Geometric Dilution of Precision (GDOP) measures how anchor geometry
affects positioning accuracy. Lower GDOP = better geometry.

From LaTeX Section 9:
    GDOP = sqrt(trace((A^T A)^{-1}))

Used for:
- Assessing positioning quality
- Optimizing drone placement
- Detecting degenerate anchor configurations
"""

import numpy as np
from typing import List, Optional
import logging

from utils.types import Anchor
from utils.constants import MAX_CONDITION_NUMBER, REGULARIZATION_LAMBDA

logger = logging.getLogger(__name__)


class AnchorGeometry:
    """
    Analyzes anchor geometry and computes quality metrics.
    """
    
    def __init__(self):
        """Initialize anchor geometry analyzer."""
        pass
    
    def compute_gdop(
        self,
        anchors: List[Anchor],
        tag_position: np.ndarray
    ) -> Optional[float]:
        """
        Compute Geometric Dilution of Precision (GDOP).
        
        GDOP quantifies how positioning errors are amplified by geometry.
        Lower GDOP indicates better anchor positioning relative to tag.
        
        From LaTeX: GDOP = sqrt(trace((A^T A)^{-1}))
        
        Args:
            anchors: List of anchors
            tag_position: Current or estimated tag position [x, y, z]
            
        Returns:
            GDOP value, or None if computation fails
        """
        if len(anchors) < 4:
            logger.warning("Need at least 4 anchors for GDOP computation")
            return None
        
        # Build geometry matrix (unit vectors from tag to anchors)
        G = self._build_geometry_matrix(anchors, tag_position)
        
        if G is None:
            return None
        
        try:
            # Compute (G^T G)^{-1}
            GtG = G.T @ G
            
            # Check condition number
            cond = np.linalg.cond(GtG)
            if cond > MAX_CONDITION_NUMBER:
                logger.warning(f"Ill-conditioned geometry matrix: cond = {cond:.2e}")
                # Apply regularization
                GtG += REGULARIZATION_LAMBDA * np.eye(3)
            
            GtG_inv = np.linalg.inv(GtG)
            
            # GDOP = sqrt(trace(GtG_inv))
            gdop = np.sqrt(np.trace(GtG_inv))
            
            return gdop
        
        except np.linalg.LinAlgError:
            logger.error("Failed to compute GDOP: singular matrix")
            return None
    
    def _build_geometry_matrix(
        self,
        anchors: List[Anchor],
        tag_position: np.ndarray
    ) -> Optional[np.ndarray]:
        """
        Build geometry matrix G.
        
        Each row is a unit vector from tag to anchor:
            G[i] = (a_i - p) / ||a_i - p||
        
        Args:
            anchors: List of anchors
            tag_position: Tag position [x, y, z]
            
        Returns:
            N x 3 geometry matrix, or None if invalid
        """
        N = len(anchors)
        G = np.zeros((N, 3))
        
        for i, anchor in enumerate(anchors):
            diff = anchor.position - tag_position
            dist = np.linalg.norm(diff)
            
            if dist < 1e-3:  # Tag too close to anchor
                logger.warning(f"Tag very close to anchor {anchor.id}")
                return None
            
            # Unit vector
            G[i] = diff / dist
        
        return G
    
    def compute_hdop(
        self,
        anchors: List[Anchor],
        tag_position: np.ndarray
    ) -> Optional[float]:
        """
        Compute Horizontal Dilution of Precision (HDOP).
        
        HDOP considers only horizontal (x, y) geometry,
        useful when vertical is constrained by barometer.
        
        Args:
            anchors: List of anchors
            tag_position: Tag position [x, y, z]
            
        Returns:
            HDOP value, or None if computation fails
        """
        G = self._build_geometry_matrix(anchors, tag_position)
        
        if G is None:
            return None
        
        try:
            # Extract horizontal components (x, y)
            G_horizontal = G[:, :2]
            
            GtG = G_horizontal.T @ G_horizontal
            GtG_inv = np.linalg.inv(GtG + REGULARIZATION_LAMBDA * np.eye(2))
            
            hdop = np.sqrt(np.trace(GtG_inv))
            
            return hdop
        
        except np.linalg.LinAlgError:
            logger.error("Failed to compute HDOP")
            return None
    
    def compute_vdop(
        self,
        anchors: List[Anchor],
        tag_position: np.ndarray
    ) -> Optional[float]:
        """
        Compute Vertical Dilution of Precision (VDOP).
        
        VDOP measures vertical positioning accuracy.
        
        Args:
            anchors: List of anchors
            tag_position: Tag position [x, y, z]
            
        Returns:
            VDOP value, or None if computation fails
        """
        G = self._build_geometry_matrix(anchors, tag_position)
        
        if G is None:
            return None
        
        try:
            # Extract vertical component (z)
            G_vertical = G[:, 2:3]
            
            GtG = G_vertical.T @ G_vertical
            vdop = np.sqrt(1.0 / (GtG[0, 0] + REGULARIZATION_LAMBDA))
            
            return vdop
        
        except (np.linalg.LinAlgError, ZeroDivisionError):
            logger.error("Failed to compute VDOP")
            return None
    
    def is_geometry_degenerate(
        self,
        anchors: List[Anchor],
        tag_position: np.ndarray,
        gdop_threshold: float = 20.0
    ) -> bool:
        """
        Check if anchor geometry is degenerate.
        
        Degenerate cases:
        - Anchors collinear (1D)
        - Anchors coplanar with tag (2D)
        - High GDOP value
        
        Args:
            anchors: List of anchors
            tag_position: Tag position
            gdop_threshold: GDOP threshold for degeneracy
            
        Returns:
            True if geometry is degenerate
        """
        if len(anchors) < 4:
            return True
        
        # Check GDOP
        gdop = self.compute_gdop(anchors, tag_position)
        if gdop is None or gdop > gdop_threshold:
            return True
        
        # Check if anchors are approximately coplanar
        if self._are_anchors_coplanar(anchors):
            logger.warning("Anchors are coplanar - degenerate 3D geometry")
            return True
        
        return False
    
    def _are_anchors_coplanar(
        self,
        anchors: List[Anchor],
        tolerance: float = 0.1
    ) -> bool:
        """
        Check if anchors lie approximately in a plane.
        
        Uses SVD to check if minimum singular value is small.
        
        Args:
            anchors: List of anchors
            tolerance: Coplanarity tolerance (meters)
            
        Returns:
            True if anchors are coplanar
        """
        if len(anchors) < 4:
            return True
        
        # Center anchor positions
        positions = np.array([a.position for a in anchors])
        centroid = np.mean(positions, axis=0)
        centered = positions - centroid
        
        # SVD decomposition
        try:
            _, s, _ = np.linalg.svd(centered)
            
            # If smallest singular value is small, points are coplanar
            if s[-1] < tolerance:
                return True
        except np.linalg.LinAlgError:
            logger.warning("SVD failed in coplanarity check")
            return True
        
        return False
    
    def compute_anchor_diversity_score(
        self,
        anchors: List[Anchor],
        tag_position: np.ndarray
    ) -> float:
        """
        Compute diversity score for anchor distribution.
        
        Measures how well-distributed anchors are around the tag.
        Higher score = better diversity.
        
        Based on:
        - Spatial distribution
        - Elevation angles
        - Azimuthal coverage
        
        Args:
            anchors: List of anchors
            tag_position: Tag position
            
        Returns:
            Diversity score [0, 1]
        """
        if len(anchors) < 4:
            return 0.0
        
        # Compute unit vectors from tag to anchors
        vectors = []
        for anchor in anchors:
            diff = anchor.position - tag_position
            dist = np.linalg.norm(diff)
            if dist > 1e-3:
                vectors.append(diff / dist)
        
        if len(vectors) < 4:
            return 0.0
        
        # Measure angular diversity (sum of all pairwise dot products)
        # More negative = more diverse
        diversity_sum = 0.0
        count = 0
        
        for i in range(len(vectors)):
            for j in range(i + 1, len(vectors)):
                dot = np.dot(vectors[i], vectors[j])
                diversity_sum += dot
                count += 1
        
        # Normalize: ideal case is orthogonal vectors (dot = 0)
        # Worst case is parallel vectors (dot = 1)
        avg_dot = diversity_sum / count if count > 0 else 1.0
        
        # Convert to [0, 1] score: 0 = parallel, 1 = orthogonal
        diversity_score = max(0.0, 1.0 - avg_dot)
        
        return diversity_score
    
    def suggest_drone_position(
        self,
        fixed_anchors: List[Anchor],
        tag_position: np.ndarray,
        drone_altitude_range: tuple = (2.0, 4.0)
    ) -> Optional[np.ndarray]:
        """
        Suggest optimal drone position to minimize GDOP.
        
        From LaTeX Section 9:
            arg min GDOP subject to velocity constraints
        
        Simple heuristic: position drone above tag at specified altitude,
        filling geometric gap in anchor distribution.
        
        Args:
            fixed_anchors: List of fixed anchors
            tag_position: Current tag position
            drone_altitude_range: (min, max) altitude for drone
            
        Returns:
            Suggested drone position [x, y, z], or None
        """
        if len(fixed_anchors) >= 6:
            # Already have good coverage
            return None
        
        # Compute centroid of fixed anchors
        anchor_positions = np.array([a.position for a in fixed_anchors])
        anchor_centroid = np.mean(anchor_positions, axis=0)
        
        # Find direction with least coverage
        # Simple approach: opposite of average anchor direction
        vectors_to_anchors = anchor_positions - tag_position
        avg_direction = np.mean(vectors_to_anchors, axis=0)
        
        # Normalize horizontal component
        horizontal = avg_direction[:2]
        if np.linalg.norm(horizontal) > 1e-3:
            horizontal = horizontal / np.linalg.norm(horizontal)
        else:
            horizontal = np.array([1.0, 0.0])
        
        # Place drone in opposite horizontal direction
        optimal_distance = 5.0  # meters
        optimal_horizontal = -horizontal * optimal_distance + tag_position[:2]
        
        # Choose altitude in specified range
        optimal_altitude = np.mean(drone_altitude_range)
        
        drone_position = np.array([
            optimal_horizontal[0],
            optimal_horizontal[1],
            optimal_altitude
        ])
        
        return drone_position
    
    def evaluate_anchor_configuration(
        self,
        anchors: List[Anchor],
        tag_position: np.ndarray
    ) -> dict:
        """
        Comprehensive evaluation of anchor configuration.
        
        Returns metrics useful for system diagnosis and optimization.
        
        Args:
            anchors: List of anchors
            tag_position: Tag position
            
        Returns:
            Dictionary of metrics
        """
        metrics = {
            "num_anchors": len(anchors),
            "gdop": self.compute_gdop(anchors, tag_position),
            "hdop": self.compute_hdop(anchors, tag_position),
            "vdop": self.compute_vdop(anchors, tag_position),
            "is_degenerate": self.is_geometry_degenerate(anchors, tag_position),
            "diversity_score": self.compute_anchor_diversity_score(anchors, tag_position)
        }
        
        # Add distance statistics
        distances = [anchor.distance_to(tag_position) for anchor in anchors]
        metrics["mean_distance"] = np.mean(distances)
        metrics["min_distance"] = np.min(distances)
        metrics["max_distance"] = np.max(distances)
        
        return metrics
