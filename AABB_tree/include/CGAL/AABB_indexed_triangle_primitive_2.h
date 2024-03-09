// Copyright (c) 2024 GeometryFactory (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
//
// Author(s)     : Sven Oesau
//


#ifndef CGAL_AABB_INDEXED_TRIANGLE_PRIMITIVE_2_H_
#define CGAL_AABB_INDEXED_TRIANGLE_PRIMITIVE_2_H_

#include <CGAL/license/AABB_tree.h>

#include <CGAL/AABB_primitive.h>
#include <iterator>

namespace CGAL {

namespace internal {
  template <class GeomTraits, class Iterator, class PointIterator>
  struct Triangle_2_from_index_range_iterator_property_map {
    //classical typedefs
    typedef Iterator key_type;
    typedef typename GeomTraits::Triangle_2 value_type;
    typedef typename GeomTraits::Triangle_2& reference;

    typedef boost::readable_property_map_tag category;
    typedef Triangle_2_from_index_range_iterator_property_map<GeomTraits, Iterator, PointIterator> Self;

    Triangle_2_from_index_range_iterator_property_map() {}
    Triangle_2_from_index_range_iterator_property_map(PointIterator b) : begin(b) {}

    inline friend value_type
      get(Self s, key_type it)
    {
      return typename GeomTraits::Construct_triangle_2()(s.begin[(*it)[0]], s.begin[(*it)[1]], s.begin[(*it)[2]]);
    }

    PointIterator begin;
  };

  template <class GeomTraits, class Iterator, class PointIterator>
  struct Point_from_indexed_triangle_2_iterator_property_map {
    //classical typedefs
    typedef Iterator key_type;
    typedef typename PointIterator::value_type value_type;
    typedef const value_type& reference;

    typedef boost::readable_property_map_tag category;
    typedef Point_from_indexed_triangle_2_iterator_property_map<GeomTraits, Iterator, PointIterator> Self;

    Point_from_indexed_triangle_2_iterator_property_map() {}
    Point_from_indexed_triangle_2_iterator_property_map(PointIterator b) : begin(b) {}

    inline friend reference
    get(Self s, key_type it)
    {
      return s.begin[((*it)[0])];
    }

    PointIterator begin;
  };
}//namespace internal


/*!
 * \ingroup PkgAABBTreeRef
 * Primitive type that uses as identifier an iterator with a range of 3 indices as `value_type`.
 * The iterator from which the primitive is built should not be invalided
 * while the AABB tree holding the primitive is in use.
 *
 * \cgalModels{AABBPrimitive}
 *
 * \tparam GeomTraits is a traits class providing the nested type `Point_2` and `Triangle_2`.
 *         It also provides the functor `Construct_triangle_2` that has an operator taking 3 `Point_2` as
 *         parameters and returns a `Triangle_2`
 * \tparam IndexIterator is a model of `ForwardIterator` with its value type being a `RandomAccessRange` of size 3 with an index type as `value_type`, e.g., `uint8_t`, `uint16_t` or int.
 * \tparam PointRange is a model of `RandomAccessRange` with its value type being a `Point_2`.
 * \tparam CacheDatum is either `CGAL::Tag_true` or `CGAL::Tag_false`. In the former case,
 *           the datum is stored in the primitive, while in the latter it is
 *           constructed on the fly to reduce the memory footprint.
 *           The default is `CGAL::Tag_false` (datum is not stored).
 *
 * \sa `AABBPrimitive`
 * \sa `AABB_primitive<Id,ObjectPropertyMap,PointPropertyMapPolyhedron,ExternalPropertyMaps,CacheDatum>`
 * \sa `AABB_segment_primitive_2<Iterator,CacheDatum>`
 * \sa `AABB_halfedge_graph_segment_primitive<HalfedgeGraph,OneHalfedgeGraphPerTree,CacheDatum>`
 * \sa `AABB_face_graph_triangle_primitive<FaceGraph,OneFaceGraphPerTree,CacheDatum>`
 */
template < class GeomTraits,
           class IndexIterator,
           class PointRange,
           class CacheDatum=Tag_false>
class AABB_indexed_triangle_primitive_2
#ifndef DOXYGEN_RUNNING
  : public AABB_primitive<  IndexIterator,
                            internal::Triangle_2_from_index_range_iterator_property_map<GeomTraits, IndexIterator, typename PointRange::iterator>,
                            internal::Point_from_indexed_triangle_2_iterator_property_map<GeomTraits, IndexIterator, typename PointRange::iterator>,
                            Tag_true,
                            CacheDatum >
#endif
{
  typedef AABB_primitive< IndexIterator,
                          internal::Triangle_2_from_index_range_iterator_property_map<GeomTraits, IndexIterator, typename PointRange::iterator>,
                          internal::Point_from_indexed_triangle_2_iterator_property_map<GeomTraits, IndexIterator, typename PointRange::iterator>,
                          Tag_true,
                          CacheDatum > Base;
public:
  ///constructor from an iterator
  AABB_indexed_triangle_primitive_2(IndexIterator it, PointRange &range) : Base(it) {}

  /// \internal
  static typename Base::Shared_data construct_shared_data(PointRange &range) {
    return std::make_pair(
      internal::Triangle_2_from_index_range_iterator_property_map<GeomTraits, IndexIterator, typename PointRange::iterator>(range.begin()),
      internal::Point_from_indexed_triangle_2_iterator_property_map<GeomTraits, IndexIterator, typename PointRange::iterator>(range.begin()));
  }
};

}  // end namespace CGAL

#endif // CGAL_AABB_INDEXED_TRIANGLE_PRIMITIVE_2_H_
