// Copyright (c) 2015  Geometry Factory
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
//
// Author(s) : Simon Giraudot

#ifndef CGAL_WRITE_PLY_POINTS_H
#define CGAL_WRITE_PLY_POINTS_H

#include <CGAL/license/Point_set_processing_3.h>


#include <CGAL/property_map.h>
#include <CGAL/point_set_processing_assertions.h>
#include <CGAL/IO/read_ply_points.h>

#include <boost/version.hpp>

#include <iostream>
#include <iterator>

namespace CGAL {

namespace PLY {

  /**
     \ingroup PkgPointSetProcessing
     
     Generates a PLY property handler to write 3D points. Points are
     written as 3 PLY properties of type `double` and named `x`, `y`
     and `z`.

     \sa `write_ply_points_with_properties()`

     \tparam PointMap the property map used to store points.
  */
  template <typename PointMap>
  cpp11::tuple<PointMap, Property<double>, Property<double>, Property<double> >
  point_writer(PointMap point_map)
  {
    return cpp11::make_tuple (point_map, Property<double>("x"), Property<double>("y"), Property<double>("z"));
  }

  /**
     \ingroup PkgPointSetProcessing
     
     Generates a PLY property handler to write 3D normal
     vectors. Vectors are written as 3 PLY properties of type `double`
     and named `nx`, `ny` and `nz`.

     \sa `write_ply_points_with_properties()`

     \tparam VectorMap the property map used to store vectors.
  */
  template <typename VectorMap>
  cpp11::tuple<VectorMap, Property<double>, Property<double>, Property<double> >
  normal_writer(VectorMap normal_map)
  {
    return cpp11::make_tuple (normal_map, Property<double>("nx"), Property<double>("ny"), Property<double>("nz"));
  }

  /// \cond SKIP_IN_MANUAL

namespace internal {

  template <typename T> void property_header_type (std::ostream& stream) { stream << "undefined_type"; }

  template <> void property_header_type<char> (std::ostream& stream) { stream << "char"; }
  template <> void property_header_type<unsigned char> (std::ostream& stream) { stream << "uchar"; }
  template <> void property_header_type<short> (std::ostream& stream) { stream << "short"; }
  template <> void property_header_type<unsigned short> (std::ostream& stream) { stream << "ushort"; }
  template <> void property_header_type<int> (std::ostream& stream) { stream << "int"; }
  template <> void property_header_type<unsigned int> (std::ostream& stream) { stream << "uint"; }
  template <> void property_header_type<float> (std::ostream& stream) { stream << "float"; }
  template <> void property_header_type<double> (std::ostream& stream) { stream << "double"; }


  
  template <typename T>
  void property_header (std::ostream& stream, const PLY::Property<T>& prop)
  {
    stream << "property ";
    property_header_type<T>(stream);
    stream << " " << prop.name << std::endl;
  }

  
  template <std::size_t N>
  struct Properties_header
  {
    template <class PLY_property_tuple>
    static void write(std::ostream& stream, PLY_property_tuple& wrappers)
    {
      Properties_header<N-1>::write(stream, wrappers);
      property_header (stream, std::get<N+1>(wrappers));
    }
  };
  template <>
  struct Properties_header<0>
  {
    template <class PLY_property_tuple>
    static void write(std::ostream& stream, PLY_property_tuple& wrappers)
    {
      property_header (stream, std::get<1>(wrappers));
    }
  };

  template <typename PropertyMap,
            typename ... T>
  void output_property_header (std::ostream& stream,
                               cpp11::tuple<PropertyMap, PLY::Property<T>... >& current)
  {
    Properties_header<sizeof...(T)-1>::write(stream, current); 
  }


  template <typename PropertyMap,
            typename T>
  void output_property_header (std::ostream& stream,
                               std::pair<PropertyMap, PLY::Property<T> >& current)
  {
    property_header (stream, current.second);
  }

  template <typename PropertyMap,
            typename T,
            typename NextPropertyHandler,
            typename ... PropertyHandler>
  void output_property_header (std::ostream& stream,
                               std::pair<PropertyMap, PLY::Property<T> >& current,
                               NextPropertyHandler& next,
                               PropertyHandler&& ... properties)
  {
    property_header (stream, current.second);
    output_property_header (stream, next, properties...);
  }
  template <typename PropertyMap,
            typename ... T,
            typename NextPropertyHandler,
            typename ... PropertyHandler>
  void output_property_header (std::ostream& stream,
                               cpp11::tuple<PropertyMap, PLY::Property<T>... >& current,
                               NextPropertyHandler& next,
                               PropertyHandler&& ... properties)
  {
    Properties_header<sizeof...(T)-1>::write(stream, current); 
    output_property_header (stream, next, properties...);
  }


  template <typename ForwardIterator,
            typename PropertyMap>
  void property_write (std::ostream& stream, ForwardIterator it, PropertyMap map)
  {
    stream << get (map, *it);
  }

  template <typename ForwardIterator,
            typename PropertyMap>
  void simple_property_write (std::ostream& stream, ForwardIterator it, PropertyMap map)
  {
    if (CGAL::get_mode(stream) == IO::ASCII)
      stream << get (map, *it);
    else
      {
        typename PropertyMap::value_type value = get(map, *it);
        stream.write (reinterpret_cast<char*>(&value), sizeof(value));
      }
  }

  template <typename ForwardIterator,
            typename PropertyMap,
            typename ... T>
  void output_properties (std::ostream& stream,
                          ForwardIterator it,
                          cpp11::tuple<PropertyMap, PLY::Property<T>... >& current)
  {
    property_write (stream, it, std::get<0>(current));
    if (get_mode(stream) == IO::ASCII)
      stream << std::endl;
  }


  template <typename ForwardIterator,
            typename PropertyMap,
            typename T>
  void output_properties (std::ostream& stream,
                          ForwardIterator it,
                          std::pair<PropertyMap, PLY::Property<T> >& current)
  {
    simple_property_write (stream, it, current.first);
    if (get_mode(stream) == IO::ASCII)
      stream << std::endl;
  }

  template <typename ForwardIterator,
            typename PropertyMap,
            typename T,
            typename NextPropertyHandler,
            typename ... PropertyHandler>
  void output_properties (std::ostream& stream,
                          ForwardIterator it,
                          std::pair<PropertyMap, PLY::Property<T> >& current,
                          NextPropertyHandler& next,
                          PropertyHandler&& ... properties)
  {
    simple_property_write (stream, it, current.first);
    if (get_mode(stream) == IO::ASCII)
      stream << " ";
    output_properties (stream, it, next, properties...);
  }
  
  template <typename ForwardIterator,
            typename PropertyMap,
            typename ... T,
            typename NextPropertyHandler,
            typename ... PropertyHandler>
  void output_properties (std::ostream& stream,
                          ForwardIterator it,
                          cpp11::tuple<PropertyMap, PLY::Property<T>... >& current,
                          NextPropertyHandler& next,
                          PropertyHandler&& ... properties)
  {
    property_write (stream, it, std::get<0>(current));
    if (get_mode(stream) == IO::ASCII)
      stream << " ";
    output_properties (stream, it, next, properties...);
  }
}

  /// \endcond
}



//===================================================================================
/// \ingroup PkgPointSetProcessing
/// Saves the [first, beyond) range of points with properties to a
/// .ply stream. PLY is either ASCII or binary depending on the value
/// of `CGAL::get_mode(stream)`.
///
/// Properties are handled through a variadic list of property
/// handlers. A property handle can either be:
///
///  - A `std::pair<PropertyMap, PLY::Property<T> >` if the user wants
///  to write a scalar value T as a PLY property (for example, writing
///  an `int` variable as an `int` PLY property).
///
///  - A `CGAL::cpp11::tuple<PropertyMap, PLY::Property<T>...>` if the
///  user wants to write a complex object as several PLY
///  properties. In that case, and overload of the stream operator
///  `std::operator<<()` must be provided for
///  `PropertyMap::value_type` that handles both ASCII and binary
///  output (see `CGAL::get_mode()`).
///
/// @sa `PLY::point_writer()`
/// @sa `PLY::normal_writer()`
///
/// @tparam ForwardIterator iterator over input points.
/// @tparam PropertyHandler handlers to recover properties.
///
/// @return true on success.
template < typename ForwardIterator,
           typename ... PropertyHandler>
bool
write_ply_points_with_properties(
  std::ostream& stream, ///< output stream.
  ForwardIterator first,  ///< iterator over the first input point.
  ForwardIterator beyond, ///< past-the-end iterator over the input points.
  PropertyHandler&& ... properties) ///< parameter pack of property handlers
{
  CGAL_point_set_processing_precondition(first != beyond);

  if(!stream)
  {
    std::cerr << "Error: cannot open file" << std::endl;
    return false;
  }

  // Write header
  stream << "ply" << std::endl
         << ((get_mode(stream) == IO::BINARY) ? "format binary_little_endian 1.0" : "format ascii 1.0") << std::endl
         << "comment Generated by the CGAL library" << std::endl
         << "element vertex " << std::distance (first, beyond) << std::endl;
  
  PLY::internal::output_property_header (stream, properties...);
  
  stream << "end_header" << std::endl;
  

  // Write positions + normals
  for(ForwardIterator it = first; it != beyond; it++)
  {
    PLY::internal::output_properties (stream, it, properties...);
  }

  return ! stream.fail();

}

//===================================================================================
/// \ingroup PkgPointSetProcessing
/// Saves the [first, beyond) range of points (positions + normals) to
/// a .ply stream. PLY is either ASCII or binary depending on the
/// value of `CGAL::get_mode(stream)`.
///
/// \pre normals must be unit vectors
///
/// @tparam ForwardIterator iterator over input points.
/// @tparam PointMap is a model of `ReadablePropertyMap` with  value type `Point_3<Kernel>`.
///        It can be omitted if the value type of `ForwardIterator` is convertible to `Point_3<Kernel>`.
/// @tparam VectorMap is a model of `ReadablePropertyMap` with a value type  `Vector_3<Kernel>`.
///
/// @return true on success.

// This variant requires all parameters.
template < typename ForwardIterator,
           typename PointMap,
           typename VectorMap >
bool
write_ply_points_and_normals(
  std::ostream& stream, ///< output stream.
  ForwardIterator first, ///< first input point.
  ForwardIterator beyond, ///< past-the-end input point.
  PointMap point_map, ///< property map: value_type of OutputIterator -> Point_3.
  VectorMap normal_map) ///< property map: value_type of OutputIterator -> Vector_3.
{
  return write_ply_points_with_properties(
    stream,
    first, beyond,
    PLY::point_writer(point_map),
    PLY::normal_writer(normal_map));
}

/// @cond SKIP_IN_MANUAL
// This variant creates a default point property map = Identity_property_map.
template <typename ForwardIterator,
          typename VectorMap
>
bool
write_ply_points_and_normals(
  std::ostream& stream, ///< output stream.
  ForwardIterator first, ///< first input point.
  ForwardIterator beyond, ///< past-the-end input point.
  VectorMap normal_map) ///< property map: value_type of OutputIterator -> Vector_3.
{
  return write_ply_points_and_normals(
    stream,
    first, beyond,
    make_identity_property_map(
    typename std::iterator_traits<ForwardIterator>::value_type()),
    normal_map);
}
/// @endcond


//===================================================================================
/// \ingroup PkgPointSetProcessing
/// Saves the [first, beyond) range of points (positions only) to a
/// .ply stream. PLY is either ASCII or binary depending on the value
/// of `CGAL::get_mode(stream)`.
///
/// @tparam ForwardIterator iterator over input points.
/// @tparam PointMap is a model of `ReadablePropertyMap` with a value_type = `Point_3<Kernel>`.
///        It can be omitted if the value type of `ForwardIterator` is convertible to `Point_3<Kernel>`.
///
/// @return true on success.

// This variant requires all parameters.
template < typename ForwardIterator,
           typename PointMap >
bool
write_ply_points(
  std::ostream& stream, ///< output stream.
  ForwardIterator first, ///< first input point.
  ForwardIterator beyond, ///< past-the-end input point.
  PointMap point_map) ///< property map: value_type of OutputIterator -> Point_3.
{
  return write_ply_points_with_properties (stream, first, beyond, PLY::point_writer(point_map));
}

/// @cond SKIP_IN_MANUAL
// This variant creates a default point property map = Identity_property_map.
template < typename ForwardIterator >
bool
write_ply_points(
  std::ostream& stream, ///< output stream.
  ForwardIterator first, ///< first input point.
  ForwardIterator beyond) ///< past-the-end input point.
{
  return write_ply_points(
    stream,
    first, beyond,
    make_identity_property_map(
    typename std::iterator_traits<ForwardIterator>::value_type())
    );
}
/// @endcond


} //namespace CGAL

#endif // CGAL_WRITE_PLY_POINTS_H
