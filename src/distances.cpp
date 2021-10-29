#include <Eigen/Eigen>

#include "py4dgeo/compute.hpp"
#include "py4dgeo/kdtree.hpp"
#include "py4dgeo/py4dgeo.hpp"

namespace py4dgeo {

void
compute_distances(EigenPointCloudConstRef corepoints,
                  double scale,
                  const Epoch& epoch1,
                  const Epoch& epoch2,
                  EigenPointCloudConstRef directions,
                  double max_cylinder_length,
                  EigenVectorRef distances,
                  EigenVectorRef uncertainties,
                  const WorkingSetFinderCallback& workingsetfinder,
                  const UncertaintyMeasureCallback& uncertaintycalculator)
{
  for (IndexType i = 0; i < corepoints.rows(); ++i) {
    // Either choose the ith row or the first (if there is no per-corepoint
    // direction)
    auto dir = directions.row(directions.rows() > 1 ? i : 0);

    auto subset1 = workingsetfinder(
      epoch1, scale, corepoints.row(i), dir, max_cylinder_length, i);
    auto subset2 = workingsetfinder(
      epoch2, scale, corepoints.row(i), dir, max_cylinder_length, i);

    // Distance calculation
    double dist = dir.dot(subset1.colwise().mean() - subset2.colwise().mean());
    distances(i, 0) = std::abs(dist);

    // Uncertainty calculation
    uncertainties(i, 0) = uncertaintycalculator(subset1, subset2, dir);
  }
}

EigenPointCloud
radius_workingset_finder(const Epoch& epoch,
                         double radius,
                         EigenPointCloudConstRef,
                         EigenPointCloudConstRef,
                         double,
                         IndexType core_idx)
{
  // Find the working set in the other epoch
  KDTree::RadiusSearchResult points;
  epoch.kdtree.precomputed_radius_search(core_idx, radius, points);
  return epoch.cloud(points, Eigen::all);
}

EigenPointCloud
cylinder_workingset_finder(const Epoch& epoch,
                           double radius,
                           EigenPointCloudConstRef corepoint,
                           EigenPointCloudConstRef direction,
                           double max_cylinder_length,
                           IndexType core_idx)
{
  // The search radius is the maximum of cylinder length and radius
  auto search_radius = std::max(radius, max_cylinder_length);

  // Find the points in the radius of max_cylinder_length
  KDTree::RadiusSearchResult ball_points;
  epoch.kdtree.precomputed_radius_search(core_idx, search_radius, ball_points);
  auto superset = epoch.cloud(ball_points, Eigen::all);

  // If max_cylinder_length is sufficiently small, we are done
  if (max_cylinder_length <= radius)
    return superset;

  // Calculate the squared distances to the cylinder axis
  auto distances = (superset.rowwise() - corepoint.row(0))
                     .rowwise()
                     .cross(direction.row(0))
                     .rowwise()
                     .squaredNorm()
                     .eval();

  // Non-performance oriented version of index extraction. There should
  // be a version using Eigen masks, but I could not find it.
  std::vector<Eigen::Index> indices;
  for (Eigen::Index i = 0; i < superset.rows(); ++i)
    if (distances(i, 0) < radius * radius)
      indices.push_back(i);

  // Select only those indices that are within the cylinder
  return superset(indices, Eigen::all);
}

double
variance(EigenPointCloudConstRef subset, EigenPointCloudConstRef direction)
{
  auto centered = subset.rowwise() - subset.colwise().mean();
  auto cov = (centered.adjoint() * centered) / double(subset.rows() - 1);
  auto multiplied = direction.row(0) * cov * direction.row(0).transpose();
  return multiplied.eval()(0, 0);
}

double
standard_deviation_uncertainty(EigenPointCloudConstRef set1,
                               EigenPointCloudConstRef set2,
                               EigenPointCloudConstRef direction)
{
  auto variance1 = variance(set1, direction);
  auto variance2 = variance(set2, direction);

  // Calculate the standard deviation from above variances
  return std::sqrt(variance1 + variance2);
}

} // namespace py4dgeo