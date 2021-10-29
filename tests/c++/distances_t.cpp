#include "catch2/catch.hpp"
#include "py4dgeo/compute.hpp"
#include "py4dgeo/kdtree.hpp"
#include "py4dgeo/py4dgeo.hpp"
#include "testsetup.hpp"

#include <vector>

using namespace py4dgeo;

TEST_CASE("M3C2 distance calculation", "[compute]")
{
  // Get a test epoch
  auto cloud = testcloud();
  Epoch epoch(cloud);
  epoch.kdtree.build_tree(10);
  epoch.kdtree.precompute(epoch.cloud, 10.0);

  std::vector<double> scales{ 1.0 };
  EigenPointCloud directions(epoch.cloud.rows(), 3);

  // Precompute the multiscale directions
  compute_multiscale_directions(epoch, epoch.cloud, scales, directions);

  // Calculate the distances
  EigenVector distances(epoch.cloud.rows(), 1);
  EigenVector uncertainties(epoch.cloud.rows(), 1);

  // We try to test all callback combinations
  auto wsfinder =
    GENERATE(radius_workingset_finder, cylinder_workingset_finder);
  auto uncertaintymeasure =
    GENERATE(no_uncertainty, standard_deviation_uncertainty);

  compute_distances(epoch.cloud,
                    2.0,
                    epoch,
                    epoch,
                    directions,
                    0.0,
                    distances,
                    uncertainties,
                    wsfinder,
                    uncertaintymeasure);

  for (IndexType i = 0; i < distances.rows(); ++i)
    REQUIRE(std::abs(distances.row(i).norm()) < 1e-8);
}

TEST_CASE("Single-direction M3C2 distance calculation", "[compute]")
{
  // Get a test epoch
  auto cloud = testcloud();
  Epoch epoch(cloud);
  epoch.kdtree.build_tree(10);
  epoch.kdtree.precompute(epoch.cloud, 10.0);

  // Single distance vector
  EigenPointCloud directions(1, 3);
  directions << 0, 0, 1;

  // Calculate the distances
  EigenVector distances(epoch.cloud.rows(), 1);
  EigenVector uncertainties(epoch.cloud.rows(), 1);

  // We try to test all callback combinations
  auto wsfinder =
    GENERATE(radius_workingset_finder, cylinder_workingset_finder);
  auto uncertaintymeasure =
    GENERATE(no_uncertainty, standard_deviation_uncertainty);

  compute_distances(epoch.cloud,
                    2.0,
                    epoch,
                    epoch,
                    directions,
                    0.0,
                    distances,
                    uncertainties,
                    wsfinder,
                    uncertaintymeasure);

  for (IndexType i = 0; i < distances.rows(); ++i)
    REQUIRE(std::abs(distances.row(i).norm()) < 1e-8);
}