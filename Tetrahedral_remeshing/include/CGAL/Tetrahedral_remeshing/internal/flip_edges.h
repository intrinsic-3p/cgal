// Copyright (c) 2020 GeometryFactory (France) and Telecom Paris (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org)
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
//
// Author(s)     : Jane Tournois, Noura Faraj, Jean-Marc Thiery, Tamy Boubekeur

#ifndef CGAL_INTERNAL_FLIP_EDGES_H
#define CGAL_INTERNAL_FLIP_EDGES_H

#include <CGAL/license/Tetrahedral_remeshing.h>

#include <CGAL/Triangulation_utils_3.h>
#include <CGAL/utility.h>

#include <CGAL/Tetrahedral_remeshing/internal/tetrahedral_remeshing_helpers.h>

#include <boost/container/small_vector.hpp>
#include <optional>
#include <boost/functional/hash.hpp>

#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <queue>

namespace CGAL
{
namespace Tetrahedral_remeshing
{
namespace internal
{
enum Flip_Criterion{ MIN_ANGLE_BASED, AVERAGE_ANGLE_BASED,
                     VALENCE_BASED, VALENCE_MIN_DH_BASED };

//outer_mirror_facets contains the set of facets of the outer hull
//of the set of cells modified by the flip operation,
//"seen from" outside
//i.e. for each facet f among those, f.first has not been modified by flip
template<typename C3t3, typename CellSet, typename FacetSet>
void update_c3t3_facets(C3t3& c3t3,
                        const CellSet& cells_to_update,
                        const FacetSet& outer_mirror_facets)
{
  typedef typename C3t3::Facet       Facet;
  typedef typename C3t3::Cell_handle Cell_handle;
  typedef typename C3t3::Surface_patch_index Surface_patch_index;

  for (Cell_handle c : cells_to_update)
  {
    //their subdomain indices have not been modified because we kept the same cells
    //surface patch indices need to be fixed though
    for (int i = 0; i < 4; ++i)
    {
      const Facet f(c, i);
      const Facet mf = c3t3.triangulation().mirror_facet(f);
      if (outer_mirror_facets.find(mf) != outer_mirror_facets.end())
      {
        //we are on the border of the modified zone, c3t3 info is valid outside,
        //on mirror facet
        const typename C3t3::Surface_patch_index patch = c3t3.surface_patch_index(mf);
        if (c3t3.is_in_complex(mf))
          f.first->set_surface_patch_index(f.second, patch);
        else
          f.first->set_surface_patch_index(f.second, Surface_patch_index());
      }
      else
      {
        //we are inside the modified zone, c3t3 info is not valid anymore
        if (c3t3.is_in_complex(f) || c3t3.is_in_complex(mf))
        {
          f.first->set_surface_patch_index(f.second, Surface_patch_index());
          mf.first->set_surface_patch_index(mf.second, Surface_patch_index());
        }
      }
    }
  }
}

template<typename C3t3, typename IncCellsVectorMap, typename Cell_selector>
Sliver_removal_result flip_3_to_2(typename C3t3::Edge& edge,
                                  C3t3& c3t3,
                                  const std::vector<typename C3t3::Vertex_handle>& vertices_around_edge,
                                  const Flip_Criterion& criterion,
                                  IncCellsVectorMap& inc_cells,
                                  Cell_selector& cell_selector)
{
  typedef typename C3t3::Triangulation Tr;
  typedef typename C3t3::Facet         Facet;
  typedef typename C3t3::Vertex_handle Vertex_handle;
  typedef typename C3t3::Cell_handle   Cell_handle;
  typedef typename Tr::Cell_circulator Cell_circulator;
  typedef typename Tr::Geom_traits     Gt;
  typedef typename Gt::FT              FT;

  //Edge to face flip
  Tr& tr = c3t3.triangulation();

  Cell_circulator circ = tr.incident_cells(edge);
  Cell_circulator done = circ;

  Vertex_handle vh0 = edge.first->vertex(edge.second);
  Vertex_handle vh1 = edge.first->vertex(edge.third);

  //Select 2 cells to keep and update and one to remove
  Cell_handle ch0 = Cell_handle(circ++);
  Cell_handle ch1 = Cell_handle(circ++);
  Cell_handle cell_to_remove = Cell_handle(circ++);
  if (circ != done)
  {
    std::cout << "Wrong flip function" << std::endl;
    return NOT_FLIPPABLE;
  }

  //Check structural validity
  Cell_handle c;
  int i0, i1, i3;
  if (tr.is_facet(vertices_around_edge[0], vertices_around_edge[1], vertices_around_edge[2],
                  c, i0, i1, i3))
    return NOT_FLIPPABLE;

  //Check topological validity
  const typename C3t3::Subdomain_index subdomain = ch0->subdomain_index();
  if ( subdomain != ch1->subdomain_index()
       || subdomain != cell_to_remove->subdomain_index()
       || ch1->subdomain_index() != cell_to_remove->subdomain_index())
    return NOT_FLIPPABLE;

  Vertex_handle vh2;
  Vertex_handle vh3;

  for (int i = 0; i < 3; ++i){
    if (!ch0->has_vertex(vertices_around_edge[i]))
      vh2 = vertices_around_edge[i];
    else if (!ch1->has_vertex(vertices_around_edge[i]))
      vh3 = vertices_around_edge[i];
  }

  int vh0_id = ch0->index(vh0);
  int vh1_id = ch1->index(vh1);

  //Check if flip valid
  if (!is_well_oriented(tr, vh2,
                        ch0->vertex(indices(vh0_id, 0)),
                        ch0->vertex(indices(vh0_id, 1)),
                        ch0->vertex(indices(vh0_id, 2)))
      || !is_well_oriented(tr, vh3,
                           ch1->vertex(indices(vh1_id, 0)),
                           ch1->vertex(indices(vh1_id, 1)),
                           ch1->vertex(indices(vh1_id, 2))))
    return NOT_FLIPPABLE;

  ///********************VALIDITY CHECK***************************/
  //double curr_min_dh;
  //bool check_validity = false;
  //std::vector<typename Tr::Tetrahedron> pre_sliver_Removal_cells;
  //if (check_validity){
  //  pre_sliver_Removal_cells.clear();
  //  pre_sliver_Removal_cells.push_back(K::Tetrahedron_3(ch0->vertex(0)->point(), ch0->vertex(1)->point(), ch0->vertex(2)->point(), ch0->vertex(3)->point()));
  //  pre_sliver_Removal_cells.push_back(K::Tetrahedron_3(ch1->vertex(0)->point(), ch1->vertex(1)->point(), ch1->vertex(2)->point(), ch1->vertex(3)->point()));
  //  pre_sliver_Removal_cells.push_back(K::Tetrahedron_3(cell_to_remove->vertex(0)->point(), cell_to_remove->vertex(1)->point(),
  //    cell_to_remove->vertex(2)->point(), cell_to_remove->vertex(3)->point()));

  //  curr_min_dh = min_dihedral_angle<Gt>(ch0);
  //  curr_min_dh = std::min(curr_min_dh, min_dihedral_angle<Gt>(ch1));
  //  curr_min_dh = std::min(curr_min_dh, min_dihedral_angle<Gt>(cell_to_remove));

  //  pre_sliver_Removal_vertices.clear();
  //  for (int i = 0; i < vertices_around_edge.size(); ++i){
  //    pre_sliver_Removal_vertices.push_back(Point_3(vertices_around_edge[i]->point()));
  //  }

  //  previous_edges.clear();
  //  previous_edges.push_back(std::make_pair(vh0->point(), vh1->point()));
  //}
  /*************************************************************/


  if (criterion == MIN_ANGLE_BASED)
  {
    //Current worst dihedral angle
    Dihedral_angle_cosine curr_max_cosdh = max_cos_dihedral_angle(tr, ch0);
    curr_max_cosdh = (std::max)(curr_max_cosdh, max_cos_dihedral_angle(tr, ch1));
    curr_max_cosdh = (std::max)(curr_max_cosdh, max_cos_dihedral_angle(tr, cell_to_remove));

    //Result worst dihedral angle
    if (curr_max_cosdh < max_cos_dihedral_angle(tr, vh2,
                                         ch0->vertex(indices(vh0_id, 0)),
                                         ch0->vertex(indices(vh0_id, 1)),
                                         ch0->vertex(indices(vh0_id, 2)))
        || curr_max_cosdh < max_cos_dihedral_angle(tr, vh3,
                                         ch1->vertex(indices(vh1_id, 0)),
                                         ch1->vertex(indices(vh1_id, 1)),
                                         ch1->vertex(indices(vh1_id, 2))))
      return NO_BEST_CONFIGURATION;
  }
  else if (criterion == AVERAGE_ANGLE_BASED)
  {
    //Current worst dihedral angle
    double average_min_dh = min_dihedral_angle(tr, ch0);
    average_min_dh += min_dihedral_angle(tr, ch1);
    average_min_dh += min_dihedral_angle(tr, cell_to_remove);

    average_min_dh /= 3.;

    FT new_average_min_dh = 0.5 *
                            (min_dihedral_angle(tr, vh2, ch0->vertex(indices(vh0_id, 0)),
                                ch0->vertex(indices(vh0_id, 1)),
                                ch0->vertex(indices(vh0_id, 2)))
                           + min_dihedral_angle(tr, vh3, ch1->vertex(indices(vh1_id, 0)),
                                 ch1->vertex(indices(vh1_id, 1)),
                                 ch1->vertex(indices(vh1_id, 2))));
    //Result worst dihedral angle
    if (average_min_dh > new_average_min_dh)
      return NO_BEST_CONFIGURATION;
  }

  //Keep the facets
  typedef CGAL::Triple<Vertex_handle, Vertex_handle, Vertex_handle> Facet_vvv;
  typedef std::unordered_map<Facet_vvv, std::size_t> FaceMapIndex;
  std::unordered_set<Facet, boost::hash<Facet>> outer_mirror_facets;

  FaceMapIndex facet_map_indices;
  std::vector<Facet> mirror_facets;
  circ = Cell_circulator(done);
  do
  {
    // facet opposite to vh0
    int curr_vh0_id = circ->index(vh0);
    Facet n_vh0_facet = tr.mirror_facet(Facet(circ, curr_vh0_id));

    outer_mirror_facets.insert(n_vh0_facet);

    Facet_vvv face0 = make_vertex_triple(circ->vertex(indices(curr_vh0_id, 0)),
                                         circ->vertex(indices(curr_vh0_id, 1)),
                                         circ->vertex(indices(curr_vh0_id, 2)));

    typename FaceMapIndex::iterator it = facet_map_indices.find(face0);
    if (it == facet_map_indices.end())
    {
      facet_map_indices[face0] = mirror_facets.size();
      mirror_facets.push_back(n_vh0_facet);
    }

    // facet opposite to vh1
    int curr_vh1_id = circ->index(vh1);
    Facet n_vh1_facet = tr.mirror_facet(Facet(circ, curr_vh1_id));

    outer_mirror_facets.insert(n_vh1_facet);

    Facet_vvv face1 = make_vertex_triple(circ->vertex(indices(curr_vh1_id, 0)),
                                         circ->vertex(indices(curr_vh1_id, 1)),
                                         circ->vertex(indices(curr_vh1_id, 2)));
    it = facet_map_indices.find(face1);
    if (it == facet_map_indices.end())
    {
      facet_map_indices[face1] = mirror_facets.size();
      mirror_facets.push_back(n_vh1_facet);
    }
  }
  while (++circ != done);

  /*
  c3t3.remove_from_complex( ch0 );
  c3t3.remove_from_complex( ch1 );
  c3t3.remove_from_complex( cell_to_remove );

  tr.flip(edge);

  for( int i = 0 ; i < facets.size() ; i ++ ){
  Cell_handle new_cell = facets[i].first->neighbor( facets[i].second );
  c3t3.add_to_complex( new_cell, si );
  }
  */

  //Update cells
  ch0->set_vertex(vh0_id, vh2);
  ch1->set_vertex(vh1_id, vh3);

  // "New" cells are not created, only modified/updated
  std::vector<Cell_handle> cells_to_update;
  cells_to_update.push_back(ch0);
  cells_to_update.push_back(ch1);

  //Update adjacencies and vertices' cells
  for (Cell_handle ch : cells_to_update)
  {
    for (int v = 0; v < 4; ++v)
    {
      Facet_vvv face = make_vertex_triple(ch->vertex(indices(v, 0)),
                                          ch->vertex(indices(v, 1)),
                                          ch->vertex(indices(v, 2)));
      typename FaceMapIndex::iterator it = facet_map_indices.find(face);
      if (it == facet_map_indices.end())
      {
        facet_map_indices[face] = mirror_facets.size();
        mirror_facets.push_back(Facet(ch, v));
      }
      else
      {
        Facet mirror_facet = mirror_facets[it->second];

        //Update neighbor
        mirror_facet.first->set_neighbor(mirror_facet.second, ch);
        ch->set_neighbor(v, mirror_facet.first);
      }
      ch->vertex(v)->set_cell(ch);

      inc_cells[ch->vertex(v)] = std::nullopt;
      ch->reset_cache_validity();
    }
  }

  // Update c3t3
  update_c3t3_facets(c3t3, cells_to_update, outer_mirror_facets);

  treat_before_delete(cell_to_remove, cell_selector, c3t3);
  tr.tds().delete_cell(cell_to_remove);

  /********************VALIDITY CHECK***************************/
  //if (check_validity)
  //{
  //  post_sliver_Removal_cells.clear();
  //  post_sliver_Removal_cells.push_back(ch0);
  //  post_sliver_Removal_cells.push_back(ch1);

  //  double new_min_dh = min_dihedral_angle<Gt>(ch0);
  //  new_min_dh = std::min(new_min_dh, min_dihedral_angle<Gt>(ch1));

  //  post_sliver_Removal_vertices.clear();
  //  post_sliver_Removal_vertices.push_back(vh2);
  //  post_sliver_Removal_vertices.push_back(vh3);

  //  if (!is_well_oriented(ch0))
  //    return INVALID_ORIENTATION;
  //  if (!is_well_oriented(ch1))
  //    return INVALID_ORIENTATION;
  //  if (!tr.is_valid(ch0))
  //    return INVALID_CELL;
  //  if (!tr.is_valid(ch1))
  //    return INVALID_CELL;

  //  for (int i = 0; i < 4; ++i){
  //    if (!tr.is_valid(ch0->neighbor(i)))
  //      return INVALID_CELL;
  //    if (!tr.is_valid(ch1->neighbor(i)))
  //      return INVALID_CELL;
  //    if (!tr.tds().is_valid(ch0->vertex(i)))
  //      return INVALID_VERTEX;
  //    if (!tr.tds().is_valid(ch1->vertex(i))){
  //      return INVALID_VERTEX;
  //    }
  //  }

  //  if ((curr_min_dh - new_min_dh) > 0.01){
  //    std::cout << "Three_to_two_flip::Flip not improving the quality: " << curr_min_dh << " to " << new_min_dh << std::endl;
  //    return INVALID_CELL;
  //  }
  //}
  /***********************************************************/

  return VALID_FLIP;
}

template<typename C3t3, typename CandidatesQueue>
void find_best_flip_to_improve_dh(C3t3& c3t3,
                                  typename C3t3::Edge& edge,
                                  typename C3t3::Vertex_handle vh2,
                                  typename C3t3::Vertex_handle vh3,
                                  CandidatesQueue& candidates,
                                  const Dihedral_angle_cosine& curr_max_cos_dh,
                                  bool is_sliver_well_oriented = true,
                                  int e_id = 0)
{
  typedef typename C3t3::Triangulation  Tr;
  typedef typename C3t3::Vertex_handle  Vertex_handle;
  typedef typename C3t3::Cell_handle    Cell_handle;
  typedef typename C3t3::Facet          Facet;
  typedef typename Tr::Facet_circulator Facet_circulator;
  typedef typename Tr::Cell_circulator  Cell_circulator;

  // std::cout << "find_best_flip_to_improve_dh boundary " << std::endl;
  Tr& tr = c3t3.triangulation();

  Vertex_handle vh0 = edge.first->vertex(edge.second);
  Vertex_handle vh1 = edge.first->vertex(edge.third);

  Facet_circulator curr_fcirc = tr.incident_facets(edge);
  Facet_circulator curr_fdone = curr_fcirc;

  //Only keep the possible flips
  std::vector<Vertex_handle> opposite_vertices;
  int nb_cells_around_edge = 0;
  do
  {
    Vertex_handle vh;
    //Get the ids of the opposite vertices
    for (int i = 0; i < 3; ++i)
    {
      Vertex_handle curr_vertex = curr_fcirc->first->vertex(indices(curr_fcirc->second, i));
      if ( curr_vertex != vh0
           && curr_vertex != vh1
           && (curr_vertex == vh2 || curr_vertex == vh3))
      {
        vh = curr_vertex;
        Facet_circulator facet_circulator(curr_fcirc);
        Facet_circulator facet_done(curr_fcirc);

        facet_done--;
        facet_circulator++;
        facet_circulator++;

        bool is_edge = false;
        do
        {
          //Get the ids of the opposite vertices
          for (int j = 0; j < 3; ++j)
          {
            Vertex_handle curr = facet_circulator->first->vertex(
                                          indices(facet_circulator->second, j));
            if (curr != vh0  && curr != vh1)
            {
              Cell_handle ch;
              int i0, i1;
              if (tr.is_edge(curr, vh, ch, i0, i1))
                is_edge = true;
            }
          }
        } while (++facet_circulator != facet_done);

        if (!is_edge && !tr.is_infinite(vh))
          opposite_vertices.push_back(vh);
      }
    }
    nb_cells_around_edge++;
  }
  while (++curr_fcirc != curr_fdone);

  if (nb_cells_around_edge < 4)
    return;

  //Facets that will be used to create new cells i.e. all the facets opposite to vh1 and don't have vh
  //Facets that will be used to update cells i.e. all the facets opposite to vh0 will be set to vh: facet.first->set_vertex( facet.second, vh )

  Cell_circulator cell_circulator = tr.incident_cells(edge);
  Cell_circulator done = cell_circulator;

  for (std::size_t i = 0; i < opposite_vertices.size(); ++i)
  {
    Vertex_handle vh = opposite_vertices[i];
    bool keep = true;

    boost::container::small_vector<Facet, 60> facets;
    do
    {
      //Store it if it do not have vh
      if (!cell_circulator->has_vertex(vh))
      {
        //Facets opposite to vh0
        Facet facet_vh0(cell_circulator, cell_circulator->index(vh0));

        //Facets opposite to vh1
        Facet facet_vh1(cell_circulator, cell_circulator->index(vh1));

        facets.push_back(facet_vh1);
        facets.push_back(facet_vh0);
      }
    } while (++cell_circulator != done);


    Dihedral_angle_cosine max_flip_cos_dh(CGAL::NEGATIVE, 1., 1.);
    for (const Facet& fi : facets)
    {
      if (!tr.is_infinite(fi.first) && c3t3.is_in_complex(fi.first))
      {
        if (is_well_oriented(tr, vh, fi.first->vertex(indices(fi.second, 0)),
                             fi.first->vertex(indices(fi.second, 1)),
                             fi.first->vertex(indices(fi.second, 2))))
        {
          max_flip_cos_dh = (std::max)(
            max_flip_cos_dh,
            max_cos_dihedral_angle(tr, vh, fi.first->vertex(indices(fi.second, 0)),
                                           fi.first->vertex(indices(fi.second, 1)),
                                           fi.first->vertex(indices(fi.second, 2))));
        }
        else
        {
          keep = false;
          break;
        }

        if (max_flip_cos_dh.is_one())//it will not get worse than 1.
        {
          keep = false;
          break;
        }
      }
    }
    facets.clear();

    if (keep && (max_flip_cos_dh < curr_max_cos_dh  || !is_sliver_well_oriented))
    {
      //std::cout << "vh " << vh->info() <<" old " << curr_max_cos_dh << " min " << min_flip_tan_dh << std::endl;
      candidates.push(std::make_pair(max_flip_cos_dh, std::make_pair(vh, e_id)));
    }
  }
}

template<typename Vertex_handle, typename CellVector, typename Cell_handle>
bool is_edge_uv(Vertex_handle u,
                Vertex_handle v,
                const CellVector& cells_incident_to_u,
                Cell_handle& cell,
                int& i,
                int& j)
{
  if (u == v)
    return false;

  for (typename CellVector::value_type c : cells_incident_to_u)
  {
    if (c->has_vertex(v, j))
    {
      cell = c;
      i = cell->index(u);
      return true;
    }
  }
  return false;
}

template<typename Vertex_handle, typename CellVector>
bool is_edge_uv(Vertex_handle u,
                Vertex_handle v,
                const CellVector& cells_incident_to_u)
{
  typename CellVector::value_type c;
  int i, j;
  return is_edge_uv(u, v, cells_incident_to_u, c, i, j);
}

template<typename C3t3, typename CandidatesQueue,
         typename IncCellsVectorMap>
void find_best_flip_to_improve_dh(C3t3& c3t3,
                                  typename C3t3::Edge& edge,
                                  CandidatesQueue& candidates,
                                  const Dihedral_angle_cosine& curr_max_cosdh,
                                  IncCellsVectorMap& inc_cells,
                                  bool is_sliver_well_oriented = true,
                                  int e_id = 0)
{
  typedef typename C3t3::Triangulation  Tr;
  typedef typename C3t3::Vertex_handle  Vertex_handle;
  typedef typename C3t3::Cell_handle    Cell_handle;
  typedef typename C3t3::Facet          Facet;
  typedef typename Tr::Facet_circulator Facet_circulator;
  typedef typename Tr::Cell_circulator  Cell_circulator;

  Tr& tr = c3t3.triangulation();

  Vertex_handle vh0 = edge.first->vertex(edge.second);
  Vertex_handle vh1 = edge.first->vertex(edge.third);

  Facet_circulator curr_fcirc = tr.incident_facets(edge);
  Facet_circulator curr_fdone = curr_fcirc;

  //Only keep the possible flips
  std::vector<Vertex_handle> opposite_vertices;
  int nb_cells_around_edge = 0;
  do
  {
    Vertex_handle vh;
    //Get the ids of the opposite vertices
    for (int i = 0; i < 3; ++i)
    {
      Vertex_handle curr_vertex = curr_fcirc->first->vertex(
                                    indices(curr_fcirc->second, i));
      if (curr_vertex != vh0 && curr_vertex != vh1)
      {
        vh = curr_vertex;
        break;
      }
    }

    if(tr.is_infinite(vh))
      continue;

    std::optional<boost::container::small_vector<Cell_handle, 64>>& o_inc_vh = inc_cells[vh];
    if (o_inc_vh == std::nullopt)
    {
      boost::container::small_vector<Cell_handle, 64> inc_vec;
      tr.incident_cells(vh, std::back_inserter(inc_vec));
      o_inc_vh = inc_vec;
    }

    Facet_circulator facet_circulator = curr_fcirc;
    Facet_circulator facet_done = curr_fcirc;

    facet_done--;
    facet_circulator++;
    facet_circulator++;
    bool is_edge = false;
    do
    {
      //Get the ids of the opposite vertices
      for (int i = 0; i < 3; ++i)
      {
        Vertex_handle curr_vertex = facet_circulator->first->vertex(
                                      indices(facet_circulator->second, i));
        if (curr_vertex != vh0  && curr_vertex != vh1)
        {
          if (is_edge_uv(vh, curr_vertex, o_inc_vh.value()))
          {
            is_edge = true;
            break;
          }
        }
      }
    } while (++facet_circulator != facet_done);

    if (!is_edge)
      opposite_vertices.push_back(vh);

    nb_cells_around_edge++;
  }
  while (++curr_fcirc != curr_fdone);
  if (nb_cells_around_edge < 4)
    return;

  //Facets that will be used to create new cells
  //    i.e. all the facets opposite to vh1 and don't have vh
  //Facets that will be used to update cells
  //    i.e. all the facets opposite to vh0 will be set to vh:
  //    facet.first->set_vertex( facet.second, vh )

  Cell_circulator cell_circulator = tr.incident_cells(edge);
  Cell_circulator done = cell_circulator;

  boost::container::small_vector<Facet, 60> facets;
  for (Vertex_handle vh : opposite_vertices)
  {
    bool keep = true;
    do
    {
      //Store it if it do not have vh
      if (!cell_circulator->has_vertex(vh))
      {
        //Facets opposite to vh0
        Facet facet_vh0(cell_circulator, cell_circulator->index(vh0));

        //Facets opposite to vh1
        Facet facet_vh1(cell_circulator, cell_circulator->index(vh1));

        facets.push_back(facet_vh1);
        facets.push_back(facet_vh0);
      }
    }
    while (++cell_circulator != done);

    Dihedral_angle_cosine max_flip_cos_dh(CGAL::NEGATIVE, 1., 1.);
    for (const Facet& fi : facets)
    {
      if (!tr.is_infinite(fi.first))
      {
        if (is_well_oriented(tr, vh, fi.first->vertex(indices(fi.second, 0)),
                             fi.first->vertex(indices(fi.second, 1)),
                             fi.first->vertex(indices(fi.second, 2))))
        {
          max_flip_cos_dh = (std::max)(max_flip_cos_dh,
            max_cos_dihedral_angle(tr, vh, fi.first->vertex(indices(fi.second, 0)),
                                           fi.first->vertex(indices(fi.second, 1)),
                                           fi.first->vertex(indices(fi.second, 2))));
        }
        else
        {
          keep = false;
          break;
        }

        if (max_flip_cos_dh.is_one())//it will not get worse than 1.
        {
          keep = false;
          break;
        }
      }
    }
    facets.clear();

    if (keep && (max_flip_cos_dh < curr_max_cosdh || !is_sliver_well_oriented))
    {
      //std::cout << "vh " << vh->info() <<" old " << curr_max_cosdh << " min " << min_flip_tan_dh << std::endl;
      candidates.push(std::make_pair(max_flip_cos_dh, std::make_pair(vh, e_id)));
    }
  }
}

template<typename C3t3,
         typename IncCellsVectorMap,
         typename Cell_selector,
         typename Visitor>
Sliver_removal_result flip_n_to_m(C3t3& c3t3,
                                  typename C3t3::Edge& edge,
                                  typename C3t3::Vertex_handle vh,
                                  IncCellsVectorMap& inc_cells,
                                  Cell_selector& cell_selector,
                                  Visitor& visitor,
                                  bool check_validity = false)
{
  CGAL_USE(check_validity);
  // std::cout << "n_to_m_flip::start" << std::endl;
  typedef typename C3t3::Triangulation  Tr;
  typedef typename C3t3::Vertex_handle  Vertex_handle;
  typedef typename C3t3::Cell_handle    Cell_handle;
  typedef typename C3t3::Facet          Facet;
  typedef typename Tr::Facet_circulator Facet_circulator;
  typedef typename Tr::Cell_circulator  Cell_circulator;

  Tr& tr = c3t3.triangulation();

  Vertex_handle vh0 = edge.first->vertex(edge.second);
  Vertex_handle vh1 = edge.first->vertex(edge.third);

  //This vertex will have its valence augmenting a lot,
  //TODO take the best one

  //TODO!!!! Check that the created edges do not exist!!!

  boost::container::small_vector<Facet, 2> facets_in_complex;
  typename C3t3::Surface_patch_index patch;

  Facet_circulator facet_circulator = tr.incident_facets(edge);
  Facet_circulator done_facet_circulator = facet_circulator;
  bool look_for_vh_iterator = true;
  do
  {
    if (c3t3.is_in_complex(*facet_circulator))
    {
      patch = c3t3.surface_patch_index(*facet_circulator);
      facets_in_complex.push_back(*facet_circulator);
    }

    facet_circulator++;

    //Get the ids of the opposite vertices
    for (int i = 0; i < 3; ++i)
    {
      if (facet_circulator->first->vertex(indices(facet_circulator->second, i)) == vh)
        look_for_vh_iterator = false;
    }

  } while (facet_circulator != done_facet_circulator && look_for_vh_iterator);

  if (look_for_vh_iterator) {
    std::cout << "Vertex not an opposite of the edge!!" << std::endl;
    return NOT_FLIPPABLE;
  }

  Facet_circulator facet_done(facet_circulator);
  facet_done--;
  facet_circulator++;
  facet_circulator++;

  std::optional<boost::container::small_vector<Cell_handle, 64>>& o_inc_vh = inc_cells[vh];
  if (o_inc_vh == std::nullopt)
  {
    boost::container::small_vector<Cell_handle, 64> inc_vec;
    tr.incident_cells(vh, std::back_inserter(inc_vec));
    o_inc_vh = inc_vec;
  }

  do
  {
    //Get the ids of the opposite vertices
    for (int i = 0; i < 3; ++i)
    {
      Vertex_handle curr_vertex = facet_circulator->first->vertex(
                                    indices(facet_circulator->second, i));
      if (curr_vertex != vh0  && curr_vertex != vh1)
      {
        if (is_edge_uv(vh, curr_vertex, o_inc_vh.value()))
          return NOT_FLIPPABLE;
      }
    }
  } while (++facet_circulator != facet_done);


  boost::container::small_vector<Cell_handle, 20> to_remove;

  //Neighbors that will need to be updated after flip
  std::unordered_set<Facet, boost::hash<Facet>> neighbor_facets;

  //Facets that will be used to create new cells
  // i.e. all the facets opposite to vh1 and don't have vh
  std::vector<Facet> facets_for_new_cells;

  //Facets that will be used to update cells
  // i.e. all the facets opposite to vh0 will be set to vh :
  // facet.first->set_vertex( facet.second, vh )
  std::vector<Facet> facets_for_updated_cells;

  Cell_circulator cell_circulator = tr.incident_cells(edge);
  Cell_circulator done = cell_circulator;
  do
  {
    //Facets opposite to vh0
    Facet facet_vh0(cell_circulator, cell_circulator->index(vh0));
    neighbor_facets.insert(tr.mirror_facet(facet_vh0));

    //Facets opposite to vh1
    Facet facet_vh1(cell_circulator, cell_circulator->index(vh1));
    neighbor_facets.insert(tr.mirror_facet(facet_vh1));

    //Store it if it do not have vh
    if (cell_circulator->has_vertex(vh)) {
      to_remove.push_back(cell_circulator);
    }
    else
    {
      facets_for_new_cells.push_back(facet_vh1);
      facets_for_updated_cells.push_back(facet_vh0);
    }
    //
    //        if( ! is_well_oriented( cell_circulator ) )
    //            return WRONG;
  }
  while (++cell_circulator != done);

  //Check that the result will be valid
  for (const Facet& fi : facets_for_new_cells)
  {
    if ( !tr.is_infinite(fi.first)
         && !is_well_oriented(tr, vh, fi.first->vertex(indices(fi.second, 0)),
                              fi.first->vertex(indices(fi.second, 1)),
                              fi.first->vertex(indices(fi.second, 2))))
      return NOT_FLIPPABLE;
  }
  for (const Facet& fi : facets_for_updated_cells)
  {
    if ( !tr.is_infinite(fi.first)
         && !is_well_oriented(tr, vh, fi.first->vertex(indices(fi.second, 0)),
                              fi.first->vertex(indices(fi.second, 1)),
                              fi.first->vertex(indices(fi.second, 2))))
      return NOT_FLIPPABLE;
  }

  ///********************VALIDITY CHECK***************************/
  //double current_min_dh = DBL_MAX;

  //if (check_validity){

  //  pre_sliver_Removal_cells.clear();
  //  do{
  //    pre_sliver_Removal_cells.push_back(K::Tetrahedron_3(cell_circulator->vertex(0)->point(), cell_circulator->vertex(1)->point(),
  //      cell_circulator->vertex(2)->point(), cell_circulator->vertex(3)->point()));

  //    if (!tr.is_infinite(cell_circulator))
  //      current_min_dh = std::min(current_min_dh, min_dihedral_angle(cell_circulator));
  //  } while (++cell_circulator != done);

  //  pre_sliver_Removal_vertices.clear();
  //  pre_sliver_Removal_vertices.push_back(vh->point());

  //  previous_edges.clear();
  //  previous_edges.push_back(std::make_pair(vh0->point(), vh1->point()));
  //}
  ///*************************************************************/

  //Surface
  for (const Facet& f : facets_in_complex)
    c3t3.remove_from_complex(f);

  //Subdomain index?
  typedef typename C3t3::Subdomain_index Subdomain_index;
  bool selected = get(cell_selector, to_remove[0]);
  visitor.before_flip(to_remove[0]);

  std::vector<Cell_handle> cells_to_update;

  //Create new cells
  for (const Facet& fi : facets_for_new_cells)
  {
    Cell_handle new_cell = tr.tds().create_cell();

    for (int v = 0; v < 4; v++){
      new_cell->set_vertex(v, fi.first->vertex(v));
    }

    new_cell->set_vertex(fi.second, vh);

    treat_new_cell(new_cell, subdomain, cell_selector, selected, c3t3);

    visitor.after_flip(new_cell);
    cells_to_update.push_back(new_cell);
  }

  //Update_existing cells
  for (const Facet& fi : facets_for_updated_cells)
  {
    fi.first->set_vertex(fi.second, vh);
    cells_to_update.push_back(fi.first);
  }

  typedef CGAL::Triple<Vertex_handle, Vertex_handle, Vertex_handle> Facet_vvv;
  typedef std::unordered_map<Facet_vvv, std::size_t> FaceMapIndex;

  FaceMapIndex facet_map_indices;
  std::vector<Facet> facets;

  for (const Facet& f : neighbor_facets)
  {
    Cell_handle ch = f.first;
    int v = f.second;

    Facet_vvv face = make_vertex_triple(ch->vertex(indices(v,0)),
                                        ch->vertex(indices(v,1)),
                                        ch->vertex(indices(v,2)));
    typename FaceMapIndex::iterator it = facet_map_indices.find(face);
    if (it == facet_map_indices.end())
    {
      facet_map_indices[face] = facets.size();
      facets.push_back(Facet(ch, v));
    }
  }

  //Update adjacencies and vertices cells
  for (Cell_handle ch : cells_to_update)
  {
    for (int v = 0; v < 4; v++)
    {
      Facet_vvv face = make_vertex_triple(ch->vertex(indices(v,0)),
                                          ch->vertex(indices(v,1)),
                                          ch->vertex(indices(v,2)));
      typename FaceMapIndex::iterator it = facet_map_indices.find(face);
      if (it == facet_map_indices.end())
      {
        facet_map_indices[face] = facets.size();
        facets.push_back(Facet(ch, v));
      }
      else
      {
        Facet facet = facets[it->second];

        //Update neighbor
        facet.first->set_neighbor(facet.second, ch);
        ch->set_neighbor(v, facet.first);
      }
      ch->vertex(v)->set_cell(ch);

      inc_cells[ch->vertex(v)] = std::nullopt;
      ch->reset_cache_validity();
    }
  }

  // Update c3t3
  update_c3t3_facets(c3t3, cells_to_update, neighbor_facets);

  //Remove cells
  for (Cell_handle ch : to_remove)
  {
    treat_before_delete(ch, cell_selector, c3t3);
    tr.tds().delete_cell(ch);
  }

  ///********************VALIDITY CHECK***************************/
  //if (check_validity){

  //  double new_min_dh = DBL_MAX;

  //  post_sliver_Removal_cells.clear();
  //  for (unsigned int i = 0; i < cells_to_update.size(); ++i){
  //    post_sliver_Removal_cells.push_back(cells_to_update[i]);

  //    if (!tr.is_infinite(cells_to_update[i]))
  //      new_min_dh = std::min(new_min_dh, min_dihedral_angle(cells_to_update[i]));
  //  }

  //  post_sliver_Removal_vertices.clear();
  //  for (unsigned int i = 0; i < vertices_around_edge.size(); ++i){
  //    post_sliver_Removal_vertices.push_back(vertices_around_edge[i]);
  //  }

  //  current_edges.clear();
  //  for (unsigned int i = 0; i < vertices_around_edge.size(); ++i){
  //    current_edges.push_back(std::make_pair(vertices_around_edge[i]->point(), vh->point()));
  //  }


  //  for (unsigned int i = 0; i < cells_to_update.size(); ++i){
  //    if (!tr.is_valid(cells_to_update[i]))
  //      return INVALID_CELL;

  //    for (int v = 0; v < 4; v++){
  //      if (!tr.is_valid(cells_to_update[i]->neighbor(v)))
  //        return INVALID_CELL;

  //      if (!tr.tds().is_valid(cells_to_update[i]->vertex(v)))
  //        return INVALID_VERTEX;

  //    }
  //  }

  //  if ((current_min_dh - new_min_dh) > 0.01){
  //    std::cout << pre_sliver_Removal_cells.size() << " to " << post_sliver_Removal_cells.size() << " flip not improving the quality: " <<
  //      current_min_dh << " to " << new_min_dh << std::endl;
  //    return INVALID_CELL;
  //  }

  //}
  ///***********************************************************/

  // std::cout << "n_to_m_flip::end with success" << std::endl;

  return VALID_FLIP;
}


template<typename C3t3, typename IncCellsVectorMap, typename CellSelector, typename Visitor>
Sliver_removal_result flip_n_to_m(typename C3t3::Edge& edge,
                                  C3t3& c3t3,
                                  const std::vector<typename C3t3::Vertex_handle>& boundary_vertices,
                                  const Flip_Criterion& criterion,
                                  IncCellsVectorMap& inc_cells,
                                  CellSelector& cell_selector,
                                  Visitor& visitor)
{
  typedef typename C3t3::Vertex_handle Vertex_handle;
  typedef typename C3t3::Triangulation::Cell_circulator Cell_circulator;
  typename C3t3::Triangulation& tr = c3t3.triangulation();

  Sliver_removal_result result = NOT_FLIPPABLE;

  typedef std::pair<Dihedral_angle_cosine, std::pair<Vertex_handle, int> > CosAngle_and_vertex;

  //std::cout << "n_to_m_flip " << boundary_vertices.size() << std::endl;
  if (criterion == MIN_ANGLE_BASED)
  {
    std::priority_queue<CosAngle_and_vertex,
                        std::vector<CosAngle_and_vertex>,
                        std::greater<CosAngle_and_vertex>
                      > candidates;

    Cell_circulator circ = c3t3.triangulation().incident_cells(edge);
    Cell_circulator done = circ;

    Dihedral_angle_cosine curr_max_cosdh = max_cos_dihedral_angle(tr, circ++);
    do
    {
      curr_max_cosdh = (std::max)(curr_max_cosdh, max_cos_dihedral_angle(tr, circ));
    } while (++circ != done);

    if (boundary_vertices.size() == 2)
      find_best_flip_to_improve_dh(c3t3, edge, boundary_vertices[0], boundary_vertices[1],
                                   candidates, curr_max_cosdh);
    else
      find_best_flip_to_improve_dh(c3t3, edge, candidates, curr_max_cosdh,
                                   inc_cells);

    bool flip_performed = false;
    while (!candidates.empty() && !flip_performed)
    {
      CosAngle_and_vertex curr_cost_vpair = candidates.top();
      candidates.pop();

//      std::cout << "\tcurrent   cos = " << curr_max_cosdh.value()
//        << "\t angle = " << std::acos(curr_max_cosdh.value()) * 180./CGAL_PI << std::endl;
//      std::cout << "\tcandidate cos = " << curr_cost_vpair.first.value()
//        << "\t angle = " << std::acos(curr_cost_vpair.first.value()) * 180./CGAL_PI << std::endl;
//      std::cout << std::endl;

      if (curr_max_cosdh <= curr_cost_vpair.first)
        return NO_BEST_CONFIGURATION;

      result = flip_n_to_m(c3t3, edge, curr_cost_vpair.second.first, inc_cells,
                           cell_selector, visitor);

      if (result != NOT_FLIPPABLE)
        flip_performed = true;
    }
  }

  return result;
}

template<typename C3t3, typename IncCellsVectorMap, typename Cell_selector, typename Visitor>
Sliver_removal_result find_best_flip(typename C3t3::Edge& edge,
                                     C3t3& c3t3,
                                     const Flip_Criterion& criterion,
                                     IncCellsVectorMap& inc_cells,
                                     Cell_selector& cell_selector,
                                     Visitor& visitor)
{
  typedef typename C3t3::Triangulation        Tr;
  typedef typename C3t3::Vertex_handle        Vertex_handle;
  typedef typename Tr::Facet_circulator       Facet_circulator;

  Tr& tr = c3t3.triangulation();

  const Vertex_handle v0 = edge.first->vertex(edge.second);
  const Vertex_handle v1 = edge.first->vertex(edge.third);

  Facet_circulator circ = tr.incident_facets(edge);
  Facet_circulator done = circ;

  //Identify the vertices around this edge
  std::unordered_set<Vertex_handle> vertices_around_edge;
  bool boundary_edge = false;
  bool hull_edge = false;

  std::unordered_set<Vertex_handle> boundary_vertices;
//  std::unordered_set<Vertex_handle> hull_vertices;
  do
  {
    //Get the ids of the opposite vertices
    for (int i = 0; i < 3; ++i)
    {
      Vertex_handle vi = circ->first->vertex(indices(circ->second, i));
      if (vi != v0 && vi != v1)
      {
        vertices_around_edge.insert(vi);

        if ( circ->first->subdomain_index()
             != circ->first->neighbor(circ->second)->subdomain_index())
        {
          boundary_edge = true;
          boundary_vertices.insert(vi);
        }

        if ( tr.is_infinite(circ->first)
             != tr.is_infinite(circ->first->neighbor(circ->second)))
        {
          hull_edge = true;
          //hull_vertices.insert(vi);
        }
      }
    }
  }
  while (++circ != done);


  //Check if not feature edge
  if (boundary_vertices.size() > 2)
    return NOT_FLIPPABLE;

  // perform flip when possible
  Sliver_removal_result res = NOT_FLIPPABLE;
  if (vertices_around_edge.size() == 3)
  {
    if (!boundary_edge && !hull_edge)
    {
      std::vector<Vertex_handle> vertices;
      vertices.insert(vertices.end(), vertices_around_edge.begin(), vertices_around_edge.end());
      res = flip_3_to_2(edge, c3t3, vertices, criterion, inc_cells, cell_selector);
    }
  }
  else
  {
    //TODO fix for hull edges
    // if( hull_edge )
    //    return n_to_m_flip( edge, hull_vertices, flip_criterion, check_validity );
    if (!hull_edge)
    {
      std::vector<Vertex_handle> vertices;
      vertices.insert(vertices.end(), boundary_vertices.begin(), boundary_vertices.end());
      res = flip_n_to_m(edge, c3t3, vertices, criterion, inc_cells, cell_selector, visitor);
      //return n_to_m_flip(edge, boundary_vertices, flip_criterion);
    }
  }

  return res;
}


template<typename VertexPair, typename C3t3, typename Cell_selector, typename Visitor>
std::size_t flip_all_edges(const std::vector<VertexPair>& edges,
                           C3t3& c3t3,
                           const Flip_Criterion& criterion,
                           Cell_selector& cell_selector,
                           Visitor& visitor)
{
  typedef typename C3t3::Triangulation Tr;
  typedef typename Tr::Cell_handle   Cell_handle;
  typedef typename Tr::Vertex_handle Vertex_handle;
  typedef typename Tr::Edge          Edge;

  Tr& tr = c3t3.triangulation();

  std::unordered_map<Vertex_handle,
    std::optional<boost::container::small_vector<Cell_handle, 64> > > inc_cells;

  std::size_t count = 0;
  for (const VertexPair& vp : edges)
  {
    std::optional<boost::container::small_vector<Cell_handle, 64>>&
      o_inc_vh = inc_cells[vp.first];
    if (o_inc_vh == std::nullopt)
    {
      boost::container::small_vector<Cell_handle, 64> inc_vec;
      tr.incident_cells(vp.first, std::back_inserter(inc_vec));
      o_inc_vh = inc_vec;
    }

    Cell_handle ch;
    int i0, i1;
    if (is_edge_uv(vp.first, vp.second, o_inc_vh.value(), ch, i0, i1))
    {
      Edge edge(ch, i0, i1);

      Sliver_removal_result res
        = find_best_flip(edge, c3t3, criterion, inc_cells, cell_selector, visitor);
      if (res == INVALID_CELL || res == INVALID_VERTEX || res == INVALID_ORIENTATION)
      {
        std::cout << "FLIP PROBLEM!!!!" << std::endl;
        return count;
      }
      if (res == VALID_FLIP)
      {
        ++count;
#ifdef CGAL_TETRAHEDRAL_REMESHING_VERBOSE_PROGRESS
        std::cout << "\rFlip... (";
        std::cout << count << " flips)";
        std::cout.flush();
#endif
      }
    }
  }

  for (typename Tr::Finite_cells_iterator c = tr.finite_cells_begin();
       c != tr.finite_cells_end();
       ++c)
    c->reset_cache_validity();

  return count;
}

template<typename C3T3, typename CellSelector>
void collectBoundaryEdgesAndComputeVerticesValences(
  const C3T3& c3t3,
  CellSelector cell_selector,
  std::vector<typename C3T3::Edge>& boundary_edges,
  boost::unordered_map<typename C3T3::Vertex_handle,
                       boost::unordered_map<typename C3T3::Surface_patch_index, unsigned int> >&
      boundary_vertices_valences,
  boost::unordered_map<typename C3T3::Vertex_handle, std::vector<typename C3T3::Subdomain_index> >&
      vertices_subdomain_indices)
{
  typedef typename C3T3::Subdomain_index     Subdomain_index;
  typedef typename C3T3::Surface_patch_index Surface_patch_index;
  typedef typename C3T3::Vertex_handle       Vertex_handle;
  typedef typename C3T3::Edge                Edge;
  typedef typename C3T3::Triangulation::Facet_circulator Facet_circulator;

  const typename C3T3::Triangulation& tr = c3t3.triangulation();

  boundary_edges.clear();
  boundary_vertices_valences.clear();

  for (typename C3T3::Triangulation::Finite_edges_iterator
       eit = tr.finite_edges_begin();
       eit != tr.finite_edges_end();
       ++eit)
  {
    const Edge e = *eit;
    if (is_boundary(c3t3, e, cell_selector))
      boundary_edges.push_back(e);
  }

  for (std::size_t i = 0; i < boundary_edges.size(); ++i)
  {
    const Edge& e = boundary_edges[i];
    const Vertex_handle v0 = e.first->vertex(e.second);
    const Vertex_handle v1 = e.first->vertex(e.third);

    if (vertices_subdomain_indices.find(v0) == vertices_subdomain_indices.end())
      incident_subdomains(v0, c3t3, std::back_inserter(vertices_subdomain_indices[v0]));
    if (vertices_subdomain_indices.find(v1) == vertices_subdomain_indices.end())
      incident_subdomains(v1, c3t3, std::back_inserter(vertices_subdomain_indices[v1]));

    //In case of feature edge
    if (vertices_subdomain_indices[v0].size() > 2
      && vertices_subdomain_indices[v1].size() > 2)
    {
      Facet_circulator facet_circulator = tr.incident_facets(e);
      Facet_circulator done(facet_circulator);
      do
      {
        if (c3t3.is_in_complex(*facet_circulator))
        {
          Surface_patch_index surfi = c3t3.surface_patch_index(*facet_circulator);
          boundary_vertices_valences[v0][surfi]++;
          boundary_vertices_valences[v1][surfi]++;
        }
      } while (++facet_circulator != done);
    }
    else if (vertices_subdomain_indices[v0].size() == 2)
    {
      Surface_patch_index surfi_0 = surface_patch_index(v0, c3t3);

      boundary_vertices_valences[v0][surfi_0]++;
      boundary_vertices_valences[v1][surfi_0]++;

    }
    else if (vertices_subdomain_indices[v1].size() == 2)
    {
      Surface_patch_index surfi_1 = surface_patch_index(v1, c3t3);

      boundary_vertices_valences[v0][surfi_1]++;
      boundary_vertices_valences[v1][surfi_1]++;
    }
  }
}

template<typename C3T3, typename IncCellsVector>
Sliver_removal_result flip_n_to_m_on_surface(typename C3T3::Edge& edge,
    C3T3& c3t3,
    typename C3T3::Vertex_handle v0i,//v0 of new edge that will replace edge
    typename C3T3::Vertex_handle v1i,//v1 of new edge that will replace edge
    const IncCellsVector& cells_around_edge,
    Flip_Criterion flip_criterion)
{
  typedef typename C3T3::Vertex_handle Vertex_handle;
  typedef typename C3T3::Cell_handle   Cell_handle;
  typedef typename C3T3::Triangulation::Geom_traits::Point_3 Point_3;

  typename C3T3::Triangulation& tr = c3t3.triangulation();

  Vertex_handle u = edge.first->vertex(edge.second);
  Vertex_handle v = edge.first->vertex(edge.third);

  typedef std::pair<int, int> IndInCell;
  std::map<Cell_handle, IndInCell> indices;
  for (Cell_handle c : cells_around_edge)
  {
    indices[c] = std::make_pair(c->index(u), c->index(v));
  }

  for (Cell_handle c : cells_around_edge)
  {
    int i = indices[c].first;
    int j = indices[c].second;
    c->set_vertex(i, v0i);
    c->set_vertex(j, v1i);

    if (!is_well_oriented(tr, c))
    {
      c->set_vertex(j, v0i);
      c->set_vertex(i, v1i);
      if (!is_well_oriented(tr, c))
      {
        //rollback all changes
        for (Cell_handle cc : cells_around_edge)
        {
          int ii = indices[cc].first;
          if (cc->vertex(ii) != u)
          {
            cc->set_vertex(ii, u);
            cc->set_vertex(indices[cc].second, v);
          }
        }
        return NOT_FLIPPABLE;
      }
    }
  }

  return VALID_FLIP;
}

//v0i and v1i are the vertices opposite to `edge`
//on facets of the surface
template<typename C3T3, typename IncCellsVectorMap, typename Visitor>
Sliver_removal_result flip_on_surface(C3T3& c3t3,
                                      typename C3T3::Edge& edge,
                                      typename C3T3::Vertex_handle v0i,
                                      typename C3T3::Vertex_handle v1i,
                                      IncCellsVectorMap& inc_cells,
                                      Flip_Criterion flip_criterion,
                                      Visitor& visitor)
{
  typedef typename C3T3::Triangulation Tr;
  typedef typename Tr::Cell_handle     Cell_handle;
  typedef typename Tr::Vertex_handle   Vertex_handle;
  typedef typename Tr::Cell_circulator Cell_circulator;
  typedef typename Tr::Point           Point_3;

  Tr& tr = c3t3.triangulation();
  Cell_circulator circ = tr.incident_cells(edge);
  Cell_circulator done(circ);

  std::vector<Cell_handle> cells_around_edge;
  do
  {
    cells_around_edge.push_back(circ);
  } while (++circ != done);

  std::cout << cells_around_edge.size();

  if (cells_around_edge.size() != 4)
  {
    nb_surface_nm_configs++;
    if (cells_around_edge.size() > 4){
//////      if (flip_criterion == VALENCE_BASED){
//////        return find_best_n_m_flip(edge, vh0_index, vh1_index);
//////      }
//////      else {
        std::vector<Vertex_handle> boundary_vertices;
        boundary_vertices.push_back(v0i);
        boundary_vertices.push_back(v1i);

//        return flip_n_to_m_on_surface(edge, c3t3, v0i, v1i, cells_around_edge, flip_criterion);

        return flip_n_to_m(edge, c3t3, boundary_vertices,
                           flip_criterion,
                           inc_cells,
                           visitor);
//      }
    }
    else
      return NOT_FLIPPABLE;
  }

  nb_surface_44_configs++;

  if(inc_cells[edge.first->vertex(edge.second)] != boost::none)
    inc_cells[edge.first->vertex(edge.second)].get().clear();
  if (inc_cells[edge.first->vertex(edge.third)] != boost::none)
    inc_cells[edge.first->vertex(edge.third)].get().clear();

  Cell_handle ch0, ch1, ch2, ch3;
  ch0 = cells_around_edge[0];
  ch1 = cells_around_edge[1];
  ch2 = cells_around_edge[2];
  ch3 = cells_around_edge[3];

  Dihedral_angle_cosine curr_max_cosdh = max_cos_dihedral_angle(tr, ch0);
  for (int i = 1; i < 4; ++i)
    curr_max_cosdh = (std::max)(curr_max_cosdh,
                                max_cos_dihedral_angle(tr, cells_around_edge[i]));

  Vertex_handle vh0, vh1, vh2, vh3, vh4, vh5;

  int ivh4 = ch0->index(ch1);
  vh4 = ch0->vertex(ivh4);

  int ivh2 = ch0->index(ch3);
  vh2 = ch0->vertex(ivh2);

  vh5 = ch1->vertex(ch1->index(ch0));
  vh0 = ch2->vertex(ch2->index(ch1));

  for (int j = 0; j < 3; j++){
    if (indices(ivh4, j) == ivh2){
      int j1 = (j + 1) % 3;
      int j2 = (j + 2) % 3;
      vh1 = ch0->vertex(indices(ivh4, j1));
      vh3 = ch0->vertex(indices(ivh4, j2));
      break;
    }
  }

  bool planar_flip;

//  std::vector<std::pair<Point_3, Point_3> > current_edges;
//  current_edges.push_back(std::make_pair(vh1->point(), vh3->point()));

  if (vh0 == v0i && vh2 == v1i || vh2 == v0i && vh0 == v1i)
  {
    planar_flip = true;
//    current_edges.push_back(std::make_pair(vh0->point(), vh2->point()));
  }
  else if (vh4 == v0i && vh5 == v1i || vh5 == v0i && vh4 == v1i)
    planar_flip = false;
  else
    return NOT_FLIPPABLE;

  //  /*
//  post_sliver_Removal_cells.clear();
//  post_sliver_Removal_cells.push_back( ch0 );
//  post_sliver_Removal_cells.push_back( ch1 );
//  post_sliver_Removal_cells.push_back( ch2 );
//  post_sliver_Removal_cells.push_back( ch3 );
//
//  post_sliver_Removal_vertices.clear();
//  post_sliver_Removal_vertices.push_back( vh0 );
//  post_sliver_Removal_vertices.push_back( vh1 );
//  post_sliver_Removal_vertices.push_back( vh2 );
//  post_sliver_Removal_vertices.push_back( vh3 );
//  post_sliver_Removal_vertices.push_back( vh4 );
//  post_sliver_Removal_vertices.push_back( vh5 );
//
//  pre_sliver_Removal_cells.clear();
//  pre_sliver_Removal_cells.push_back(K::Tetrahedron_3 ( ch0->vertex(0)->point(), ch0->vertex(1)->point(), ch0->vertex(2)->point(), ch0->vertex(3)->point()  ));
//  pre_sliver_Removal_cells.push_back(K::Tetrahedron_3 ( ch1->vertex(0)->point(), ch1->vertex(1)->point(), ch1->vertex(2)->point(), ch1->vertex(3)->point()  ));
//  pre_sliver_Removal_cells.push_back(K::Tetrahedron_3 ( ch2->vertex(0)->point(), ch2->vertex(1)->point(), ch2->vertex(2)->point(), ch2->vertex(3)->point()  ));
//  pre_sliver_Removal_cells.push_back(K::Tetrahedron_3 ( ch3->vertex(0)->point(), ch3->vertex(1)->point(), ch3->vertex(2)->point(), ch3->vertex(3)->point()  ));
//
//  if( ! c3t3_with_info.triangulation().is_valid( ch0 ) ||
//  ! c3t3_with_info.triangulation().is_valid( ch1 ) || ! c3t3_with_info.triangulation().is_valid( ch2 ) || !c3t3_with_info.triangulation().is_valid( ch3 ) ){
//  std::cout << "Initial cell PB!!" << std::endl;
//  return CELL_PBS;
//  }
//
//  std::cout << "vh0 "<< vh0->info() <<", " << vh1->info() << ", " << vh2->info() << ", " << vh3->info()<<", " << vh4->info() << ", " << vh5 ->info() << std::endl;
//  */

  typedef std::pair<Vertex_handle, Vertex_handle> Edge_uv;
  typedef typename C3t3::Edge  Edge;
  typedef typename C3t3::Facet Facet;
  typedef typename C3t3::Surface_patch_index Surface_patch_index;

  if (planar_flip)
  {
    Surface_patch_index patch = c3t3.surface_patch_index(ch0, ch0->index(vh4));
    CGAL_assertion(patch != Surface_patch_index());
    CGAL_assertion(c3t3.is_in_complex(ch0, ch0->index(vh4)));
    c3t3.remove_from_complex(ch0, ch0->index(vh4));
    CGAL_assertion(c3t3.is_in_complex(ch3, ch3->index(vh4)));
    c3t3.remove_from_complex(ch3, ch3->index(vh4));

    boost::unordered_map<Facet, Surface_patch_index> opposite_facet_in_complex;
    for (int i = 0; i < 4; ++i)
    {
      Cell_handle chi = cells_around_edge[i];
      Facet f1(chi, chi->index(vh1));
      Facet f2(chi, chi->index(vh3));

      if (c3t3.is_in_complex(f1))
      {
        Surface_patch_index spi = c3t3.surface_patch_index(f1);
        opposite_facet_in_complex[c3t3.triangulation().mirror_facet(f1)] = spi;
        c3t3.remove_from_complex(f1);
      }
      if (c3t3.is_in_complex(f2))
      {
        Surface_patch_index spi = c3t3.surface_patch_index(f2);
        opposite_facet_in_complex[c3t3.triangulation().mirror_facet(f2)] = spi;
        c3t3.remove_from_complex(f2);
      }
    }

    Cell_handle n_ch3_vh1 = ch3->neighbor(ch3->index(vh1));
    Cell_handle n_ch0_vh3 = ch0->neighbor(ch0->index(vh3));

    Cell_handle n_ch2_vh1 = ch2->neighbor(ch2->index(vh1));
    Cell_handle n_ch1_vh3 = ch1->neighbor(ch1->index(vh3));

    boost::unordered_map<Edge_uv, typename C3t3::Curve_index> complex_edges;
    for (Cell_handle c : cells_around_edge)
    {
      for (int ii = 0; ii < 4; ++ii)
      {
        for (int jj = 0; jj < 4; ++jj)
        {
          Edge e(c, ii, jj);
          if (c3t3.is_in_complex(e))
          {
            complex_edges[make_vertex_pair(c->vertex(ii), c->vertex(jj))] = c3t3.curve_index(e);
            c3t3.remove_from_complex(e);
          }
        }
      }
    }

//    pre_sliver_Removal_cells.push_back(K::Tetrahedron_3(n_ch3_vh1->vertex(0)->point(), n_ch3_vh1->vertex(1)->point(), n_ch3_vh1->vertex(2)->point(), n_ch3_vh1->vertex(3)->point()));
//    pre_sliver_Removal_cells.push_back(K::Tetrahedron_3(n_ch0_vh3->vertex(0)->point(), n_ch0_vh3->vertex(1)->point(), n_ch0_vh3->vertex(2)->point(), n_ch0_vh3->vertex(3)->point()));
//    pre_sliver_Removal_cells.push_back(K::Tetrahedron_3(n_ch2_vh1->vertex(0)->point(), n_ch2_vh1->vertex(1)->point(), n_ch2_vh1->vertex(2)->point(), n_ch2_vh1->vertex(3)->point()));
//    pre_sliver_Removal_cells.push_back(K::Tetrahedron_3(n_ch1_vh3->vertex(0)->point(), n_ch1_vh3->vertex(1)->point(), n_ch1_vh3->vertex(2)->point(), n_ch1_vh3->vertex(3)->point()));

    ch3->set_vertex(ch3->index(vh3), vh2);
    ch0->set_vertex(ch0->index(vh1), vh0);
    ch2->set_vertex(ch2->index(vh3), vh2);
    ch1->set_vertex(ch1->index(vh1), vh0);

    Sliver_removal_result db = VALID_FLIP;
    if (!is_well_oriented(tr, ch0)
      || !is_well_oriented(tr, ch1)
      || !is_well_oriented(tr, ch2)
      || !is_well_oriented(tr, ch3))
      db = NOT_FLIPPABLE;
    else if (curr_max_cosdh < max_cos_dihedral_angle(tr, ch0)
      || curr_max_cosdh < max_cos_dihedral_angle(tr, ch1)
      || curr_max_cosdh < max_cos_dihedral_angle(tr, ch2)
      || curr_max_cosdh < max_cos_dihedral_angle(tr, ch3))
      db = NO_BEST_CONFIGURATION;

    if(db == NOT_FLIPPABLE || db == NO_BEST_CONFIGURATION)
    {
      ch3->set_vertex(ch3->index(vh2), vh3);
      ch0->set_vertex(ch0->index(vh0), vh1);
      ch2->set_vertex(ch2->index(vh2), vh3);
      ch1->set_vertex(ch1->index(vh0), vh1);

      c3t3.add_to_complex(ch0, ch0->index(vh4), patch);
      c3t3.add_to_complex(ch3, ch3->index(vh4), patch);

      for (int i = 0; i < 4; ++i)
      {
        Cell_handle chi = cells_around_edge[i];
        Facet f1(chi, chi->index(vh1));
        Facet f2(chi, chi->index(vh3));

        auto it = opposite_facet_in_complex.find(c3t3.triangulation().mirror_facet(f1));
        if (it != opposite_facet_in_complex.end())
          c3t3.add_to_complex(f1, it->second);

        it = opposite_facet_in_complex.find(c3t3.triangulation().mirror_facet(f2));
        if (it != opposite_facet_in_complex.end())
          c3t3.add_to_complex(f2, it->second);
      }

      return db;
    }

    //Top cells 2-2 flip
    ch3->set_neighbor(ch3->index(vh1), ch0);
    ch3->set_neighbor(ch3->index(vh0), n_ch0_vh3);
    n_ch0_vh3->set_neighbor(n_ch0_vh3->index(ch0), ch3);

    ch0->set_neighbor(ch0->index(vh3), ch3);
    ch0->set_neighbor(ch0->index(vh2), n_ch3_vh1);
    n_ch3_vh1->set_neighbor(n_ch3_vh1->index(ch3), ch0);

    //Bottom cells 2-2 flip
    ch2->set_neighbor(ch2->index(vh1), ch1);
    ch2->set_neighbor(ch2->index(vh0), n_ch1_vh3);
    n_ch1_vh3->set_neighbor(n_ch1_vh3->index(ch1), ch2);

    ch1->set_neighbor(ch1->index(vh3), ch2);
    ch1->set_neighbor(ch1->index(vh2), n_ch2_vh1);
    n_ch2_vh1->set_neighbor(n_ch2_vh1->index(ch2), ch1);

    for (std::size_t i = 0; i < cells_around_edge.size(); i++)
    {
      for (int j = 0; j < 4; j++)
        cells_around_edge[i]->vertex(j)->set_cell(cells_around_edge[i]);
    }

    c3t3.add_to_complex(ch0, ch0->index(vh4), patch);
    c3t3.add_to_complex(ch3, ch3->index(vh4), patch);

    for (Cell_handle c : cells_around_edge)
    {
      for (int ii = 0; ii < 4; ++ii)
      {
        for (int jj = 0; jj < 4; ++jj)
        {
          Edge_uv uv = make_vertex_pair(c->vertex(ii), c->vertex(jj));
          Edge edge_uv(c, ii, jj);
          if (complex_edges.find(uv) != complex_edges.end())
          {
            if(!c3t3.is_in_complex(edge_uv))
              c3t3.add_to_complex(edge_uv, complex_edges[uv]);
          }
        }
      }
    }

    for (int i = 0; i < 4; ++i)
    {
      Cell_handle chi = cells_around_edge[i];
      Facet f1(chi, chi->index(vh0));
      Facet f2(chi, chi->index(vh2));

      auto it = opposite_facet_in_complex.find(c3t3.triangulation().mirror_facet(f1));
      if (it != opposite_facet_in_complex.end())
        c3t3.add_to_complex(f1, it->second);

      it = opposite_facet_in_complex.find(c3t3.triangulation().mirror_facet(f2));
      if (it != opposite_facet_in_complex.end())
        c3t3.add_to_complex(f2, it->second);
    }

//    /*
//    std::cout << "Ch0 "<< ch0->info()<< std::endl;
//    if(! c3t3_with_info.triangulation().is_valid(ch0, true) )
//    db = CELL_PBS;
//    std::cout << "Ch1 "<< ch1->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(ch1, true))
//    db = CELL_PBS;
//    std::cout << "Ch2 "<< ch2->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(ch2, true))
//    db = CELL_PBS;
//    std::cout << "Ch3 "<< ch3->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(ch3, true))
//    db = CELL_PBS;
//    std::cout << "n_ch3_vh1 "<< n_ch3_vh1->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(n_ch3_vh1, true))
//    db = CELL_PBS;
//    std::cout << "n_ch0_vh3 "<< n_ch0_vh3->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(n_ch0_vh3, true))
//    db = CELL_PBS;
//    std::cout << "n_ch2_vh1 "<< n_ch2_vh1->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(n_ch2_vh1, true))
//    db = CELL_PBS;
//    std::cout << "n_ch1_vh3 "<< n_ch1_vh3->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(n_ch1_vh3, true))
//    db = CELL_PBS;
//
//    */
//
    return db;
  }
  else
  {
    typename C3t3::Surface_patch_index patch = c3t3.surface_patch_index(ch0, ch0->index(vh2));
    CGAL_assertion(patch != typename C3t3::Surface_patch_index());

    CGAL_assertion(c3t3.is_in_complex(ch0, ch0->index(vh2)));
    c3t3.remove_from_complex(ch0, ch0->index(vh2));
    CGAL_assertion(c3t3.is_in_complex(ch1, ch1->index(vh2)));
    c3t3.remove_from_complex(ch1, ch1->index(vh2));

    boost::unordered_map<Facet, Surface_patch_index> opposite_facet_in_complex;
    for (int i = 0; i < 4; ++i)
    {
      Cell_handle chi = cells_around_edge[i];
      Facet f1(chi, chi->index(vh1));
      Facet f2(chi, chi->index(vh3));

      if (c3t3.is_in_complex(f1))
      {
        Surface_patch_index spi = c3t3.surface_patch_index(f1);
        opposite_facet_in_complex[c3t3.triangulation().mirror_facet(f1)] = spi;
        c3t3.remove_from_complex(f1);
      }
      if (c3t3.is_in_complex(f2))
      {
        Surface_patch_index spi = c3t3.surface_patch_index(f2);
        opposite_facet_in_complex[c3t3.triangulation().mirror_facet(f2)] = spi;
        c3t3.remove_from_complex(f2);
      }
    }

    c3t3.remove_from_complex(ch0, ch0->index(vh2));
    c3t3.remove_from_complex(ch1, ch1->index(vh2));

    boost::unordered_map<Edge_uv, typename C3t3::Curve_index> complex_edges;
    for (Cell_handle c : cells_around_edge)
    {
      for (int ii = 0; ii < 4; ++ii)
      {
        for (int jj = 0; jj < 4; ++jj)
        {
          Edge e(c, ii, jj);
          if (c3t3.is_in_complex(e))
          {
            complex_edges[make_vertex_pair(c->vertex(ii), c->vertex(jj))] = c3t3.curve_index(e);
            c3t3.remove_from_complex(e);
          }
        }
      }
    }

    // Top Flip
    ch3->set_vertex(ch3->index(vh1), vh5);
    ch2->set_vertex(ch2->index(vh3), vh4);
    ch0->set_vertex(ch0->index(vh1), vh5);
    ch1->set_vertex(ch1->index(vh3), vh4);

    Sliver_removal_result db = VALID_FLIP;
    if (!is_well_oriented(tr, ch0)
      || !is_well_oriented(tr, ch1)
      || !is_well_oriented(tr, ch2)
      || !is_well_oriented(tr, ch3))
      db = NOT_FLIPPABLE;
    else if (curr_max_cosdh < max_cos_dihedral_angle(tr, ch0)
      || curr_max_cosdh < max_cos_dihedral_angle(tr, ch1)
      || curr_max_cosdh < max_cos_dihedral_angle(tr, ch2)
      || curr_max_cosdh < max_cos_dihedral_angle(tr, ch3))
      db = NO_BEST_CONFIGURATION;

    if (db == NOT_FLIPPABLE || db == NO_BEST_CONFIGURATION)
    {
      ch3->set_vertex(ch3->index(vh5), vh1);
      ch2->set_vertex(ch2->index(vh4), vh3);
      ch0->set_vertex(ch0->index(vh5), vh1);
      ch1->set_vertex(ch1->index(vh4), vh3);

      c3t3.add_to_complex(ch0, ch0->index(vh2), patch);
      c3t3.add_to_complex(ch1, ch1->index(vh2), patch);

      for (int i = 0; i < 4; ++i)
      {
        Cell_handle chi = cells_around_edge[i];
        Facet f1(chi, chi->index(vh1));
        Facet f2(chi, chi->index(vh3));

        auto it = opposite_facet_in_complex.find(c3t3.triangulation().mirror_facet(f1));
        if (it != opposite_facet_in_complex.end())
          c3t3.add_to_complex(f1, it->second);

        it = opposite_facet_in_complex.find(c3t3.triangulation().mirror_facet(f2));
        if (it != opposite_facet_in_complex.end())
          c3t3.add_to_complex(f2, it->second);
      }

      return db;
    }

    //Left cells 2-2 flip
    Cell_handle n_ch3_vh3 = ch3->neighbor(ch3->index(vh3));
    Cell_handle n_ch2_vh1 = ch2->neighbor(ch2->index(vh1));

    ch3->set_neighbor(ch3->index(vh3), ch2);
    ch3->set_neighbor(ch3->index(vh4), n_ch2_vh1);
    n_ch2_vh1->set_neighbor(n_ch2_vh1->index(ch2), ch3);

    ch2->set_neighbor(ch2->index(vh1), ch3);
    ch2->set_neighbor(ch2->index(vh5), n_ch3_vh3);
    n_ch3_vh3->set_neighbor(n_ch3_vh3->index(ch3), ch2);

    //Right cells 2-2 flip
    Cell_handle n_ch0_vh3 = ch0->neighbor(ch0->index(vh3));
    Cell_handle n_ch1_vh1 = ch1->neighbor(ch1->index(vh1));

    ch0->set_neighbor(ch0->index(vh3), ch1);
    ch0->set_neighbor(ch0->index(vh4), n_ch1_vh1);
    n_ch1_vh1->set_neighbor(n_ch1_vh1->index(ch1), ch0);

    ch1->set_neighbor(ch1->index(vh1), ch0);
    ch1->set_neighbor(ch1->index(vh5), n_ch0_vh3);
    n_ch0_vh3->set_neighbor(n_ch0_vh3->index(ch0), ch1);

    for (unsigned int i = 0; i < cells_around_edge.size(); i++){
      for (int j = 0; j < 4; j++)
        cells_around_edge[i]->vertex(j)->set_cell(cells_around_edge[i]);
    }

    c3t3.add_to_complex(ch0, ch0->index(vh2), patch);
    c3t3.add_to_complex(ch1, ch1->index(vh2), patch);

    for (Cell_handle c : cells_around_edge)
    {
      for (int ii = 0; ii < 4; ++ii)
      {
        for (int jj = 0; jj < 4; ++jj)
        {
          Edge_uv uv = make_vertex_pair(c->vertex(ii), c->vertex(jj));
          Edge edge_uv(c, ii, jj);
          if (complex_edges.find(uv) != complex_edges.end())
          {
            if (!c3t3.is_in_complex(edge_uv))
              c3t3.add_to_complex(edge_uv, complex_edges[uv]);
          }
        }
      }
    }

    for (int i = 0; i < 4; ++i)
    {
      Cell_handle chi = cells_around_edge[i];
      Facet f1(chi, chi->index(vh4));
      Facet f2(chi, chi->index(vh5));

      auto it = opposite_facet_in_complex.find(c3t3.triangulation().mirror_facet(f1));
      if (it != opposite_facet_in_complex.end())
        c3t3.add_to_complex(f1, it->second);

      it = opposite_facet_in_complex.find(c3t3.triangulation().mirror_facet(f2));
      if (it != opposite_facet_in_complex.end())
        c3t3.add_to_complex(f2, it->second);
    }

    db = VALID_FLIP;

//    /*
//    std::cout << "Ch0 "<< ch0->info()<< std::endl;
//    if(! c3t3_with_info.triangulation().is_valid(ch0, true) )
//    db = CELL_PBS;
//    std::cout << "Ch1 "<< ch1->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(ch1, true))
//    db = CELL_PBS;
//    std::cout << "Ch2 "<< ch2->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(ch2, true))
//    db = CELL_PBS;
//    std::cout << "Ch3 "<< ch3->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(ch3, true))
//    db = CELL_PBS;
//    std::cout << "n_ch3_vh3 "<< n_ch3_vh3->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(n_ch3_vh3, true))
//    db = CELL_PBS;
//    std::cout << "n_ch2_vh1 "<< n_ch2_vh1->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(n_ch2_vh1, true))
//    db = CELL_PBS;
//    std::cout << "n_ch0_vh3 "<< n_ch0_vh3->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(n_ch0_vh3, true))
//    db = CELL_PBS;
//    std::cout << "n_ch1_vh1 "<< n_ch1_vh1->info()<< std::endl;
//    if(!c3t3_with_info.triangulation().is_valid(n_ch1_vh1, true))
//    db = CELL_PBS;
//    */
    return db;
  }

  return NOT_FLIPPABLE;
}

template<typename C3T3, typename SurfaceIndexMapMap, typename Flip_Criterion, typename Visitor>
std::size_t flipBoundaryEdges(
  C3T3& c3t3,
  std::vector<typename C3T3::Edge>& boundary_edges,
  SurfaceIndexMapMap& boundary_vertices_valences,
  boost::unordered_map<typename C3T3::Vertex_handle, std::vector<typename C3T3::Subdomain_index> >&  vertices_subdomain_indices,
  Flip_Criterion flip_criterion,
  Visitor& visitor)
{
  typedef typename C3T3::Vertex_handle Vertex_handle;
  typedef typename C3T3::Cell_handle   Cell_handle;
  typedef typename C3T3::Facet         Facet;
  typedef typename C3T3::Edge          Edge;
  typedef typename C3T3::Surface_patch_index Surface_patch_index;
  typedef typename C3T3::Subdomain_index Subdomain_index;
  typedef typename C3T3::Triangulation Tr;
  typedef std::pair<Vertex_handle, Vertex_handle> Edge_vv;

  std::size_t nb = 0;

  Tr& tr = c3t3.triangulation();

  std::vector<Edge_vv> candidate_edges_for_flip;

  for (std::size_t i = 0; i < boundary_edges.size(); i++)
  {
    const Edge& e = boundary_edges[i];
    if (c3t3.is_in_complex(e))
      continue;
    else
    {
      const Vertex_handle& vh0 = e.first->vertex(e.second);
      const Vertex_handle& vh1 = e.first->vertex(e.third);
      candidate_edges_for_flip.push_back(std::make_pair(vh0, vh1));
    }
  }

  std::unordered_map<Vertex_handle,
    boost::optional<boost::container::small_vector<Cell_handle, 64> > > inc_cells;

  for (std::size_t ii = 0; ii < candidate_edges_for_flip.size(); ++ii)
  {
    const std::pair<Vertex_handle, Vertex_handle>& vp = candidate_edges_for_flip[ii];
    const Vertex_handle& vh0 = vp.first;
    const Vertex_handle& vh1 = vp.second;

    const typename C3T3::Triangulation::Point p0 = vh0->point();
    const typename C3T3::Triangulation::Point p1 = vh1->point();

    boost::optional<boost::container::small_vector<Cell_handle, 64>>&
      o_inc_vh = inc_cells[vp.first];
    if (o_inc_vh == boost::none || o_inc_vh.get().empty())
    {
      boost::container::small_vector<Cell_handle, 64> inc_vec;
      tr.incident_cells(vp.first, std::back_inserter(inc_vec));
      o_inc_vh = inc_vec;
    }

    Cell_handle c;
    int i, j;
    if (!is_edge_uv(vp.first, vp.second, boost::get(o_inc_vh), c, i, j))
      continue;

    std::vector<Facet> boundary_facets;
    Surface_patch_index surfi;

    std::ofstream incf("dump_incident_facets.polylines.txt");

    Edge edge(c, i, j);
    typename Tr::Facet_circulator fcirc = tr.incident_facets(edge);
    typename Tr::Facet_circulator done(fcirc);
    do
    {
      if (c3t3.is_in_complex(*fcirc))
      {
        surfi = c3t3.surface_patch_index(*fcirc);
        boundary_facets.push_back(*fcirc);
        CGAL_assertion(Surface_patch_index() != surfi);

        Facet ff = *fcirc;
        incf << "4 " << ff.first->vertex((ff.second + 1) % 4)->point().point()
             << " " << ff.first->vertex((ff.second + 2) % 4)->point().point()
             << " " << ff.first->vertex((ff.second + 3) % 4)->point().point()
             << " " << ff.first->vertex((ff.second + 4) % 4)->point().point() << std::endl;
      }
    } while (++fcirc != done);
    incf.close();

    if (boundary_facets.size() == 2)
    {
      const Facet& f0 = boundary_facets[0];
      const Facet& f1 = boundary_facets[1]; 

      Vertex_handle vh2;
      Vertex_handle vh3;
      for (int i = 0; i < 3; i++)
      {
        Vertex_handle v2 = f0.first->vertex(indices(f0.second, i));
        Vertex_handle v3 = f1.first->vertex(indices(f1.second, i));
        if (v2 != vh0 && v2 != vh1)
          vh2 = v2;
        if (v3 != vh0 && v3 != vh1)
          vh3 = v3;
      }

      Cell_handle t_ch;
      int t_i0, t_i1;
      if (!tr.is_edge(vh2, vh3, t_ch, t_i0, t_i1))
      {
        nb_surface_flip_candidates++;

        int v0 = boundary_vertices_valences[vh0][surfi];
        int v1 = boundary_vertices_valences[vh1][surfi];
        int v2 = boundary_vertices_valences[vh2][surfi];
        int v3 = boundary_vertices_valences[vh3][surfi];
        int m0 = (vertices_subdomain_indices[vh0].size() >2 ? 4 : 6);
        int m1 = (vertices_subdomain_indices[vh1].size() >2 ? 4 : 6);
        int m2 = (vertices_subdomain_indices[vh2].size() >2 ? 4 : 6);
        int m3 = (vertices_subdomain_indices[vh3].size() >2 ? 4 : 6);

        int initial_cost = (v0 - m0)*(v0 - m0)
                         + (v1 - m1)*(v1 - m1)
                         + (v2 - m2)*(v2 - m2)
                         + (v3 - m3)*(v3 - m3);
        v0--;
        v1--;
        v2++;
        v3++;

        int final_cost = (v0 - m0)*(v0 - m0)
                       + (v1 - m1)*(v1 - m1)
                       + (v2 - m2)*(v2 - m2)
                       + (v3 - m3)*(v3 - m3);
        if (initial_cost > final_cost)
        {
          int nbf = std::distance(c3t3.facets_in_complex_begin(),
                                  c3t3.facets_in_complex_end());

//          CGAL::dump_c3t3(c3t3, "dump_before_flip_");

          Sliver_removal_result db = flip_on_surface(c3t3, edge, vh2, vh3,
                                                     inc_cells,
                                                     flip_criterion,
                                                     visitor);
          if (db == VALID_FLIP)
          {
            CGAL_assertion(tr.tds().is_edge(vh2, vh3));
            Cell_handle c;
            int li, lj, lk;
            CGAL_assertion_code(bool b =)
            tr.tds().is_facet(vh2, vh3, vh0, c, li, lj, lk);
            CGAL_assertion(b);
            c3t3.add_to_complex(c, (6 - li - lj - lk), surfi);

            CGAL_assertion_code(b = )
            tr.tds().is_facet(vh2, vh3, vh1, c, li, lj, lk);
            CGAL_assertion(b);
            c3t3.add_to_complex(c, (6 - li - lj - lk), surfi);

            std::cout << " done" << std::endl;
            int nbf_post = std::distance(c3t3.facets_in_complex_begin(),
                                         c3t3.facets_in_complex_end());
            CGAL_assertion(nbf == nbf_post);

            nb_surface_flip_done++;
            nb++;

//            CGAL::dump_c3t3(c3t3, "dump_after_flip_");

//            std::ofstream ofs("dump_edge_flipped.polylines.txt");
//            ofs << "2 " << p0.point() << " " << p1.point() << std::endl;
//            ofs.close();

            boundary_vertices_valences[vh0][surfi]--;
            boundary_vertices_valences[vh1][surfi]--;
            boundary_vertices_valences[vh2][surfi]++;
            boundary_vertices_valences[vh3][surfi]++;
          }
          else if (db == INVALID_CELL || db == INVALID_VERTEX || db == INVALID_ORIENTATION)
          {
            std::cout << "Cell problem" << std::endl;
            return nb;
          }
          else
            std::cout << " failed" << std::endl;
        }
      }
    }
  }
  CGAL_assertion(tr.tds().is_valid(true));
  return nb;
}

template<typename C3T3, typename CellSelector, typename Visitor>
void flip_edges(C3T3& c3t3,
                const bool protect_boundaries,
                CellSelector& cell_selector,
                Visitor& visitor)
{
  CGAL_USE(protect_boundaries);
  typedef typename C3T3::Triangulation       T3;
  typedef typename T3::Vertex_handle         Vertex_handle;
  typedef typename T3::Edge                  Edge;
  typedef typename std::pair<Vertex_handle, Vertex_handle> Edge_vv;
  typedef typename C3T3::Subdomain_index     Subdomain_index;

#ifdef CGAL_TETRAHEDRAL_REMESHING_VERBOSE
  std::cout << "Flip edges...";
  std::cout.flush();
  std::size_t nb_flips = 0;
#endif

  //const Flip_Criterion criterion = VALENCE_MIN_DH_BASED;

  //compute vertices normals map?

  typedef typename C3T3::Surface_patch_index Surface_patch_index;
  typedef boost::unordered_map<Surface_patch_index, unsigned int> Spi_map;
  if (!protect_boundaries)
  {
    std::cout << "\tBoundary flips" << std::endl;
    //Boundary flip
    std::vector<Edge> boundary_edges;
    boost::unordered_map<Vertex_handle, Spi_map> boundary_vertices_valences;
    boost::unordered_map<Vertex_handle, std::vector<Subdomain_index> > vertices_subdomain_indices;
    collectBoundaryEdgesAndComputeVerticesValences(c3t3,
                                                   cell_selector,
                                                   boundary_edges,
                                                   boundary_vertices_valences,
                                                   vertices_subdomain_indices);

    if(!debug::are_cell_orientations_valid(c3t3.triangulation()))
      std::cerr << "ERROR in ORIENTATION" << std::endl;

  //  if (criterion == VALENCE_BASED)
  //    flipBoundaryEdges(boundary_edges, boundary_vertices_valences, VALENCE_BASED);
  //  else
#ifdef CGAL_TETRAHEDRAL_REMESHING_VERBOSE
    nb_flips +=
#endif
      flipBoundaryEdges(c3t3, boundary_edges, boundary_vertices_valences,
                        vertices_subdomain_indices, MIN_ANGLE_BASED, visitor);
  }

//  std::vector<Edge_vv> inside_edges;
//  get_internal_edges(c3t3,
//                     cell_selector,
//                     std::back_inserter(inside_edges));
//
//  //if (criterion == VALENCE_BASED)
//  //  flip_inside_edges(inside_edges);
//  //else
//  //{
//#ifdef CGAL_TETRAHEDRAL_REMESHING_VERBOSE
//  nb_flips +=
//#endif
//    flip_all_edges(inside_edges, c3t3, MIN_ANGLE_BASED, visitor);
//  //}

#ifdef CGAL_TETRAHEDRAL_REMESHING_VERBOSE
  nb_flips +=
#endif
    flip_all_edges(inside_edges, c3t3, MIN_ANGLE_BASED, cell_selector, visitor);
  //}

#ifdef CGAL_TETRAHEDRAL_REMESHING_VERBOSE
  std::cout << " done (" << nb_flips << " flips)." << std::endl;
#endif
}

}//namespace internal
}//namespace Tetrahedral_remeshing
}//namespace CGAL

#endif // CGAL_INTERNAL_FLIP_EDGES_H
