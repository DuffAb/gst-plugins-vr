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

/**
 * SECTION:element-glcompositor
 *
 * Warp HMD distortion.
 *
 * <refsect2>
 * <title>Examples</title>
 * |[
 * gst-launch-1.0 filesrc location=~/Videos/360.webm ! decodebin ! glupload ! glcolorconvert ! glcompositor ! hmdwarp ! glimagesink
 * ]| Play spheric video.
 * </refsect2>
 */

#define GST_USE_UNSTABLE_API

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gsthmdwarp.h"

#include <gst/gl/gstglapi.h>
#include <graphene-gobject.h>
#include <gio/gio.h>

#define GST_CAT_DEFAULT gst_hmd_warp_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define gst_hmd_warp_parent_class parent_class

enum
{
  PROP_0,
};

#define DEBUG_INIT \
    GST_DEBUG_CATEGORY_INIT (gst_hmd_warp_debug, "glcompositor", 0, "glcompositor element");

G_DEFINE_TYPE_WITH_CODE (GstHmdWarp, gst_hmd_warp,
    GST_TYPE_GL_FILTER, DEBUG_INIT);

static void gst_hmd_warp_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_hmd_warp_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_hmd_warp_set_caps (GstGLFilter * filter, GstCaps * incaps, GstCaps * outcaps);

static void gst_hmd_warp_gl_stop (GstGLBaseFilter * filter);
static gboolean gst_hmd_warp_stop (GstBaseTransform * trans);
static gboolean gst_hmd_warp_init_gl (GstGLFilter * filter);
static gboolean gst_hmd_warp_draw (gpointer stuff);

static gboolean gst_hmd_warp_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex, GstGLMemory * out_tex);

static void
gst_hmd_warp_class_init (GstHmdWarpClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_hmd_warp_set_property;
  gobject_class->get_property = gst_hmd_warp_get_property;

  GST_GL_BASE_FILTER_CLASS (klass)->gl_stop = gst_hmd_warp_gl_stop;

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  GST_GL_FILTER_CLASS (klass)->init_fbo = gst_hmd_warp_init_gl;
  GST_GL_FILTER_CLASS (klass)->set_caps = gst_hmd_warp_set_caps;
  GST_GL_FILTER_CLASS (klass)->filter_texture = gst_hmd_warp_filter_texture;
  base_transform_class->stop = gst_hmd_warp_stop;

  gst_element_class_set_metadata (element_class, "HMD warp",
      "Filter/Effect/Video", "Warp HMD distortion",
      "Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>\n");

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2;
}

static void
gst_hmd_warp_init (GstHmdWarp * self)
{
  self->shader = NULL;
  self->in_tex = 0;
  self->render_plane = NULL;
}

static void
gst_hmd_warp_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_hmd_warp_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_hmd_warp_set_caps (GstGLFilter * filter, GstCaps * incaps, GstCaps * outcaps)
{
  GstHmdWarp *self = GST_HMD_WARP (filter);

  graphene_vec2_init (&self->screen_size,
      (gdouble) GST_VIDEO_INFO_WIDTH (&filter->out_info),
      (gdouble) GST_VIDEO_INFO_HEIGHT (&filter->out_info));

  self->aspect = graphene_vec2_get_x (&self->screen_size) / graphene_vec2_get_y (&self->screen_size);

  GST_DEBUG ("caps change, res: %dx%d",
      GST_VIDEO_INFO_WIDTH (&filter->out_info),
      GST_VIDEO_INFO_HEIGHT (&filter->out_info));

  self->caps_change = TRUE;

  return TRUE;
}

static void
gst_hmd_warp_gl_stop (GstGLBaseFilter * filter)
{
  GstHmdWarp *self = GST_HMD_WARP (filter);

  if (self->shader) {
    gst_3d_shader_delete (self->shader);
    gst_object_unref (self->shader);
    self->shader = NULL;
  }
  if (self->render_plane) {
    gst_object_unref (self->render_plane);
    self->render_plane = NULL;
  }

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (filter);
}

static gboolean
gst_hmd_warp_stop (GstBaseTransform * trans)
{
  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (trans);
}

static gboolean
gst_hmd_warp_init_gl (GstGLFilter * filter)
{
  GstHmdWarp *self = GST_HMD_WARP (filter);
  GstGLContext *context = GST_GL_BASE_FILTER (self)->context;
  GstGLFuncs *gl = context->gl_vtable;
  GError *error = NULL;

  if (!self->render_plane) {
    self->shader = gst_3d_shader_new (context);
    if (!gst_3d_shader_from_vert_frag (self->shader, "mvp_uv.vert", "warp.frag", &error))
      goto handle_error;

    gst_3d_shader_bind (self->shader);

    self->render_plane = gst_3d_mesh_new_plane (context, self->aspect);

    gl->ClearColor (0.f, 0.f, 0.f, 0.f);
    gl->ActiveTexture (GL_TEXTURE0);
    gst_gl_shader_set_uniform_1i (self->shader->shader, "texture", 0);

    gst_gl_shader_use (self->shader->shader);
    gst_3d_shader_upload_vec2 (self->shader, &self->screen_size, "screen_size");

    gst_3d_mesh_bind_shader (self->render_plane, self->shader);
  }
  return TRUE;

handle_error:
  GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND, ("%s", error->message), (NULL));
  g_clear_error (&error);

  return FALSE;
}

static gboolean
gst_hmd_warp_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex, GstGLMemory * out_tex)
{
  GstHmdWarp *self = GST_HMD_WARP (filter);

  // RUN_TIME_COUNT(210);

  self->in_tex = in_tex;
  // RUN_TIME_BEGIN();
  gst_gl_framebuffer_draw_to_texture (filter->fbo, out_tex,
      gst_hmd_warp_draw, (gpointer) self);
  // RUN_TIME_END();
  return TRUE;
}

static gboolean
gst_hmd_warp_draw (gpointer this)
{
  GstHmdWarp *self = GST_HMD_WARP (this);
  GstGLContext *context = GST_GL_BASE_FILTER (this)->context;
  GstGLFuncs *gl = context->gl_vtable;

  gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  gst_gl_shader_use (self->shader->shader);
  gl->BindTexture (GL_TEXTURE_2D, self->in_tex->tex_id);

  graphene_matrix_t projection_ortho;
  graphene_matrix_init_ortho (&projection_ortho, -self->aspect, self->aspect, -1.0, 1.0, -1.0, 1.0);
  gst_3d_shader_upload_matrix (self->shader, &projection_ortho, "mvp");
  gst_3d_mesh_bind (self->render_plane);
  gst_3d_mesh_draw (self->render_plane);

  gl->BindVertexArray (0);
  gl->BindTexture (GL_TEXTURE_2D, 0);
  gst_gl_context_clear_shader (context);

  return TRUE;
}
