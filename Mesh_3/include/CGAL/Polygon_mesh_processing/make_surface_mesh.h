// Copyright (c) 2021 GeometryFactory (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
//
// Author(s)     : Jane Tournois

#ifndef CGAL_POLYGON_MESH_PROCESSING_MAKE_SURFACE_MESH_H
#define CGAL_POLYGON_MESH_PROCESSING_MAKE_SURFACE_MESH_H

#include <CGAL/license/Mesh_3.h>

#include <CGAL/disable_warnings.h>

#include <CGAL/Mesh_triangulation_3.h>
#include <CGAL/Mesh_complex_3_in_triangulation_3.h>
#include <CGAL/Mesh_criteria_3.h>

#include <CGAL/Polyhedral_mesh_domain_with_features_3.h>
#include <CGAL/make_mesh_3.h>
#include <CGAL/facets_in_complex_3_to_triangle_mesh.h>
#include <CGAL/Mesh_facet_topology.h>

#include <CGAL/Polygon_mesh_processing/internal/named_function_params.h>
#include <CGAL/Polygon_mesh_processing/internal/named_params_helper.h>

#include <limits>

#ifdef CGAL_PMP_REMESHING_VERBOSE
#define CGAL_MESH_3_VERBOSE 1
#endif

namespace CGAL {

namespace Polygon_mesh_processing {

/*!
* \ingroup PMP_meshing_grp
* @brief remeshes a surface triangle mesh.
*
* @tparam TriangleMesh model of `FaceListGraph`
* @tparam NamedParameters a sequence of \ref bgl_namedparameters "Named Parameters"
*
* @param pmesh a triangle surface mesh
* @param out the output triangle surface mesh
* @param np an optional sequence of \ref bgl_namedparameters "Named Parameters" among the ones listed below
*
* \cgalNamedParamsBegin
*   \cgalParamNBegin{geom_traits}
*     \cgalParamDescription{an instance of a geometric traits class}
*     \cgalParamType{a class model of `Kernel`}
*     \cgalParamDefault{a \cgal Kernel deduced from the point type, using `CGAL::Kernel_traits`}
*     \cgalParamExtra{The geometric traits class must be compatible with the vertex point type.}
*     \cgalParamExtra{Exact constructions kernels are not supported by this function.}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{features_angle_bound}
*     \cgalParamDescription{The dihedral angle bound for detection of feature edges}
*     \cgalParamType{A number type `FT`, either deduced from the `geom_traits`
*          \ref bgl_namedparameters "Named Parameters" if provided,
*          or from the geometric traits class deduced from the point property map
*         of `TriangleMesh`.}
*     \cgalParamDefault{`60`}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{edge_is_constrained_map}
*     \cgalParamDescription{a property map containing the constrained-or-not status of each edge of `pmesh`}
*     \cgalParamType{a class model of `ReadWritePropertyMap` with `boost::graph_traits<TriangleMesh>::%edge_descriptor`
*                    as key type and `bool` as value type. It must be default constructible.}
*     \cgalParamDefault{a default property map where no edge is constrained}
*     \cgalParamExtra{A constrained edge can be split or collapsed, but not flipped, nor its endpoints moved by smoothing.}
*     \cgalParamExtra{Sub-edges generated by splitting are set to be constrained.}
*     \cgalParamExtra{Patch boundary edges (i.e. incident to only one face in the range) are always considered as constrained edges.}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{vertex_is_constrained_map}
*     \cgalParamDescription{a property map containing the constrained-or-not status of each vertex of `pmesh`.}
*     \cgalParamType{a class model of `ReadWritePropertyMap` with `boost::graph_traits<TriangleMesh>::%vertex_descriptor`
*                    as key type and `bool` as value type. It must be default constructible.}
*     \cgalParamDefault{a default property map where no vertex is constrained}
*     \cgalParamExtra{A constrained vertex cannot be modified during remeshing.}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{protect_constraints}
*     \cgalParamDescription{If `true`, the edges set as constrained in `edge_is_constrained_map`
*                           (or by default the boundary edges) are not split nor collapsed during remeshing.}
*     \cgalParamType{Boolean}
*     \cgalParamDefault{`false`}
*     \cgalParamExtra{Note that around constrained edges that have their length higher than
*                     twice `target_edge_length`, remeshing will fail to provide good quality results.
*                     It can even fail to terminate because of cascading vertex insertions.}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{face_patch_map}
*     \cgalParamDescription{a property map with the patch id's associated to the faces of `faces`}
*     \cgalParamType{a class model of `ReadWritePropertyMap` with `boost::graph_traits<TriangleMesh>::%face_descriptor`
*                    as key type and the desired property, model of `CopyConstructible` and `LessThanComparable` as value type.}
*     \cgalParamDefault{a default property map where each face is associated with the ID of
*                       the connected component it belongs to. Connected components are
*                       computed with respect to the constrained edges listed in the property map
*                       `edge_is_constrained_map`}
*     \cgalParamExtra{The map is updated during the remeshing process while new faces are created.}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{mesh_edge_size}
*     \cgalParamDescription{A scalar field (resp. a constant) providing a space varying
*          (resp. a uniform) upper bound for the lengths of curve edges.
*          This parameter has to be set to a positive value when 1-dimensional features protection is used
*          (when `protect_constraints` is `true`).}
*     \cgalParamType{A number type `FT` model of the concept `Field`, or a model of the concept `MeshDomainField_3`}
*     \cgalParamDefault{`(std::numeric_limits<FT>::%max)()`, with
*          `FT` a number type, either deduced from the `geom_traits` \ref bgl_namedparameters
*          "Named Parameters" if provided, or from the geometric traits class deduced from the
*          point property map of `TriangleMesh`.}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{mesh_facet_size}
*     \cgalParamDescription{A scalar field (resp. a constant) describing a space varying
*          (resp. a uniform) upper-bound for the radii of the surface Delaunay balls.}
*     \cgalParamType{A number type `FT` model of the concept `Field`, or a model of the concept
*          `MeshDomainField_3`}
*     \cgalParamDefault{`0.`}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{mesh_facet_angle}
*     \cgalParamDescription{A lower bound for the angles (in degrees) of the surface mesh facets.}
*     \cgalParamType{A number type `FT` model of the concept `Field`}
*     \cgalParamDefault{`0.`}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{mesh_facet_distance}
*     \cgalParamDescription{A scalar field (resp. a constant) describing a space varying
*          (resp. a uniform) upper bound for the distance between the facet circumcenter
*          and the center of its surface Delaunay ball.}
*     \cgalParamType{A number type `FT` model of the concept `Field`, or a model of the concept
*          `MeshDomainField_3`}
*     \cgalParamDefault{`0.`}
*   \cgalParamNEnd
*
*   \cgalParamNBegin{mesh_facet_topology}
*     \cgalParamDescription{The set of topological constraints which have to be verified
*          by each surface facet. The default value is `CGAL::FACET_VERTICES_ON_SURFACE`.
*          See `CGAL::Mesh_facet_topology` manual page to get all possible values.}
*     \cgalParamType{`CGAL::Mesh_facet_topology`}
*     \cgalParamDefault{`0`}
*   \cgalParamNEnd
* \cgalNamedParamsEnd
*/
template<typename TriangleMesh
       , typename NamedParameters>
void make_surface_mesh(const TriangleMesh& pmesh
                     , TriangleMesh& out
                     , const NamedParameters& np)
{
  using parameters::get_parameter;
  using parameters::choose_parameter;

  using TM   = TriangleMesh;
  using GT   = typename GetGeomTraits<TM, NamedParameters>::type;
  using Mesh_domain = CGAL::Polyhedral_mesh_domain_with_features_3<GT, TM>;
  using Tr   = typename CGAL::Mesh_triangulation_3<Mesh_domain>::type;
  using C3t3 = CGAL::Mesh_complex_3_in_triangulation_3<Tr,
                         typename Mesh_domain::Corner_index,
                         typename Mesh_domain::Curve_index>;
  using Mesh_criteria = CGAL::Mesh_criteria_3<Tr>;
  using FT = typename GT::FT;

  if (!CGAL::is_triangle_mesh(pmesh)) {
    std::cerr << "Input geometry is not triangulated." << std::endl;
    return;
  }

  // Create a vector with only one element: the pointer to the polyhedron.
  std::vector<const TM*> poly_ptrs_vector(1);
  poly_ptrs_vector[0] = &pmesh;

  // Create a polyhedral domain, with only one polyhedron,
  // and no "bounding polyhedron", so the volumetric part of the domain will be
  // empty.
  Mesh_domain domain(poly_ptrs_vector.begin(), poly_ptrs_vector.end());

  // Get sharp features
  const bool protect = choose_parameter(get_parameter(np, internal_np::protect_constraints), false);
  const FT angle_bound = choose_parameter(get_parameter(np, internal_np::features_angle_bound), 60.);
  if(protect)
    domain.detect_features(angle_bound); //includes detection of borders

  // Mesh criteria
  auto esize  = choose_parameter(get_parameter(np, internal_np::mesh_edge_size),
                                 (std::numeric_limits<FT>::max)());
  auto fsize  = choose_parameter(get_parameter(np, internal_np::mesh_facet_size), 0.);
  auto fangle = choose_parameter(get_parameter(np, internal_np::mesh_facet_angle), 0.);
  auto fdist  = choose_parameter(get_parameter(np, internal_np::mesh_facet_distance), 0.);
  auto ftopo  = choose_parameter(get_parameter(np, internal_np::mesh_facet_topology),
                                 CGAL::FACET_VERTICES_ON_SURFACE);

  Mesh_criteria criteria(CGAL::parameters::edge_size = esize,
                         CGAL::parameters::facet_size = fsize,
                         CGAL::parameters::facet_angle = fangle,
                         CGAL::parameters::facet_distance = fdist,
                         CGAL::parameters::facet_topology = ftopo);

  // Mesh generation
  C3t3 c3t3 = CGAL::make_mesh_3<C3t3>(domain, criteria,
                                      CGAL::parameters::no_perturb(),
                                      CGAL::parameters::no_exude());

  CGAL::facets_in_complex_3_to_triangle_mesh(c3t3, out);
}

template<typename TriangleMesh>
void make_surface_mesh(const TriangleMesh& tmesh,
                       TriangleMesh& out)
{
  make_surface_mesh(tmesh, out, parameters::all_default());
}



} //end namespace Polygon_mesh_processing
} //end namespace CGAL

#include <CGAL/enable_warnings.h>

#endif //CGAL_POLYGON_MESH_PROCESSING_MAKE_SURFACE_MESH_H
