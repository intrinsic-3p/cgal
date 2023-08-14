// Copyright(c) 2012, 2020  Tel - Aviv University(Israel).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-Commercial
//
// Author(s): Engin Deniz Diktas <denizdiktas@gmail.com>

#include "SingleVertex.h"


SingleVertex::SingleVertex(const QVector3D& pos)
{
  initializeOpenGLFunctions();

  m_pos = pos;
  m_visible = true;

  // DEFINE OPENGL BUFFERS
  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);

  // Vertex Buffer
  glGenBuffers(1, &m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  auto vertex_buffer_size = sizeof(m_pos);
  auto vertex_buffer_data = reinterpret_cast<const void*>(&m_pos);
  glBufferData(GL_ARRAY_BUFFER,
               vertex_buffer_size,
               vertex_buffer_data,
               GL_DYNAMIC_DRAW);

  // Position Vertex-Attribute
  GLint position_attrib_index = 0;
  const void* position_offset = 0;
  GLsizei stride = 0;
  glVertexAttribPointer(position_attrib_index,
                        3,
                        GL_FLOAT, GL_FALSE,
                        stride,
                        position_offset);
  glEnableVertexAttribArray(position_attrib_index);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}


void SingleVertex::set_visible(bool flag)
{
  m_visible = flag;
}
void SingleVertex::set_pos(const QVector3D& pos)
{
  m_pos = pos;
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  auto vertex_buffer_size = sizeof(m_pos);
  auto vertex_buffer_data = reinterpret_cast<const void*>(&m_pos);
  auto offset = 0;
  glBufferSubData(GL_ARRAY_BUFFER, 
                  offset,
                  vertex_buffer_size,
                  vertex_buffer_data);
}
const QVector3D& SingleVertex::get_pos() const
{
  return m_pos;
}


void SingleVertex::draw()
{
  if (m_visible)
  {
    glBindVertexArray(m_vao);
      glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
  }
}

