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

#ifndef __GST_3D_CAMERA_H__
#define __GST_3D_CAMERA_H__


#include <gst/gst.h>
#include <gst/gl/gstgl_fwd.h>
#include <graphene.h>

G_BEGIN_DECLS
#define GST_3D_TYPE_CAMERA            (gst_3d_camera_get_type ())
#define GST_3D_CAMERA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_3D_TYPE_CAMERA, Gst3DCamera))
#define GST_3D_CAMERA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_3D_TYPE_CAMERA, Gst3DCameraClass))
#define GST_IS_3D_CAMERA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_3D_TYPE_CAMERA))
#define GST_IS_3D_CAMERA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_3D_TYPE_CAMERA))
#define GST_3D_CAMERA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_3D_TYPE_CAMERA, Gst3DCameraClass))
typedef struct _Gst3DCamera Gst3DCamera;
typedef struct _Gst3DCameraClass Gst3DCameraClass;

#define RUN_TIME_BEGIN()  clock_t begin = clock(); \
                          do{}while(0)
#define RUN_TIME_END()    clock_t end = clock(); \
                          g_print ("%s (%f ms)\n",  __FUNCTION__, (double)(end - begin) / CLOCKS_PER_SEC * 1000);\
                          do{}while(0)

#define RUN_TIME_COUNT(number)    \
                          static clock_t last##__LINE__ = 0; \
                          static clock_t max##__LINE__ = 0; \
                          static clock_t min##__LINE__ = -1; \
                          static clock_t avg##__LINE__ = 0; \
                          static uint32_t count##__LINE__; \
                          clock_t curr##__LINE__; \
                          clock_t diff##__LINE__; \
                          if(!last##__LINE__) last##__LINE__ = clock(); \
                          else { \
                            curr##__LINE__ = clock(); \
                            diff##__LINE__ = curr##__LINE__ - last##__LINE__; \
                            last##__LINE__ = curr##__LINE__; \
                            if(diff##__LINE__ > max##__LINE__) max##__LINE__ = diff##__LINE__; \
                            if(diff##__LINE__ < min##__LINE__) min##__LINE__ = diff##__LINE__; \
                            avg##__LINE__ += diff##__LINE__; \
                            if(count##__LINE__ == number) { \
                              g_print ("%s<%d> max(%fms) min(%fms) avg(%fms)\n",  __FUNCTION__, __LINE__, (double)(max##__LINE__) / CLOCKS_PER_SEC * 1000, (double)(min##__LINE__) / CLOCKS_PER_SEC * 1000, (double)(avg##__LINE__) / CLOCKS_PER_SEC * 1000 / number);\
                              avg##__LINE__ = 0; \
                              count##__LINE__ = 0; \
                            } \
                            count##__LINE__++; \
                          } \
                          do{}while(0)

struct _Gst3DCamera
{
  /*< private > */
  GstObject parent;
  graphene_matrix_t mvp;

  /* position */
  graphene_vec3_t eye;          //ÉãÏñ»úµÄÎ»ÖÃ position
  graphene_vec3_t center;       //ÉãÏñ»ú¿´ÏòµÄÎ»ÖÃ Ó¦¸ÃÊÇ²»Í£¸Ä±äµÄ£¬µ«ÊÇÕâ¸öÏîÄ¿ÖÐÃ»ÓÐ¸Ä±ä£¬ÐèÒªÐÞ¸Ä front
  graphene_vec3_t up;           //ÉãÏñ»úÏòÉÏµÄ·½Ïò
  graphene_vec3_t world_up;     //ÊÀ½ç×ø±êÏòÉÏµÄ·½Ïò
  graphene_vec3_t camera_right; //ÉãÏñ»úÏòÓÒµÄ·½Ïò

  GList *pushed_buttons;

  /* perspective */
  gfloat fov;
  gfloat aspect;
  gfloat znear;
  gfloat zfar;
  gboolean ortho;
  
  /* user input */
  gdouble cursor_last_x;
  gdouble cursor_last_y;
  
  int pressed_mouse_button;
};

struct _Gst3DCameraClass
{
  GstObjectClass parent_class;
  void (*update_view)          (Gst3DCamera *cam);
  void (*navigation_event)     (Gst3DCamera *cam, GstEvent * event);
};

void gst_3d_camera_update_view (Gst3DCamera * self);
void gst_3d_camera_update_view_mvp (Gst3DCamera * self);
void gst_3d_camera_navigation_event (Gst3DCamera * self, GstEvent * event);

void gst_3d_camera_press_key (Gst3DCamera * self, const gchar * key);
void gst_3d_camera_release_key (Gst3DCamera * self, const gchar * key);
void gst_3d_camera_print_pressed_keys (Gst3DCamera * self);

GType gst_3d_camera_get_type (void);

G_END_DECLS
#endif /* __GST_3D_CAMERA_H__ */
