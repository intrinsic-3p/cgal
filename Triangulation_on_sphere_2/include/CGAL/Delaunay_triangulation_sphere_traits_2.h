// Copyright (c) 1997, 2012, 2019 INRIA Sophia-Antipolis (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the Licenxse, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0+
//
// Author(s)     : Mariette Yvinec,
//                 Claudia Werner,
//                 Mael Rouxel-Labbé

#ifndef CGAL_DELAUNAY_TRIANGULATION_SPHERE_TRAITS_2_H
#define CGAL_DELAUNAY_TRIANGULATION_SPHERE_TRAITS_2_H

#include <CGAL/enum.h>

namespace CGAL {
namespace internal {

template <typename Traits>
class Power_test_2
{
public:
  typedef typename Traits::Point_3                  Point_3;
  typedef Oriented_side                             result_type;

  Power_test_2(const Point_3& center, const Traits& traits) : _center(center), _traits(traits) { }

  Oriented_side operator()(const Point_3& p, const Point_3& q) const
  {
    const Comparison_result pq = _traits.compare_xyz_3_object()(p, q);

    if(pq == EQUAL)
      return ON_ORIENTED_BOUNDARY;

    const Comparison_result sq = _traits.compare_xyz_3_object()(_center, q);
    if(pq == sq)
      return ON_POSITIVE_SIDE;

    return ON_NEGATIVE_SIDE;
  }

  Oriented_side operator()(const Point_3& p, const Point_3& q, const Point_3& r) const
  {
    return - (_traits.coplanar_orientation_3_object()(p, q, _center, r));
  }

  Oriented_side operator()(const Point_3& p, const Point_3& q, const Point_3& r, const Point_3& s) const
  {
    return _traits.orientation_3_object()(p, q, r, s);
  }

protected:
  const Point_3& _center;
  const Traits& _traits;
};

template <typename Traits>
class Orientation_on_sphere_2
{
public:
  typedef typename Traits::Point_3                  Point_3;
  typedef Comparison_result                         result_type;

  Orientation_on_sphere_2(const Point_3& center, const Traits& traits) : _center(center), _traits(traits) { }

  Comparison_result operator()(const Point_3& p, const Point_3& q, const Point_3& r) const
  { return _traits.orientation_3_object()(_center, p, q, r); }

protected:
  const Point_3& _center;
  const Traits& _traits;
};

template <typename Traits>
class Equal_on_sphere_2
{
public:
  typedef typename Traits::Point_3                  Point_3;
  typedef bool                                      result_type;

  Equal_on_sphere_2(const Point_3& center, const Traits& traits) : _center(center), _traits(traits) { }

  bool operator()(const Point_3& p, const Point_3 q) const
  {
    return _traits.collinear_3_object()(_center, p, q) &&
            !_traits.collinear_are_ordered_along_line_3_object()(p, _center, q); // @fixme correct? strictly?
  }

protected:
  const Point_3& _center;
  const Traits& _traits;
};

template <typename Traits>
class Inside_cone_2
{
public:
  typedef typename Traits::Point_3                  Point_3;
  typedef bool                                      result_type;

  Inside_cone_2(const Point_3& center, const Traits& traits) : _center(center), _traits(traits) { }

  bool operator()(const Point_3& p, const Point_3& q, const Point_3& r) const
  {
    if(collinear(_center, p, r) || collinear(_center, q, r) || orientation(_center, p, q, r) != COLLINEAR)
      return false;

    if(collinear(_center, p, q))
      return true;

    // @fixme (?) what's going on here?
    return _traits.coplanar_orientation_3_object()(_center, p, q, r) ==
             (POSITIVE == _traits.coplanar_orientation_3_object()(_center, q, p, r));
  }

protected:
  const Point_3& _center;
  const Traits& _traits;
};

} // namespace internal

template <typename K>
class Delaunay_triangulation_sphere_traits_2
//  : public K // @tmp disabled to see what is really needed
{
  typedef K                                         Base;
  typedef Delaunay_triangulation_sphere_traits_2<K> Self;

public:
  typedef typename K::FT                            FT;
  typedef typename K::Point_3                       Point_on_sphere;
  typedef typename K::Point_3                       Point_3;
  typedef typename K::Segment_3                     Segment_3;

  typedef typename K::Compare_xyz_3                 Compare_xyz_3;
  typedef typename K::Construct_circumcenter_3      Construct_circumcenter_3;
  typedef typename K::Construct_point_3             Construct_point_3;
  typedef typename K::Construct_segment_3           Construct_segment_3;
  typedef typename K::Coplanar_orientation_3        Coplanar_orientation_3;
  typedef typename K::Orientation_3                 Orientation_3;

  typedef typename K::Compute_squared_distance_3    Construct_circumcenter_on_sphere_2; // @fixme project on sphere?
  typedef internal::Equal_on_sphere_2<Base>         Equal_on_sphere_2;
  typedef internal::Inside_cone_2<Base>             Inside_cone_2;
  typedef internal::Orientation_on_sphere_2<Base>   Orientation_on_sphere_2;
  typedef internal::Power_test_2<Base>              Power_test_2;

  Delaunay_triangulation_sphere_traits_2(const Point_3& center = CGAL::ORIGIN,
                                         const FT radius = 1,
                                         const K& k = K())
    : /*Base(k),*/ _center(center), _radius(radius)
  {
    initialize_bounds();
  }

private:
  void initialize_bounds()
  {
    const FT minDist = _radius * std::pow(2, -23); // @fixme
    const FT minRadius = _radius * (1 - std::pow(2, -50));
    const FT maxRadius = _radius * (1 + std::pow(2, -50));
    _minDistSquared = CGAL::square(minDist);
    _minRadiusSquared = CGAL::square(minRadius);
    _maxRadiusSquared = CGAL::square(maxRadius);
  }

public:
  // For 3D points (@tmp remove all of those once the concept is clear)
  Compare_xyz_3
  compare_xyz_3_object() const
  { return Compare_xyz_3(); }

  Construct_point_3
  construct_point_3_object() const
  { return Construct_point_3(); }

  Construct_segment_3
  construct_segment_3_object() const
  { return Base().construct_segment_3_object(); }

  Coplanar_orientation_3
  coplanar_orientation_3_object() const
  { return Coplanar_orientation_3(); }

  Orientation_3
  orientation_3_object() const
  { return Base().orientation_3_object(); }

  // For points on sphere
  Construct_circumcenter_on_sphere_2
  construct_circumcenter_on_sphere_2_object() const
  { return Construct_circumcenter_on_sphere_2(); }

  Equal_on_sphere_2
  equal_on_sphere_2_object() const
  { return Equal_on_sphere_2(_center, K()); } // @tmp static_cast<const Base&>(*this)

  Inside_cone_2
  inside_cone_2_object() const
  { return Inside_cone_2(_center, K()); }

  Orientation_on_sphere_2
  orientation_on_sphere_2_object() const
  { return Orientation_on_sphere_2(_center, K()); }

  Power_test_2
  power_test_2_object() const
  { return Power_test_2(_center, K()); }

public:
  const Point_3& center() const { return _center; }

  void set_center(const Point_3& center) { _center = center; }
  void set_radius(const FT radius) { _radius = radius; initialize_bounds(); }

  bool is_on_sphere(const Point_3& p) const
  {
    const FT sq_dist = K().compute_squared_distance_3_object()(p, _center); // @tmp
    return (_minRadiusSquared < sq_dist && sq_dist < _maxRadiusSquared);
  }

  bool are_points_too_close(const Point_3& p, const Point_3& q) const
  {
    return (K().compute_squared_distance_3_object()(p, q) <= _minDistSquared); // @tmp use base::
  }

protected:
  Point_3 _center;
  FT _radius;

  FT _minDistSquared; // minimal distance of two points to each other
  FT _minRadiusSquared; // minimal distance of a point from center of the sphere
  FT _maxRadiusSquared; // maximal distance of a point from center of the sphere
};

} // namespace CGAL

#endif // CGAL_DELAUNAY_TRIANGULATION_SPHERE_TRAITS_2_H
