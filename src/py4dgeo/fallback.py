"""Fallback implementations for C++ components of the M3C2 algorithms """

from py4dgeo.epoch import Epoch
from py4dgeo.m3c2 import M3C2

import numpy as np
import py4dgeo._py4dgeo as _py4dgeo


def radius_workingset_finder(params: _py4dgeo.WorkingSetFinderParameters) -> np.ndarray:
    indices = params.epoch.kdtree.radius_search(params.corepoint, params.radius)
    return params.epoch.cloud[indices, :]


def cylinder_workingset_finder(
    params: _py4dgeo.WorkingSetFinderParameters,
) -> np.ndarray:
    # Cut the cylinder into N segments, perform radius searches around the
    # segment midpoints and create the union of indices
    N = 1
    max_cylinder_length = params.max_distance
    if max_cylinder_length >= params.radius:
        N = int(np.ceil(max_cylinder_length / params.radius))
    else:
        max_cylinder_length = params.radius

    r_cyl = np.sqrt(
        params.radius * params.radius
        + max_cylinder_length * max_cylinder_length / (N * N)
    )

    slabs = []
    for i in range(N):
        # Find indices around slab midpoint
        qp = params.corepoint[0, :] + np.float32(2 * i - N + 1) / np.float32(
            N
        ) * np.float32(max_cylinder_length) * params.cylinder_axis[0, :].astype("f")
        indices = params.epoch.kdtree.radius_search(qp, r_cyl)

        # Gather the points from the point cloud
        superset = params.epoch.cloud[indices, :]

        # Calculate distance from the axis and the plane perpendicular to the axis
        to_corepoint = superset.astype("d") - qp.astype("d")
        to_corepoint_plane = to_corepoint.dot(params.cylinder_axis[0, :])
        to_axis2 = np.sum(
            np.square(
                to_corepoint
                - np.multiply(
                    to_corepoint_plane[:, np.newaxis], params.cylinder_axis[0, :]
                )
            ),
            axis=1,
        )

        # Filter the points that are not within the slab
        filtered = superset[
            np.logical_and(
                to_axis2 <= params.radius * params.radius,
                np.abs(to_corepoint_plane) <= max_cylinder_length / N,
            )
        ]

        slabs.append(filtered)

    return np.concatenate(tuple(slabs))


def mean_stddev_distance(
    params: _py4dgeo.DistanceUncertaintyCalculationParameters,
) -> tuple:
    # Calculate distance
    distance = params.normal[0, :].dot(
        params.workingset2.astype("d").mean(axis=0)
        - params.workingset1.astype("d").mean(axis=0)
    )

    # Calculate variances
    variance1 = params.normal @ np.cov(params.workingset1.T) @ params.normal.T
    variance2 = params.normal @ np.cov(params.workingset2.T) @ params.normal.T

    # The structured array that describes the full uncertainty
    uncertainty = _py4dgeo.DistanceUncertainty(
        lodetection=1.96
        * (
            np.sqrt(
                variance1 / params.workingset1.shape[0]
                + variance2 / params.workingset2.shape[0]
            )
            + params.registration_error
        ),
        spread1=np.sqrt(variance1),
        num_samples1=params.workingset1.shape[0],
        spread2=np.sqrt(variance2),
        num_samples2=params.workingset2.shape[0],
    )

    return distance, uncertainty


def median_iqr_distance(
    params: _py4dgeo.DistanceUncertaintyCalculationParameters,
) -> tuple:
    distance = np.median(
        (params.workingset2.astype("d") - params.corepoint.astype("d")[0, :]).dot(
            params.normal[0, :]
        )
    ) - np.median(
        (params.workingset1.astype("d") - params.corepoint.astype("d")[0, :]).dot(
            params.normal[0, :]
        )
    )

    return distance, _py4dgeo.DistanceUncertainty()


class PythonFallbackM3C2(M3C2):
    """An implementation of M3C2 that makes use of Python fallback implementations"""

    @property
    def name(self):
        return "M3C2 (Python Fallback)"

    def callback_workingset_finder(self):
        return cylinder_workingset_finder

    def callback_distance_calculation(self):
        return mean_stddev_distance
