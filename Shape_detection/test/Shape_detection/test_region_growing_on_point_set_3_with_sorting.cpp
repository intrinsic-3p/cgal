// STL includes.
#include <vector>
#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cassert>

// CGAL includes.
#include <CGAL/assertions.h>
#include <CGAL/property_map.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

#include <CGAL/Point_set_3.h>
#include <CGAL/Point_set_3/IO.h>

#include <CGAL/Shape_detection/Region_growing/Region_growing.h>
#include <CGAL/Shape_detection/Region_growing/Region_growing_on_point_set.h>
#include <CGAL/Shape_detection/Region_growing/internal/free_functions.h>

namespace SD = CGAL::Shape_detection;
using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;

using FT      = typename Kernel::FT;
using Point_3 = typename Kernel::Point_3;

using Input_range = CGAL::Point_set_3<Point_3>;
using Point_map   = typename Input_range::Point_map;
using Normal_map  = typename Input_range::Vector_map;

using Neighbor_query = SD::Point_set::K_neighbor_query<Kernel, Input_range, Point_map>;
using Region_type    = SD::Point_set::Least_squares_plane_fit_region<Kernel, Input_range, Point_map, Normal_map>;
using Sorting        = SD::Point_set::Least_squares_plane_fit_sorting<Kernel, Input_range, Neighbor_query, Point_map>;
using Region_growing = SD::Region_growing<Input_range, Neighbor_query, Region_type, typename Sorting::Seed_map>;

int main(int argc, char *argv[]) {

  // Default parameter values.
  const std::size_t k                  = 12;
  const FT          distance_threshold = FT(2);
  const FT          angle_threshold    = FT(20);
  const std::size_t min_region_size    = 50;

  // Load data.
  std::ifstream in(argc > 1 ? argv[1] : "data/point_set_3.xyz");
  CGAL::set_ascii_mode(in);
  assert(in);

  const bool with_normal_map = true;
  Input_range input_range(with_normal_map);
  in >> input_range;
  in.close();
  assert(input_range.size() == 8075);

  // Create parameter classes.
  Neighbor_query neighbor_query(
    input_range, CGAL::parameters::
    neighbor_radius(k).
    point_map(input_range.point_map()));

  Region_type region_type(
    input_range,
    CGAL::parameters::
    distance_threshold(distance_threshold).
    angle_threshold(angle_threshold).
    min_region_size(min_region_size).
    point_map(input_range.point_map()).
    normal_map(input_range.normal_map()));

  // Sort indices.
  Sorting sorting(
    input_range, neighbor_query,
    CGAL::parameters::point_map(input_range.point_map()));
  sorting.sort();

  // Run region growing.
  Region_growing region_growing(
    input_range, neighbor_query, region_type, sorting.seed_map());

  std::vector< std::vector<std::size_t> > regions;
  region_growing.detect(std::back_inserter(regions));
  region_growing.release_memory();
  assert(regions.size() == 7);

  // Test free functions and stability.
  for (std::size_t k = 0; k < 3; ++k) {
    regions.clear();
    SD::internal::region_growing_planes(
      input_range, std::back_inserter(regions),
      CGAL::parameters::
      distance_threshold(distance_threshold).
      angle_threshold(angle_threshold).
      min_region_size(min_region_size).
      point_map(input_range.point_map()).
      normal_map(input_range.normal_map()));
    assert(regions.size() == 7);
  }

  std::cout << "rg_sortpoints3, epick_test_success: " << true << std::endl;
  return EXIT_SUCCESS;
}
