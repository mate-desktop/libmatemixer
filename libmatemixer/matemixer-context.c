/*
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include "matemixer.h"
#include "matemixer-backend.h"
#include "matemixer-backend-module.h"
#include "matemixer-client-stream.h"
#include "matemixer-context.h"
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-private.h"
#include "matemixer-stream.h"

/**
 * SECTION:matemixer-context
 * @short_description:The main class for interfacing with the library
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerContextPrivate
{
    gboolean                backend_chosen;
    MateMixerState          state;
    MateMixerBackend       *backend;
    MateMixerBackendData    backend_data;
    MateMixerBackendType    backend_type;
    MateMixerBackendModule *module;
};

enum {
    PROP_0,
    PROP_APP_NAME,
    PROP_APP_ID,
    PROP_APP_VERSION,
    PROP_APP_ICON,
    PROP_SERVER_ADDRESS,
    PROP_STATE,
    PROP_DEFAULT_INPUT_STREAM,
    PROP_DEFAULT_OUTPUT_STREAM,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
    DEVICE_ADDED,
    DEVICE_REMOVED,
    STREAM_ADDED,
    STREAM_REMOVED,
    STORED_STREAM_ADDED,
    STORED_STREAM_REMOVED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void mate_mixer_context_class_init   (MateMixerContextClass *klass);

static void mate_mixer_context_get_property (GObject               *object,
                                             guint                  param_id,
                                             GValue                *value,
                                             GParamSpec            *pspec);
static void mate_mixer_context_set_property (GObject               *object,
                                             guint                  param_id,
                                             const GValue          *value,
                                             GParamSpec            *pspec);

static void mate_mixer_context_init         (MateMixerContext      *context);
static void mate_mixer_context_dispose      (GObject               *object);
static void mate_mixer_context_finalize     (GObject               *object);

G_DEFINE_TYPE (MateMixerContext, mate_mixer_context, G_TYPE_OBJECT);

static void     on_backend_state_notify                 (MateMixerBackend *backend,
                                                         GParamSpec       *pspec,
                                                         MateMixerContext *context);

static void     on_backend_device_added                 (MateMixerBackend *backend,
                                                         const gchar      *name,
                                                         MateMixerContext *context);
static void     on_backend_device_removed               (MateMixerBackend *backend,
                                                         const gchar      *name,
                                                         MateMixerContext *context);

static void     on_backend_stream_added                 (MateMixerBackend *backend,
                                                         const gchar      *name,
                                                         MateMixerContext *context);
static void     on_backend_stream_removed               (MateMixerBackend *backend,
                                                         const gchar      *name,
                                                         MateMixerContext *context);

static void     on_backend_stored_stream_added          (MateMixerBackend *backend,
                                                         const gchar      *name,
                                                         MateMixerContext *context);
static void     on_backend_stored_stream_removed        (MateMixerBackend *backend,
                                                         const gchar      *name,
                                                         MateMixerContext *context);

static void     on_backend_default_input_stream_notify  (MateMixerBackend *backend,
                                                         GParamSpec       *pspec,
                                                         MateMixerContext *context);
static void     on_backend_default_output_stream_notify (MateMixerBackend *backend,
                                                         GParamSpec       *pspec,
                                                         MateMixerContext *context);

static gboolean try_next_backend                        (MateMixerContext *context);

static void     change_state                            (MateMixerContext *context,
                                                         MateMixerState    state);

static void     close_context                           (MateMixerContext *context);

static void
mate_mixer_context_class_init (MateMixerContextClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_context_dispose;
    object_class->finalize     = mate_mixer_context_finalize;
    object_class->get_property = mate_mixer_context_get_property;
    object_class->set_property = mate_mixer_context_set_property;

    /**
     * MateMixerContext:app-name:
     *
     * Localized human readable name of the application.
     */
    properties[PROP_APP_NAME] =
        g_param_spec_string ("app-name",
                             "App name",
                             "Application name",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerContext:app-id:
     *
     * Identifier of the application (e.g. org.example.app).
     */
    properties[PROP_APP_ID] =
        g_param_spec_string ("app-id",
                             "App ID",
                             "Application identifier",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerContext:app-version:
     *
     * Version of the application.
     */
    properties[PROP_APP_VERSION] =
        g_param_spec_string ("app-version",
                             "App version",
                             "Application version",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerContext:app-icon:
     *
     * An XDG icon name for the application.
     */
    properties[PROP_APP_ICON] =
        g_param_spec_string ("app-icon",
                             "App icon",
                             "Application icon name",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerContext:server-address:
     *
     * Address of the sound server to connect to.
     *
     * This feature is only supported in the PulseAudio backend. There is
     * no need to specify an address in order to connect to the local daemon.
     */
    properties[PROP_SERVER_ADDRESS] =
        g_param_spec_string ("server-address",
                             "Server address",
                             "Sound server address",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_STATE] =
        g_param_spec_enum ("state",
                           "State",
                           "Current backend connection state",
                           MATE_MIXER_TYPE_STATE,
                           MATE_MIXER_STATE_IDLE,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DEFAULT_INPUT_STREAM] =
        g_param_spec_object ("default-input-stream",
                             "Default input stream",
                             "Default input stream",
                             MATE_MIXER_TYPE_STREAM,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DEFAULT_OUTPUT_STREAM] =
        g_param_spec_object ("default-output-stream",
                             "Default output stream",
                             "Default output stream",
                             MATE_MIXER_TYPE_STREAM,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    /**
     * MateMixerContext::device-added:
     * @context: a #MateMixerContext
     * @name: name of the added device
     *
     * The signal is emitted each time a device is added to the system.
     */
    signals[DEVICE_ADDED] =
        g_signal_new ("device-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerContextClass, device_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerContext::device-removed:
     * @context: a #MateMixerContext
     * @name: name of the removed device
     *
     * The signal is emitted each time a device is removed from the system.
     */
    signals[DEVICE_REMOVED] =
        g_signal_new ("device-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerContextClass, device_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerContext::stream-added:
     * @context: a #MateMixerContext
     * @name: name of the added stream
     *
     * The signal is emitted each time a stream is created.
     */
    signals[STREAM_ADDED] =
        g_signal_new ("stream-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerContextClass, stream_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerContext::stream-removed:
     * @context: a #MateMixerContext
     * @name: name of the removed stream
     *
     * The signal is emitted each time a stream is removed.
     */
    signals[STREAM_REMOVED] =
        g_signal_new ("stream-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerContextClass, stream_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerContext::stored-stream-added:
     * @context: a #MateMixerContext
     * @name: name of the added stored stream
     *
     * The signal is emitted each time a stored stream is created.
     */
    signals[STORED_STREAM_ADDED] =
        g_signal_new ("stored-control-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerContextClass, stored_stream_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerContext::stream-removed:
     * @context: a #MateMixerContext
     * @name: name of the removed stream
     *
     * The signal is emitted each time a stream is removed.
     */
    signals[STORED_STREAM_REMOVED] =
        g_signal_new ("stored-stream-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerContextClass, stored_stream_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    g_type_class_add_private (object_class, sizeof (MateMixerContextPrivate));
}

static void
mate_mixer_context_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    MateMixerContext *context;

    context = MATE_MIXER_CONTEXT (object);

    switch (param_id) {
    case PROP_APP_NAME:
        g_value_set_string (value, context->priv->backend_data.app_name);
        break;
    case PROP_APP_ID:
        g_value_set_string (value, context->priv->backend_data.app_id);
        break;
    case PROP_APP_VERSION:
        g_value_set_string (value, context->priv->backend_data.app_version);
        break;
    case PROP_APP_ICON:
        g_value_set_string (value, context->priv->backend_data.app_icon);
        break;
    case PROP_SERVER_ADDRESS:
        g_value_set_string (value, context->priv->backend_data.server_address);
        break;
    case PROP_STATE:
        g_value_set_enum (value, context->priv->state);
        break;
    case PROP_DEFAULT_INPUT_STREAM:
        g_value_set_object (value, mate_mixer_context_get_default_input_stream (context));
        break;
    case PROP_DEFAULT_OUTPUT_STREAM:
        g_value_set_object (value, mate_mixer_context_get_default_output_stream (context));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_context_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    MateMixerContext *context;

    context = MATE_MIXER_CONTEXT (object);

    switch (param_id) {
    case PROP_APP_NAME:
        mate_mixer_context_set_app_name (context, g_value_get_string (value));
        break;
    case PROP_APP_ID:
        mate_mixer_context_set_app_id (context, g_value_get_string (value));
        break;
    case PROP_APP_VERSION:
        mate_mixer_context_set_app_version (context, g_value_get_string (value));
        break;
    case PROP_APP_ICON:
        mate_mixer_context_set_app_icon (context, g_value_get_string (value));
        break;
    case PROP_SERVER_ADDRESS:
        mate_mixer_context_set_server_address (context, g_value_get_string (value));
        break;
    case PROP_DEFAULT_INPUT_STREAM:
        mate_mixer_context_set_default_input_stream (context, g_value_get_object (value));
        break;
    case PROP_DEFAULT_OUTPUT_STREAM:
        mate_mixer_context_set_default_output_stream (context, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_context_init (MateMixerContext *context)
{
    context->priv = G_TYPE_INSTANCE_GET_PRIVATE (context,
                                                 MATE_MIXER_TYPE_CONTEXT,
                                                 MateMixerContextPrivate);
}

static void
mate_mixer_context_dispose (GObject *object)
{
    close_context (MATE_MIXER_CONTEXT (object));

    G_OBJECT_CLASS (mate_mixer_context_parent_class)->dispose (object);
}

static void
mate_mixer_context_finalize (GObject *object)
{
    MateMixerContext *context;

    context = MATE_MIXER_CONTEXT (object);

    g_free (context->priv->backend_data.app_name);
    g_free (context->priv->backend_data.app_id);
    g_free (context->priv->backend_data.app_version);
    g_free (context->priv->backend_data.app_icon);
    g_free (context->priv->backend_data.server_address);

    G_OBJECT_CLASS (mate_mixer_context_parent_class)->finalize (object);
}

/**
 * mate_mixer_context_new:
 *
 * Creates a new #MateMixerContext instance.
 *
 * Returns: a new #MateMixerContext instance or %NULL if the library has not
 * been initialized using mate_mixer_init().
 */
MateMixerContext *
mate_mixer_context_new (void)
{
    if (mate_mixer_is_initialized () == FALSE) {
        g_critical ("The library has not been initialized");
        return NULL;
    }

    return g_object_new (MATE_MIXER_TYPE_CONTEXT, NULL);
}

/**
 * mate_mixer_context_set_backend_type:
 * @context: a #MateMixerContext
 * @backend_type: the sound system backend to use
 *
 * Makes the #MateMixerContext use the given #MateMixerBackendType.
 *
 * By default the backend type is determined automatically. This function can
 * be used before mate_mixer_context_open() to alter this behavior and make the
 * @context use the given backend.
 *
 * This function will fail if support for the backend is not installed in
 * the system or if the current state is either %MATE_MIXER_STATE_CONNECTING or
 * %MATE_MIXER_STATE_READY.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_context_set_backend_type (MateMixerContext     *context,
                                     MateMixerBackendType  backend_type)
{
    MateMixerBackendModule     *module;
    const GList                *modules;
    const MateMixerBackendInfo *info;

    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);

    if (context->priv->state == MATE_MIXER_STATE_CONNECTING ||
        context->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    modules = _mate_mixer_get_modules ();

    while (modules != NULL) {
        module = MATE_MIXER_BACKEND_MODULE (modules->data);
        info   = mate_mixer_backend_module_get_info (module);

        if (info->backend_type == backend_type) {
            context->priv->backend_type = backend_type;
            return TRUE;
        }
        modules = modules->next;
    }
    return FALSE;
}

/**
 * mate_mixer_context_set_app_name:
 * @context: a #MateMixerContext
 * @app_name: the name of your application, or %NULL to unset
 *
 * Sets the name of the application. This feature is only supported in the
 * PulseAudio backend.
 *
 * This function will fail if the current state is either
 * %MATE_MIXER_STATE_CONNECTING or %MATE_MIXER_STATE_READY, therefore you should
 * use it before opening a connection to the sound system.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_context_set_app_name (MateMixerContext *context, const gchar *app_name)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);

    if (context->priv->state == MATE_MIXER_STATE_CONNECTING ||
        context->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    if (g_strcmp0 (context->priv->backend_data.app_name, app_name) != 0) {
        g_free (context->priv->backend_data.app_name);

        context->priv->backend_data.app_name = g_strdup (app_name);

        g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_APP_NAME]);
    }
    return TRUE;
}

/**
 * mate_mixer_context_set_app_id:
 * @context: a #MateMixerContext
 * @app_id: the identifier of your application, or %NULL to unset
 *
 * Sets the identifier of the application (e.g. org.example.app). This feature
 * is only supported in the PulseAudio backend.
 *
 * This function will fail if the current state is either
 * %MATE_MIXER_STATE_CONNECTING or %MATE_MIXER_STATE_READY, therefore you should
 * use it before opening a connection to the sound system.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_context_set_app_id (MateMixerContext *context, const gchar *app_id)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);

    if (context->priv->state == MATE_MIXER_STATE_CONNECTING ||
        context->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    if (g_strcmp0 (context->priv->backend_data.app_id, app_id) != 0) {
        g_free (context->priv->backend_data.app_id);

        context->priv->backend_data.app_id = g_strdup (app_id);

        g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_APP_ID]);
    }
    return TRUE;
}

/**
 * mate_mixer_context_set_app_version:
 * @context: a #MateMixerContext
 * @app_version: the version of your application, or %NULL to unset
 *
 * Sets the version of the application. This feature is only supported in the
 * PulseAudio backend.
 *
 * This function will fail if the current state is either
 * %MATE_MIXER_STATE_CONNECTING or %MATE_MIXER_STATE_READY, therefore you should
 * use it before opening a connection to the sound system.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_context_set_app_version (MateMixerContext *context, const gchar *app_version)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);

    if (context->priv->state == MATE_MIXER_STATE_CONNECTING ||
        context->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    if (g_strcmp0 (context->priv->backend_data.app_version, app_version) != 0) {
        g_free (context->priv->backend_data.app_version);

        context->priv->backend_data.app_version = g_strdup (app_version);

        g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_APP_VERSION]);
    }
    return TRUE;
}

/**
 * mate_mixer_context_set_app_icon:
 * @context: a #MateMixerContext
 * @app_icon: the XDG icon name of your application, or %NULL to unset
 *
 * Sets the XDG icon name of the application. This feature is only supported in
 * the PulseAudio backend.
 *
 * This function will fail if the current state is either
 * %MATE_MIXER_STATE_CONNECTING or %MATE_MIXER_STATE_READY, therefore you should
 * use it before opening a connection to the sound system.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_context_set_app_icon (MateMixerContext *context, const gchar *app_icon)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);

    if (context->priv->state == MATE_MIXER_STATE_CONNECTING ||
        context->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    if (g_strcmp0 (context->priv->backend_data.app_icon, app_icon) != 0) {
        g_free (context->priv->backend_data.app_icon);

        context->priv->backend_data.app_icon = g_strdup (app_icon);

        g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_APP_ICON]);
    }
    return TRUE;
}

/**
 * mate_mixer_context_set_server_address:
 * @context: a #MateMixerContext
 * @address: the address of the sound server to connect to or %NULL
 *
 * Sets the address of the sound server. This feature is only supported in the
 * PulseAudio backend. If the address is not set, the default PulseAudio sound
 * server will be used, which is normally the local daemon.
 *
 * This function will fail if the current state is either
 * %MATE_MIXER_STATE_CONNECTING or %MATE_MIXER_STATE_READY, therefore you should
 * use it before opening a connection to the sound system.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_context_set_server_address (MateMixerContext *context, const gchar *address)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);

    if (context->priv->state == MATE_MIXER_STATE_CONNECTING ||
        context->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    if (g_strcmp0 (context->priv->backend_data.server_address, address) != 0) {
        g_free (context->priv->backend_data.server_address);

        context->priv->backend_data.server_address = g_strdup (address);

        g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_SERVER_ADDRESS]);
    }
    return TRUE;
}

/**
 * mate_mixer_context_open:
 * @context: a #MateMixerContext
 *
 * Opens connection to a sound system. Unless the backend type was given
 * beforehand, the library will find a working sound system automatically.
 * If the automatic discovery fails to find a working sound system, it will
 * use the "Null" backend, which provides no functionality.
 *
 * This function can complete the operation either synchronously or
 * asynchronously.
 *
 * In case this function returns %TRUE, you should check the current state of
 * the connection using mate_mixer_context_get_state(). If the current state
 * is %MATE_MIXER_STATE_READY, the connection has been established successfully.
 * Otherwise the state will be set to %MATE_MIXER_STATE_CONNECTING and the
 * result of the operation will be determined asynchronously. You should wait
 * for the state transition by connecting to the notification signal of the
 * #MateMixerContext:state property.
 *
 * In case this function returns %FALSE, it is not possible to use the selected
 * backend and the state will be set to %MATE_MIXER_STATE_FAILED.
 *
 * Returns: %TRUE on success or if the result will be determined asynchronously,
 * or %FALSE on failure.
 */
gboolean
mate_mixer_context_open (MateMixerContext *context)
{
    MateMixerBackendModule     *module = NULL;
    MateMixerState              state;
    const GList                *modules;
    const MateMixerBackendInfo *info = NULL;

    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);

    if (context->priv->state == MATE_MIXER_STATE_CONNECTING ||
        context->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    /* We are going to choose the first backend to try. It will be either the one
     * specified by the application or the one with the highest priority */
    modules = _mate_mixer_get_modules ();

    if (context->priv->backend_type != MATE_MIXER_BACKEND_UNKNOWN) {
        while (modules != NULL) {
            const MateMixerBackendInfo *info;

            module = MATE_MIXER_BACKEND_MODULE (modules->data);
            info   = mate_mixer_backend_module_get_info (module);

            if (info->backend_type == context->priv->backend_type)
                break;

            module = NULL;
            modules = modules->next;
        }
        if (module == NULL) {
            /* The selected backend is not available */
            change_state (control, MATE_MIXER_STATE_FAILED);
            return FALSE;
        }
    } else {
        /* The highest priority module is on the top of the list */
        module = MATE_MIXER_BACKEND_MODULE (modules->data);
    }

    if (info == NULL)
        info = mate_mixer_backend_module_get_info (module);

    context->priv->module  = g_object_ref (module);
    context->priv->backend = g_object_new (info->g_type, NULL);

    mate_mixer_backend_set_data (context->priv->backend, &context->priv->backend_data);

    g_debug ("Trying to open backend %s", info->name);

    /* This transitional state is always present, it will change to MATE_MIXER_STATE_READY
     * or MATE_MIXER_STATE_FAILED either instantly or asynchronously */
    change_state (context, MATE_MIXER_STATE_CONNECTING);

    /* The backend initialization might fail in case it is known right now that
     * the backend is unusable */
    if (mate_mixer_backend_open (context->priv->backend) == FALSE) {
        if (context->priv->backend_type == MATE_MIXER_BACKEND_UNKNOWN) {
            /* User didn't request a specific backend, so try another one */
            return try_next_backend (context);
        }

        /* User requested a specific backend and it failed */
        close_context (context);
        change_state (context, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    state = mate_mixer_backend_get_state (context->priv->backend);

    if (G_UNLIKELY (state != MATE_MIXER_STATE_READY &&
                    state != MATE_MIXER_STATE_CONNECTING)) {
        /* This would be a backend bug */
        g_warn_if_reached ();

        if (context->priv->backend_type == MATE_MIXER_BACKEND_UNKNOWN)
            return try_next_backend (context);

        close_context (context);
        change_state (context, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    g_signal_connect (G_OBJECT (context->priv->backend),
                      "notify::state",
                      G_CALLBACK (on_backend_state_notify),
                      context);

    change_state (context, state);
    return TRUE;
}

/**
 * mate_mixer_context_close:
 * @context: a #MateMixerContext
 *
 * Closes connection to the currently used sound system. The state will be
 * set to %MATE_MIXER_STATE_IDLE.
 */
void
mate_mixer_context_close (MateMixerContext *context)
{
    g_return_if_fail (MATE_MIXER_IS_CONTEXT (context));

    close_context (context);
    change_state (context, MATE_MIXER_STATE_IDLE);
}

/**
 * mate_mixer_context_get_state:
 * @context: a #MateMixerContext
 *
 * Gets the current backend connection state of the #MateMixerContext.
 *
 * Returns: The current connection state.
 */
MateMixerState
mate_mixer_context_get_state (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), MATE_MIXER_STATE_UNKNOWN);

    return context->priv->state;
}

/**
 * mate_mixer_context_get_device:
 * @context: a #MateMixerContext
 * @name: a device name
 *
 * Gets the device with the given name.
 *
 * Returns: a #MateMixerDevice or %NULL if there is no such device.
 */
MateMixerDevice *
mate_mixer_context_get_device (MateMixerContext *context, const gchar *name)
{
    GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = (GList *) mate_mixer_context_list_devices (context);
    while (list != NULL) {
        MateMixerDevice *device = MATE_MIXER_DEVICE (list->data);

        if (strcmp (name, mate_mixer_device_get_name (device)) == 0)
            return device;

        list = list->next;
    }
    return NULL;
}

/**
 * mate_mixer_context_get_stream:
 * @context: a #MateMixerContext
 * @name: a stream name
 *
 * Gets the stream with the given name.
 *
 * Returns: a #MateMixerStream or %NULL if there is no such stream.
 */
MateMixerStream *
mate_mixer_context_get_stream (MateMixerContext *context, const gchar *name)
{
    GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = (GList *) mate_mixer_context_list_streams (context);
    while (list != NULL) {
        MateMixerStream *stream = MATE_MIXER_STREAM (list->data);

        if (strcmp (name, mate_mixer_stream_get_name (stream)) == 0)
            return stream;

        list = list->next;
    }
    return NULL;
}

/**
 * mate_mixer_context_get_stored_stream:
 * @context: a #MateMixerContext
 * @name: a stream name
 *
 * Gets the stream with the given name.
 *
 * Returns: a #MateMixerStream or %NULL if there is no such stream.
 */
MateMixerStream *
mate_mixer_context_get_stored_stream (MateMixerContext *context, const gchar *name)
{
    GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = (GList *) mate_mixer_context_list_stored_streams (context);
    while (list != NULL) {
        MateMixerStream *stream = MATE_MIXER_STREAM (list->data);

        if (strcmp (name, mate_mixer_stream_get_name (stream)) == 0)
            return stream;

        list = list->next;
    }
    return NULL;
}

/**
 * mate_mixer_context_list_devices:
 * @context: a #MateMixerContext
 *
 * Gets a list of devices. Each list item is a #MateMixerDevice representing a
 * hardware or software sound device in the system.
 *
 * The returned #GList is owned by the #MateMixerContext and may be invalidated
 * at any time.
 *
 * Returns: a #GList of all devices in the system or %NULL if there are none or
 * you are not connected to a sound system.
 */
const GList *
mate_mixer_context_list_devices (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);

    if (context->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    return mate_mixer_backend_list_devices (MATE_MIXER_BACKEND (context->priv->backend));
}

/**
 * mate_mixer_context_list_streams:
 * @context: a #MateMixerContext
 *
 * Gets a list of streams. Each list item is a #MateMixerStream representing an
 * input or output of a sound device.
 *
 * The returned #GList is owned by the #MateMixerContext and may be invalidated
 * at any time.
 *
 * Returns: a #GList of all streams in the system or %NULL if there are none or
 * you are not connected to a sound system.
 */
const GList *
mate_mixer_context_list_streams (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);

    if (context->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    return mate_mixer_backend_list_streams (MATE_MIXER_BACKEND (context->priv->backend));
}

/**
 * mate_mixer_context_list_stored_streams:
 * @context: a #MateMixerContext
 *
 */
const GList *
mate_mixer_context_list_stored_streams (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);

    if (context->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    return mate_mixer_backend_list_stored_streams (MATE_MIXER_BACKEND (context->priv->backend));
}

/**
 * mate_mixer_context_get_default_input_stream:
 * @context: a #MateMixerContext
 *
 * Gets the default input stream. The returned stream is where sound input is
 * directed to by default.
 *
 * Returns: a #MateMixerStream or %NULL if there are no input streams in
 * the system.
 */
MateMixerStream *
mate_mixer_context_get_default_input_stream (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);

    if (context->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    return mate_mixer_backend_get_default_input_stream (context->priv->backend);
}

/**
 * mate_mixer_context_set_default_input_stream:
 * @context: a #MateMixerContext
 * @stream: a #MateMixerStream to set as the default input stream
 *
 * Changes the default input stream in the system. The @stream must be an
 * input non-client stream.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_context_set_default_input_stream (MateMixerContext *context,
                                             MateMixerStream  *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (context->priv->state != MATE_MIXER_STATE_READY)
        return FALSE;

    if (MATE_MIXER_IS_CLIENT_STREAM (stream)) {
        g_warning ("Unable to set client stream as the default input stream");
        return FALSE;
    }
    if (!(mate_mixer_stream_get_flags (stream) & MATE_MIXER_STREAM_INPUT)) {
        g_warning ("Unable to set non-input stream as the default input stream");
        return FALSE;
    }

    return mate_mixer_backend_set_default_input_stream (context->priv->backend, stream);
}

/**
 * mate_mixer_context_get_default_output_stream:
 * @context: a #MateMixerContext
 *
 * Gets the default output stream. The returned stream is where sound output is
 * directed to by default.
 *
 * Returns: a #MateMixerStream or %NULL if there are no output streams in
 * the system.
 */
MateMixerStream *
mate_mixer_context_get_default_output_stream (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);

    if (context->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    return mate_mixer_backend_get_default_output_stream (context->priv->backend);
}

/**
 * mate_mixer_context_set_default_output_stream:
 * @context: a #MateMixerContext
 * @stream: a #MateMixerStream to set as the default output stream
 *
 * Changes the default output stream in the system. The @stream must be an
 * output non-client stream.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_context_set_default_output_stream (MateMixerContext *context,
                                              MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (context->priv->state != MATE_MIXER_STATE_READY)
        return FALSE;

    if (MATE_MIXER_IS_CLIENT_STREAM (stream)) {
        g_warning ("Unable to set client stream as the default output stream");
        return FALSE;
    }
    if (!(mate_mixer_stream_get_flags (stream) & MATE_MIXER_STREAM_OUTPUT)) {
        g_warning ("Unable to set non-output stream as the default output stream");
        return FALSE;
    }

    return mate_mixer_backend_set_default_output_stream (context->priv->backend, stream);
}

/**
 * mate_mixer_context_get_backend_name:
 * @context: a #MateMixerContext
 *
 * Gets the name of the currently used backend. This function will not
 * work until connected to a sound system.
 *
 * Returns: the name or %NULL on error.
 */
const gchar *
mate_mixer_context_get_backend_name (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), NULL);

    if (context->priv->backend_chosen == FALSE)
        return NULL;

    return mate_mixer_backend_module_get_info (context->priv->module)->name;
}

/**
 * mate_mixer_context_get_backend_type:
 * @context: a #MateMixerContext
 *
 * Gets the type of the currently used backend. This function will not
 * work until connected to a sound system.
 *
 * Returns: the backend type or %MATE_MIXER_BACKEND_UNKNOWN on error.
 */
MateMixerBackendType
mate_mixer_context_get_backend_type (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), MATE_MIXER_BACKEND_UNKNOWN);

    if (context->priv->backend_chosen == FALSE)
        return MATE_MIXER_BACKEND_UNKNOWN;

    return mate_mixer_backend_module_get_info (context->priv->module)->backend_type;
}

/**
 * mate_mixer_context_get_backend_flags:
 * @context: a #MateMixerContext
 */
MateMixerBackendFlags
mate_mixer_context_get_backend_flags (MateMixerContext *context)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTEXT (context), MATE_MIXER_BACKEND_NO_FLAGS);

    return mate_mixer_backend_get_flags (context->priv->backend);
}

static void
on_backend_state_notify (MateMixerBackend *backend,
                         GParamSpec       *pspec,
                         MateMixerContext *context)
{
    MateMixerState state = mate_mixer_backend_get_state (backend);

    switch (state) {
    case MATE_MIXER_STATE_CONNECTING:
        g_debug ("Backend %s changed state to CONNECTING",
                 mate_mixer_backend_module_get_info (context->priv->module)->name);

        change_state (context, state);
        break;

    case MATE_MIXER_STATE_READY:
        g_debug ("Backend %s changed state to READY",
                 mate_mixer_backend_module_get_info (context->priv->module)->name);

        change_state (context, state);
        break;

    case MATE_MIXER_STATE_FAILED:
        g_debug ("Backend %s changed state to FAILED",
                 mate_mixer_backend_module_get_info (context->priv->module)->name);

        if (context->priv->backend_type == MATE_MIXER_BACKEND_UNKNOWN) {
            /* User didn't request a specific backend, so try another one */
            try_next_backend (context);
        } else {
            /* User requested a specific backend and it failed */
            close_context (context);
            change_state (context, state);
        }
        break;

    default:
        break;
    }
}

static void
on_backend_device_added (MateMixerBackend *backend,
                         const gchar      *name,
                         MateMixerContext *context)
{
    g_signal_emit (G_OBJECT (context),
                   signals[DEVICE_ADDED],
                   0,
                   name);
}

static void
on_backend_device_removed (MateMixerBackend *backend,
                           const gchar      *name,
                           MateMixerContext *context)
{
    g_signal_emit (G_OBJECT (context),
                   signals[DEVICE_REMOVED],
                   0,
                   name);
}

static void
on_backend_stream_added (MateMixerBackend *backend,
                         const gchar      *name,
                         MateMixerContext *context)
{
    g_signal_emit (G_OBJECT (context),
                   signals[STREAM_ADDED],
                   0,
                   name);
}

static void
on_backend_stream_removed (MateMixerBackend *backend,
                           const gchar      *name,
                           MateMixerContext *context)
{
    g_signal_emit (G_OBJECT (context),
                   signals[STREAM_REMOVED],
                   0,
                   name);
}

static void
on_backend_stored_stream_added (MateMixerBackend *backend,
                                const gchar      *name,
                                MateMixerContext *context)
{
    g_signal_emit (G_OBJECT (context),
                   signals[STORED_STREAM_ADDED],
                   0,
                   name);
}

static void
on_backend_stored_stream_removed (MateMixerBackend *backend,
                                  const gchar      *name,
                                  MateMixerContext *context)
{
    g_signal_emit (G_OBJECT (context),
                   signals[STORED_STREAM_REMOVED],
                   0,
                   name);
}

static void
on_backend_default_input_stream_notify (MateMixerBackend *backend,
                                        GParamSpec       *pspec,
                                        MateMixerContext *context)
{
    g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_DEFAULT_INPUT_STREAM]);
}

static void
on_backend_default_output_stream_notify (MateMixerBackend *backend,
                                         GParamSpec       *pspec,
                                         MateMixerContext *context)
{
    g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_DEFAULT_OUTPUT_STREAM]);
}

static gboolean
try_next_backend (MateMixerContext *context)
{
    MateMixerBackendModule     *module = NULL;
    MateMixerState              state;
    const GList                *modules;
    const MateMixerBackendInfo *info = NULL;

    modules = _mate_mixer_get_modules ();

    while (modules != NULL) {
        if (context->priv->module == modules->data) {
            /* Found the last tested backend, try to use the next one with a lower
             * priority unless we have reached the end of the list */
            if (modules->next != NULL)
                module = MATE_MIXER_BACKEND_MODULE (modules->next->data);
            break;
        }
        modules = modules->next;
    }
    close_context (context);

    if (module == NULL) {
        /* We have tried all the modules and all of them failed */
        change_state (context, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    info = mate_mixer_backend_module_get_info (module);

    control->priv->module  = g_object_ref (module);
    control->priv->backend = g_object_new (info->g_type, NULL);

    mate_mixer_backend_set_data (context->priv->backend, &context->priv->backend_data);

    g_debug ("Trying to open backend %s", info->name);

    /* Try to open this backend and in case of failure keep trying until we find
     * one that works or reach the end of the list */
    if (mate_mixer_backend_open (context->priv->backend) == FALSE)
        return try_next_backend (context);

    state = mate_mixer_backend_get_state (context->priv->backend);

    if (G_UNLIKELY (state != MATE_MIXER_STATE_READY &&
                    state != MATE_MIXER_STATE_CONNECTING)) {
        /* This would be a backend bug */
        g_warn_if_reached ();

        return try_next_backend (context);
    }

    g_signal_connect (G_OBJECT (context->priv->backend),
                      "notify::state",
                      G_CALLBACK (on_backend_state_notify),
                      context);

    change_state (context, state);
    return TRUE;
}

static void
change_state (MateMixerContext *context, MateMixerState state)
{
    if (context->priv->state == state)
        return;

    context->priv->state = state;

    if (state == MATE_MIXER_STATE_READY && context->priv->backend_chosen == FALSE) {
        /* It is safe to connect to the backend signals after reaching the READY
         * state, because the app is not allowed to query any data before that state;
         * therefore we won't end up in an inconsistent state by caching a list and
         * then missing a notification about a change in the list */
        g_signal_connect (G_OBJECT (context->priv->backend),
                          "device-added",
                          G_CALLBACK (on_backend_device_added),
                          context);
        g_signal_connect (G_OBJECT (context->priv->backend),
                          "device-removed",
                          G_CALLBACK (on_backend_device_removed),
                          context);
        g_signal_connect (G_OBJECT (context->priv->backend),
                          "stream-added",
                          G_CALLBACK (on_backend_stream_added),
                          context);
        g_signal_connect (G_OBJECT (context->priv->backend),
                          "stream-removed",
                          G_CALLBACK (on_backend_stream_removed),
                          context);
        g_signal_connect (G_OBJECT (context->priv->backend),
                          "stored-stream-added",
                          G_CALLBACK (on_backend_stored_stream_added),
                          context);
        g_signal_connect (G_OBJECT (context->priv->backend),
                          "stored-stream-removed",
                          G_CALLBACK (on_backend_stored_stream_removed),
                          context);

        g_signal_connect (G_OBJECT (context->priv->backend),
                          "notify::default-input",
                          G_CALLBACK (on_backend_default_input_stream_notify),
                          context);
        g_signal_connect (G_OBJECT (context->priv->backend),
                          "notify::default-output",
                          G_CALLBACK (on_backend_default_output_stream_notify),
                          context);

        context->priv->backend_chosen = TRUE;
    }

    g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_STATE]);
}

static void
close_context (MateMixerContext *context)
{
    if (context->priv->backend != NULL) {
        g_signal_handlers_disconnect_by_data (G_OBJECT (context->priv->backend),
                                              context);

        mate_mixer_backend_close (context->priv->backend);
        g_clear_object (&context->priv->backend);
    }

    g_clear_object (&context->priv->module);

    context->priv->backend_chosen = FALSE;
}
