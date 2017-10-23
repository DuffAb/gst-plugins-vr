/*
 * GStreamer Plugins VR
 * Copyright (C) 2016 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>

#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

#include <gio/gio.h>

#include "gst3dshader.h"

#define GST_CAT_DEFAULT gst_3d_shader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

G_DEFINE_TYPE_WITH_CODE (Gst3DShader, gst_3d_shader, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_3d_shader_debug, "3dshader", 0, "shader"));

void
gst_3d_shader_init (Gst3DShader * self)
{
  self->shader = NULL;
}

Gst3DShader *
gst_3d_shader_new (GstGLContext * context)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  Gst3DShader *shader = g_object_new (GST_3D_TYPE_SHADER, NULL);
  shader->context = gst_object_ref (context);
  return shader;
}

Gst3DShader *
gst_3d_shader_new_vert_frag (GstGLContext * context, const gchar * vertex,
    const gchar * fragment, GError **error)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  Gst3DShader *shader = gst_3d_shader_new (context);
  if (!gst_3d_shader_from_vert_frag (shader, vertex, fragment, error)) {
    gst_object_unref (shader);
    return NULL;
  }

  return shader;
}

static void
gst_3d_shader_finalize (GObject * object)
{
  Gst3DShader *self = GST_3D_SHADER (object);
  g_return_if_fail (self != NULL);

  if (self->context) {
    gst_object_unref (self->context);
    self->context = NULL;
  }

  G_OBJECT_CLASS (gst_3d_shader_parent_class)->finalize (object);
}

static void
gst_3d_shader_class_init (Gst3DShaderClass * klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  obj_class->finalize = gst_3d_shader_finalize;
}

void
gst_3d_shader_bind (Gst3DShader * self)
{
  gst_gl_shader_use (self->shader);
}

const char *
gst_3d_shader_read (const char *file)
{
  GBytes *bytes = NULL;
  GError *error = NULL;
  const char *shader;

  char *path = g_strjoin ("", "/gpu/", file, NULL);
  GST_LOG ("Loading shader from file: %s", path);
  bytes = g_resources_lookup_data (path, 0, &error);
  g_free (path);

  if (bytes) {
    shader = (const gchar *) g_bytes_get_data (bytes, NULL);
    g_bytes_unref (bytes);
  } else {
    if (error != NULL) {
      GST_ERROR ("Unable to read file: %s", error->message);
      g_error_free (error);
    }
    return "";
  }
  return shader;
}

void
gst_3d_shader_delete (Gst3DShader * self)
{
  if (self->shader != NULL) {
    gst_object_unref (self->shader);
    self->shader = NULL;
  }
}

gboolean
gst_3d_shader_from_vert_frag (Gst3DShader * self, const gchar * vertex,
    const gchar * fragment, GError **error)
{
  gboolean ret = FALSE;
  GstGLShader *shader = NULL;
  GstGLContext *context = self->context;

  if (gst_gl_context_get_gl_api (context)) {
    GstGLSLStage *stage;

    const gchar *vertex_src = gst_3d_shader_read (vertex);
    const gchar *fragment_src = gst_3d_shader_read (fragment);

    GST_LOG_OBJECT (self, "Creating shader from vertex src %s, fragment src %s",
        vertex_src, fragment_src);

    shader = gst_gl_shader_new (context);

    if (!(stage = gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
                GST_GLSL_VERSION_NONE, GST_GLSL_PROFILE_NONE, vertex_src))) {
      goto print_error;
    }

    if (!gst_gl_shader_compile_attach_stage (shader, stage, error)) {
      gst_object_unref (stage);
      goto print_error;
    }

    if (!(stage = gst_glsl_stage_new_with_string (context, GL_FRAGMENT_SHADER,
                GST_GLSL_VERSION_NONE, GST_GLSL_PROFILE_NONE, fragment_src))) {
      goto print_error;
    }

    if (!gst_gl_shader_compile_attach_stage (shader, stage, error)) {
      gst_object_unref (stage);
      goto print_error;
    }

    if (!gst_gl_shader_link (shader, error)) {
      goto print_error;
    }
    if (self->shader)
      gst_object_unref (self->shader);
    self->shader = gst_object_ref (shader);
    ret = TRUE;
  }

  return ret;

print_error:
  if (shader)
    gst_object_unref (shader);

  return FALSE;
}

void
gst_3d_shader_upload_matrix (Gst3DShader * self, graphene_matrix_t * mat,
    const gchar * name)
{
  GLfloat temp_matrix[16];
  graphene_matrix_to_float (mat, temp_matrix);
  gst_gl_shader_set_uniform_matrix_4fv (self->shader, name, 1, GL_FALSE,
      temp_matrix);
}

void
gst_3d_shader_upload_vec2 (Gst3DShader * self, graphene_vec2_t * vec,
    const gchar * name)
{
  GLfloat temp_vec[2];
  graphene_vec2_to_float (vec, temp_vec);
  gst_gl_shader_set_uniform_2fv (self->shader, name, 1, temp_vec);
}
