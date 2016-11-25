/*
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "gstmfxtaskaggregator.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/**
* GstMfxTaskAggregator:
*
* An MFX aggregator wrapper.
*/
struct _GstMfxTaskAggregator
{
  /*< private > */
  GstMfxMiniObject parent_instance;

  GstMfxDisplay *display;
  GList *cache;
  GstMfxTask *current_task;
  mfxSession parent_session;
};

static void
gst_mfx_task_aggregator_finalize (GstMfxTaskAggregator * aggregator)
{
  g_list_free_full(aggregator->cache, gst_mfx_task_unref);
  gst_mfx_display_unref (aggregator->display);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_task_aggregator_class (void)
{
  static const GstMfxMiniObjectClass GstMfxTaskAggregatorClass = {
    sizeof (GstMfxTaskAggregator),
    (GDestroyNotify) gst_mfx_task_aggregator_finalize
  };
  return &GstMfxTaskAggregatorClass;
}

static gboolean
aggregator_create (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, FALSE);

  aggregator->cache = NULL;
  aggregator->display = gst_mfx_display_new ();
  if (!aggregator->display)
    return FALSE;

  if (!gst_mfx_display_init_vaapi (aggregator->display))
    goto error;

  return TRUE;
error:
  gst_mfx_display_unref (aggregator->display);
  return FALSE;
}

GstMfxTaskAggregator *
gst_mfx_task_aggregator_new (void)
{
  GstMfxTaskAggregator *aggregator;

  aggregator = gst_mfx_mini_object_new0 (gst_mfx_task_aggregator_class ());
  if (!aggregator)
    return NULL;

  if (!aggregator_create (aggregator))
    goto error;

  return aggregator;
error:
  gst_mfx_task_aggregator_unref (aggregator);
  return NULL;
}

GstMfxTaskAggregator *
gst_mfx_task_aggregator_ref (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, NULL);

  return gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (aggregator));
}

void
gst_mfx_task_aggregator_unref (GstMfxTaskAggregator * aggregator)
{
  gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (aggregator));
}

void
gst_mfx_task_aggregator_replace (GstMfxTaskAggregator ** old_aggregator_ptr,
    GstMfxTaskAggregator * new_aggregator)
{
  g_return_if_fail (old_aggregator_ptr != NULL);

  gst_mfx_mini_object_replace ((GstMfxMiniObject **) old_aggregator_ptr,
      GST_MFX_MINI_OBJECT (new_aggregator));
}

GstMfxDisplay *
gst_mfx_task_aggregator_get_display (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, 0);

  return gst_mfx_display_ref (aggregator->display);
}

mfxSession
gst_mfx_task_aggregator_create_session (GstMfxTaskAggregator * aggregator)
{
  mfxIMPL impl;
  mfxStatus sts;
  mfxSession session;
  const char *desc;

  mfxInitParam init_params;

  memset (&init_params, 0, sizeof (init_params));

  //init_params.GPUCopy = MFX_GPUCOPY_ON;
  init_params.Implementation = MFX_IMPL_AUTO_ANY;
  init_params.Version.Major = 1;
  init_params.Version.Minor = 15;

  sts = MFXInitEx (init_params, &session);
  if (sts < 0) {
    GST_ERROR ("Error initializing internal MFX session");
    return NULL;
  }

  sts = MFXQueryIMPL (session, &impl);

  switch (MFX_IMPL_BASETYPE (impl)) {
    case MFX_IMPL_SOFTWARE:
      desc = "software";
      break;
    case MFX_IMPL_HARDWARE:
    case MFX_IMPL_HARDWARE2:
    case MFX_IMPL_HARDWARE3:
    case MFX_IMPL_HARDWARE4:
      desc = "hardware accelerated";
      break;
    default:
      desc = "unknown";
  }

  GST_INFO ("Initialized internal MFX session using %s implementation", desc);

  if (!aggregator->parent_session)
    aggregator->parent_session = session;
  else
    sts = MFXJoinSession (aggregator->parent_session, session);

  return session;
}

GstMfxTask *
gst_mfx_task_aggregator_get_current_task (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, FALSE);

  return gst_mfx_task_ref (aggregator->current_task);
}

gboolean
gst_mfx_task_aggregator_set_current_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task)
{
  g_return_val_if_fail (aggregator != NULL, FALSE);
  g_return_val_if_fail (task != NULL, FALSE);

  aggregator->current_task = task;

  return TRUE;
}

void
gst_mfx_task_aggregator_add_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task)
{
  g_return_if_fail (aggregator != NULL);
  g_return_if_fail (task != NULL);

  aggregator->cache = g_list_prepend (aggregator->cache, gst_mfx_task_ref(task));
}
