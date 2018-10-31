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
#include <stdio.h>

#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>

#include "gst3dcamera_arcball.h"
#include "gst3dmath.h"
#include "gst3drenderer.h"

#define GST_CAT_DEFAULT gst_3d_camera_arcball_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

G_DEFINE_TYPE_WITH_CODE (Gst3DCameraArcball, gst_3d_camera_arcball,
                         GST_3D_TYPE_CAMERA, GST_DEBUG_CATEGORY_INIT (gst_3d_camera_arcball_debug,
                             "3dcamera_arcball", 0, "camera_arcball"));


static void gst_3d_camera_arcball_update_view (Gst3DCamera * cam);
static void gst_3d_camera_arcball_navigation_event (Gst3DCamera * cam, GstEvent * event);
static void gst_3d_camera_arcball_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_3d_camera_arcball_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

enum PROPERTY_CAMERA {
  PROPERTY_0,
  PROPERTY_CENTER_DISTANCE,
  PROPERTY_SCROLL_SPEED,
  PROPERTY_ROTATION_SPEED,
  PROPERTY_THETA,
  PROPERTY_PHI,
  N_PROPERITES
};

void
gst_3d_camera_arcball_init (Gst3DCameraArcball * self)
{
  self->center_distance = 0.8;//眼睛在 半径=center_distance 的球面上移动，一直看向原点
  self->scroll_speed = 0.03;
  self->rotation_speed = 0.002;

  self->theta = 90.f / 180.f * M_PI;//90.0 与y轴的夹角，与center_distance结合用于确定眼睛的位置
  self->phi   = 90.f / 180.f * M_PI;//0.00 与x轴的夹角，与center_distance结合用于确定眼睛的位置

  self->zoom_step = 0.95;
  self->min_fov = 45;
  self->max_fov = 110;
  self->update_view_funct = &gst_3d_camera_arcball_update_view_from_matrix;
}

Gst3DCameraArcball *
gst_3d_camera_arcball_new (void)
{
  Gst3DCameraArcball *camera_arcball = g_object_new (GST_3D_TYPE_CAMERA_ARCBALL, NULL);
  return camera_arcball;
}

static void
gst_3d_camera_arcball_finalize (GObject * object)
{
  Gst3DCameraArcball *self = GST_3D_CAMERA_ARCBALL (object);
  g_return_if_fail (self != NULL);

  // GST_3D_CAMERA_CLASS (gst_3d_camera_arcball_parent_class)->finalize (object);
  G_OBJECT_CLASS (gst_3d_camera_arcball_parent_class)->finalize (object);
}

static void
gst_3d_camera_arcball_class_init (Gst3DCameraArcballClass * klass)
{
  // g_print ("gst_3d_camera_arcball_class_init.\n");
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = gst_3d_camera_arcball_finalize;
  gobject_class->set_property = gst_3d_camera_arcball_set_property;
  gobject_class->get_property = gst_3d_camera_arcball_get_property;

  Gst3DCameraClass *camera_class = GST_3D_CAMERA_CLASS (klass);
  camera_class->update_view = gst_3d_camera_arcball_update_view;
  camera_class->navigation_event = gst_3d_camera_arcball_navigation_event;

  GParamSpec *properties[N_PROPERITES] = {NULL,};
  properties[PROPERTY_CENTER_DISTANCE] = g_param_spec_float ("center_distance", "center_distance", "camera center_distance", 0, G_MAXUINT, 0, G_PARAM_READWRITE);
  properties[PROPERTY_SCROLL_SPEED]    = g_param_spec_float ("scroll_speed", "scroll_speed", "camera scroll_speed", 0, G_MAXUINT, 0, G_PARAM_READWRITE);
  properties[PROPERTY_ROTATION_SPEED]  = g_param_spec_float ("rotation_speed", "rotation_speed", "camera rotation_speed", 0, G_MAXUINT, 0, G_PARAM_READWRITE);
  properties[PROPERTY_THETA]           = g_param_spec_float ("theta", "theta", "camera theta", 0, G_MAXUINT, 0, G_PARAM_READWRITE);
  properties[PROPERTY_PHI]             = g_param_spec_float ("phi", "phi", "camera phi", 0, G_MAXUINT, 0, G_PARAM_READWRITE);
  
  g_object_class_install_properties (gobject_class, N_PROPERITES, properties);
}

void
gst_3d_camera_arcball_translate (Gst3DCameraArcball * self, float z)
{
  float new_val = self->center_distance + z * self->scroll_speed;

  self->center_distance = MAX (0, new_val);

  GST_DEBUG ("center distance: %f", self->center_distance);
  gst_3d_camera_update_view (GST_3D_CAMERA (self));
}

void
gst_3d_camera_arcball_rotate (Gst3DCameraArcball * self, gdouble x, gdouble y)
{
  float delta_theta = y * self->rotation_speed;
  float delta_phi = x * self->rotation_speed;

  self->phi += delta_phi;

  /* 2π < θ < π to avoid gimbal lock */
  float next_theta_pi = (self->theta + delta_theta) / M_PI;
  if (next_theta_pi < 2.0 && next_theta_pi > 1.0)
    self->theta += delta_theta;

  GST_DEBUG ("θ = %fπ ϕ = %fπ", self->theta / M_PI, self->phi / M_PI);
  gst_3d_camera_update_view (GST_3D_CAMERA (self));
}

static void
gst_3d_camera_arcball_update_view (Gst3DCamera * cam)
{
  Gst3DCameraArcball *self = GST_3D_CAMERA_ARCBALL (cam);
  self->update_view_funct (self);
}

///add by DuffAb
void 
gst_3d_camera_arcball_update_view_from_matrix (Gst3DCameraArcball * self)
{
  Gst3DCamera * cam = GST_3D_CAMERA (self);
  
  float radius = exp (self->center_distance);
  GST_LOG_OBJECT (self, "arcball radius = %f fov %f", radius, cam->fov);
  // g_print ("radius(%f), x(%f)  y(%f) z(%f).\n", radius, radius * sin (self->theta) * cos (self->phi), radius * -cos (self->theta), radius * sin (self->theta) * sin (self->phi));
  // 确定左摄像头的位置   double sin(double x) 传入的是弧度值
  graphene_vec3_init (&cam->eye,
      radius * sin (self->theta) * cos (self->phi),
      radius * -cos (self->theta),
      radius * sin (self->theta) * sin (self->phi));
  // 确定右摄像头的位置
  graphene_vec3_init (&cam->eye_right,
      radius * sin (self->theta) * cos (self->phi) * (-1.f),
      radius * -cos (self->theta) * (-1.f),
      radius * sin (self->theta) * sin (self->phi) * (-1.f));

  graphene_matrix_t projection_matrix;
  graphene_matrix_init_perspective (&projection_matrix, cam->fov, cam->aspect, cam->znear, cam->zfar);

  graphene_matrix_t view_matrix;
  graphene_matrix_init_look_at (&view_matrix, &cam->eye, &cam->center, &cam->up);

  /* fix graphene look at */
  graphene_matrix_t v_inverted;
  graphene_matrix_t v_inverted_fix;
  graphene_matrix_inverse (&view_matrix, &v_inverted);
  gst_3d_math_matrix_negate_component (&v_inverted, 3, 2, &v_inverted_fix);

  graphene_matrix_multiply (&v_inverted_fix, &projection_matrix, &cam->mvp);//这里更新 模型视图投影矩阵
}

static void
gst_3d_camera_arcball_navigation_event (Gst3DCamera * cam, GstEvent * event)
{
  GstNavigationEventType event_type = gst_navigation_event_get_type (event);

  // 返回 - 事件的结构. 该结构仍归事件所有,这意味着不应释放它,并且当您释放事件时指针变为无效,多线程安全
  GstStructure *structure = (GstStructure *) gst_event_get_structure (event);

  Gst3DCameraArcball *self = GST_3D_CAMERA_ARCBALL (cam);

  switch (event_type) {
  case GST_NAVIGATION_EVENT_MOUSE_MOVE: {
    /* hanlde the mouse motion for zooming and rotating the view */
    gdouble x, y;
    gst_structure_get_double (structure, "pointer_x", &x);
    gst_structure_get_double (structure, "pointer_y", &y);

    gdouble dx, dy;
    dx = x - cam->cursor_last_x;
    dy = y - cam->cursor_last_y;

    if (cam->pressed_mouse_button == 1) {
      GST_DEBUG ("Rotating [%fx%f]", dx, dy);
      gst_3d_camera_arcball_rotate (self, dx, dy);
    }
    cam->cursor_last_x = x;
    cam->cursor_last_y = y;
    break;
  }
  case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE: {
    gint button;
    Gst3DCamera * cam = GST_3D_CAMERA (self);

    gst_structure_get_int (structure, "button", &button);
    cam->pressed_mouse_button = 0;

    if (button == 1) {
      /* first mouse button release */
      gst_structure_get_double (structure, "pointer_x", &cam->cursor_last_x);
      gst_structure_get_double (structure, "pointer_y", &cam->cursor_last_y);
    } else if (button == 4 || button == 6) {
      /* wheel up */
      //gst_3d_camera_arcball_translate (self, -1.0);
      if (cam->fov > self->min_fov) {
        cam->fov *= self->zoom_step;
        cam->fov = MAX (self->min_fov, cam->fov);
        gst_3d_camera_update_view (cam);
      }
    } else if (button == 5 || button == 7) {
      /* wheel down */
      //gst_3d_camera_arcball_translate (self, 1.0);
      if (cam->fov < self->max_fov) {
        cam->fov /= self->zoom_step;
        cam->fov = MIN (self->max_fov, cam->fov);
        gst_3d_camera_update_view (cam);
      }
    }
    break;
  }
  case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS: {
    gint button;
    gst_structure_get_int (structure, "button", &button);
    cam->pressed_mouse_button = button;
    break;
  }
  default:
    break;
  }
}

static void
gst_3d_camera_arcball_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  // g_print ("gst_3d_camera_arcball_set_property.\n");
  Gst3DCameraArcball *self = GST_3D_CAMERA_ARCBALL (object);

  switch (property_id) {
  case PROPERTY_CENTER_DISTANCE:
    self->center_distance = g_value_get_float(value);
    break;
  case PROPERTY_SCROLL_SPEED:
    self->scroll_speed = g_value_get_float(value);
    break;
  case PROPERTY_ROTATION_SPEED:
    self->rotation_speed = g_value_get_float(value);
    break;
  case PROPERTY_THETA:
    self->theta = g_value_get_float(value);
    break;
  case PROPERTY_PHI:
    self->phi = g_value_get_float(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
gst_3d_camera_arcball_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  ;
}