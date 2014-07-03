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
#include "matemixer-control.h"
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-private.h"
#include "matemixer-stream.h"

/**
 * SECTION:matemixer-control
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerControlPrivate
{
    GList                  *devices;
    GList                  *streams;
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
    PROP_DEFAULT_INPUT,
    PROP_DEFAULT_OUTPUT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
    DEVICE_ADDED,
    DEVICE_CHANGED,
    DEVICE_REMOVED,
    STREAM_ADDED,
    STREAM_CHANGED,
    STREAM_REMOVED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void mate_mixer_control_class_init (MateMixerControlClass *klass);

static void mate_mixer_control_get_property (GObject      *object,
                                             guint         param_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);
static void mate_mixer_control_set_property (GObject      *object,
                                             guint         param_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static void mate_mixer_control_init     (MateMixerControl *control);
static void mate_mixer_control_dispose  (GObject          *object);
static void mate_mixer_control_finalize (GObject          *object);

G_DEFINE_TYPE (MateMixerControl, mate_mixer_control, G_TYPE_OBJECT);

static void     control_state_changed_cb  (MateMixerBackend *backend,
                                           GParamSpec       *pspec,
                                           MateMixerControl *control);

static void     control_device_added_cb   (MateMixerBackend *backend,
                                           const gchar      *name,
                                           MateMixerControl *control);
static void     control_device_changed_cb (MateMixerBackend *backend,
                                           const gchar      *name,
                                           MateMixerControl *control);
static void     control_device_removed_cb (MateMixerBackend *backend,
                                           const gchar      *name,
                                           MateMixerControl *control);

static void     control_stream_added_cb   (MateMixerBackend *backend,
                                           const gchar      *name,
                                           MateMixerControl *control);
static void     control_stream_changed_cb (MateMixerBackend *backend,
                                           const gchar      *name,
                                           MateMixerControl *control);
static void     control_stream_removed_cb (MateMixerBackend *backend,
                                           const gchar      *name,
                                           MateMixerControl *control);

static gboolean control_try_next_backend  (MateMixerControl *control);

static void     control_change_state      (MateMixerControl *control,
                                           MateMixerState    state);

static void     control_close             (MateMixerControl *control);

static void     control_free_backend      (MateMixerControl *control);
static void     control_free_devices      (MateMixerControl *control);
static void     control_free_streams      (MateMixerControl *control);

static void
mate_mixer_control_class_init (MateMixerControlClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_control_dispose;
    object_class->finalize     = mate_mixer_control_finalize;
    object_class->get_property = mate_mixer_control_get_property;
    object_class->set_property = mate_mixer_control_set_property;

    /**
     * MateMixerControl:app-name:
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
     * MateMixerControl:app-id:
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
     * MateMixerControl:app-version:
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
     * MateMixerControl:app-icon:
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
     * MateMixerControl:server-address:
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

    properties[PROP_DEFAULT_INPUT] =
        g_param_spec_object ("default-input",
                             "Default input",
                             "Default input stream",
                             MATE_MIXER_TYPE_STREAM,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DEFAULT_OUTPUT] =
        g_param_spec_object ("default-output",
                             "Default output",
                             "Default output stream",
                             MATE_MIXER_TYPE_STREAM,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    /**
     * MateMixerControl::device-added:
     * @control: a #MateMixerControl
     * @name: name of the added device
     *
     * The signal is emitted each time a device is added to the system.
     */
    signals[DEVICE_ADDED] =
        g_signal_new ("device-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerControlClass, device_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerControl::device-changed:
     * @control: a #MateMixerControl
     * @name: name of the changed device
     *
     * The signal is emitted each time a change occurs on one of the known
     * devices.
     */
    signals[DEVICE_CHANGED] =
        g_signal_new ("device-changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerControlClass, device_changed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerControl::device-removed:
     * @control: a #MateMixerControl
     * @name: name of the removed device
     *
     * The signal is emitted each time a device is removed from the system.
     */
    signals[DEVICE_REMOVED] =
        g_signal_new ("device-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerControlClass, device_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerControl::stream-added:
     * @control: a #MateMixerControl
     * @name: name of the added stream
     *
     * The signal is emitted each time a stream is added to the system.
     */
    signals[STREAM_ADDED] =
        g_signal_new ("stream-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerControlClass, stream_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerControl::stream-changed:
     * @control: a #MateMixerControl
     * @name: name of the changed stream
     *
     * The signal is emitted each time a change occurs on one of the known
     * streams.
     */
    signals[STREAM_CHANGED] =
        g_signal_new ("stream-changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerControlClass, stream_changed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerControl::stream-removed:
     * @control: a #MateMixerControl
     * @name: name of the removed stream
     *
     * The signal is emitted each time a stream is removed from the system.
     */
    signals[STREAM_REMOVED] =
        g_signal_new ("stream-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerControlClass, stream_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    g_type_class_add_private (object_class, sizeof (MateMixerControlPrivate));
}

static void
mate_mixer_control_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    MateMixerControl *control;

    control = MATE_MIXER_CONTROL (object);

    switch (param_id) {
    case PROP_APP_NAME:
        g_value_set_string (value, control->priv->backend_data.app_name);
        break;
    case PROP_APP_ID:
        g_value_set_string (value, control->priv->backend_data.app_id);
        break;
    case PROP_APP_VERSION:
        g_value_set_string (value, control->priv->backend_data.app_version);
        break;
    case PROP_APP_ICON:
        g_value_set_string (value, control->priv->backend_data.app_icon);
        break;
    case PROP_SERVER_ADDRESS:
        g_value_set_string (value, control->priv->backend_data.server_address);
        break;
    case PROP_STATE:
        g_value_set_enum (value, control->priv->state);
        break;
    case PROP_DEFAULT_INPUT:
        g_value_set_object (value, mate_mixer_control_get_default_input_stream (control));
        break;
    case PROP_DEFAULT_OUTPUT:
        g_value_set_object (value, mate_mixer_control_get_default_output_stream (control));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_control_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    MateMixerControl *control;

    control = MATE_MIXER_CONTROL (object);

    switch (param_id) {
    case PROP_APP_NAME:
        mate_mixer_control_set_app_name (control, g_value_get_string (value));
        break;
    case PROP_APP_ID:
        mate_mixer_control_set_app_id (control, g_value_get_string (value));
        break;
    case PROP_APP_VERSION:
        mate_mixer_control_set_app_version (control, g_value_get_string (value));
        break;
    case PROP_APP_ICON:
        mate_mixer_control_set_app_icon (control, g_value_get_string (value));
        break;
    case PROP_SERVER_ADDRESS:
        mate_mixer_control_set_server_address (control, g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_control_init (MateMixerControl *control)
{
    control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control,
                                                 MATE_MIXER_TYPE_CONTROL,
                                                 MateMixerControlPrivate);
}

static void
mate_mixer_control_dispose (GObject *object)
{
    control_close (MATE_MIXER_CONTROL (object));

    G_OBJECT_CLASS (mate_mixer_control_parent_class)->dispose (object);
}

static void
mate_mixer_control_finalize (GObject *object)
{
    MateMixerControl *control;

    control = MATE_MIXER_CONTROL (object);

    g_free (control->priv->backend_data.app_name);
    g_free (control->priv->backend_data.app_id);
    g_free (control->priv->backend_data.app_version);
    g_free (control->priv->backend_data.app_icon);
    g_free (control->priv->backend_data.server_address);

    G_OBJECT_CLASS (mate_mixer_control_parent_class)->finalize (object);
}

/**
 * mate_mixer_control_new:
 *
 * Creates a new #MateMixerControl instance.
 *
 * Returns: a new #MateMixerControl instance or %NULL if the library has not
 * been initialized using mate_mixer_init().
 */
MateMixerControl *
mate_mixer_control_new (void)
{
    if (!mate_mixer_is_initialized ()) {
        g_critical ("The library has not been initialized");
        return NULL;
    }
    return g_object_new (MATE_MIXER_TYPE_CONTROL, NULL);
}

/**
 * mate_mixer_control_set_backend_type:
 * @control: a #MateMixerControl
 * @backend_type: the sound system backend to use
 *
 * Makes the #MateMixerControl use the given #MateMixerBackendType.
 *
 * By default the backend type is determined automatically. This function can
 * be used before mate_mixer_control_open() to alter this behavior and make the
 * @control use the given backend.
 *
 * This function will fail if support for the backend is not installed in
 * the system or if the current state is either %MATE_MIXER_STATE_CONNECTING or
 * %MATE_MIXER_STATE_READY.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_control_set_backend_type (MateMixerControl     *control,
                                     MateMixerBackendType  backend_type)
{
    MateMixerBackendModule     *module;
    const GList                *modules;
    const MateMixerBackendInfo *info;

    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);

    if (control->priv->state == MATE_MIXER_STATE_CONNECTING ||
        control->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    modules = mate_mixer_get_modules ();
    while (modules) {
        module = MATE_MIXER_BACKEND_MODULE (modules->data);
        info   = mate_mixer_backend_module_get_info (module);

        if (info->backend_type == backend_type) {
            control->priv->backend_type = backend_type;
            return TRUE;
        }
        modules = modules->next;
    }
    return FALSE;
}

/**
 * mate_mixer_control_set_app_name:
 * @control: a #MateMixerControl
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
mate_mixer_control_set_app_name (MateMixerControl *control, const gchar *app_name)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);

    if (control->priv->state == MATE_MIXER_STATE_CONNECTING ||
        control->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    g_free (control->priv->backend_data.app_name);

    control->priv->backend_data.app_name = g_strdup (app_name);

    g_object_notify_by_pspec (G_OBJECT (control), properties[PROP_APP_NAME]);
    return TRUE;
}

/**
 * mate_mixer_control_set_app_id:
 * @control: a #MateMixerControl
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
mate_mixer_control_set_app_id (MateMixerControl *control, const gchar *app_id)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);

    if (control->priv->state == MATE_MIXER_STATE_CONNECTING ||
        control->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    g_free (control->priv->backend_data.app_id);

    control->priv->backend_data.app_id = g_strdup (app_id);

    g_object_notify_by_pspec (G_OBJECT (control), properties[PROP_APP_ID]);
    return TRUE;
}

/**
 * mate_mixer_control_set_app_version:
 * @control: a #MateMixerControl
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
mate_mixer_control_set_app_version (MateMixerControl *control, const gchar *app_version)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);

    if (control->priv->state == MATE_MIXER_STATE_CONNECTING ||
        control->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    g_free (control->priv->backend_data.app_version);

    control->priv->backend_data.app_version = g_strdup (app_version);

    g_object_notify_by_pspec (G_OBJECT (control), properties[PROP_APP_VERSION]);
    return TRUE;
}

/**
 * mate_mixer_control_set_app_icon:
 * @control: a #MateMixerControl
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
mate_mixer_control_set_app_icon (MateMixerControl *control, const gchar *app_icon)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);

    if (control->priv->state == MATE_MIXER_STATE_CONNECTING ||
        control->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    g_free (control->priv->backend_data.app_icon);

    control->priv->backend_data.app_icon = g_strdup (app_icon);

    g_object_notify_by_pspec (G_OBJECT (control), properties[PROP_APP_ICON]);
    return TRUE;
}

/**
 * mate_mixer_control_set_server_address:
 * @control: a #MateMixerControl
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
mate_mixer_control_set_server_address (MateMixerControl *control, const gchar *address)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);

    if (control->priv->state == MATE_MIXER_STATE_CONNECTING ||
        control->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    g_free (control->priv->backend_data.server_address);

    control->priv->backend_data.server_address = g_strdup (address);

    g_object_notify_by_pspec (G_OBJECT (control), properties[PROP_SERVER_ADDRESS]);
    return TRUE;
}

/**
 * mate_mixer_control_open:
 * @control: a #MateMixerControl
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
 * the connection using mate_mixer_control_get_state(). If the current state
 * is %MATE_MIXER_STATE_READY, the connection has been established successfully.
 * Otherwise the state will be set to %MATE_MIXER_STATE_CONNECTING and the
 * result of the operation will be determined asynchronously. You should wait
 * for the state transition by connecting to the notification signal of the
 * #MateMixerControl:state property.
 *
 * In case this function returns %FALSE, it is not possible to use the selected
 * backend and the state will be set to %MATE_MIXER_STATE_FAILED.
 *
 * Returns: %TRUE on success or if the result will be determined asynchronously,
 * or %FALSE on failure.
 */
gboolean
mate_mixer_control_open (MateMixerControl *control)
{
    MateMixerBackendModule     *module = NULL;
    MateMixerState              state;
    const GList                *modules;
    const MateMixerBackendInfo *info = NULL;

    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);

    if (control->priv->state == MATE_MIXER_STATE_CONNECTING ||
        control->priv->state == MATE_MIXER_STATE_READY)
        return FALSE;

    /* We are going to choose the first backend to try. It will be either the one
     * specified by the application or the one with the highest priority */
    modules = mate_mixer_get_modules ();

    if (control->priv->backend_type != MATE_MIXER_BACKEND_UNKNOWN) {
        while (modules) {
            const MateMixerBackendInfo *info;

            module = MATE_MIXER_BACKEND_MODULE (modules->data);
            info   = mate_mixer_backend_module_get_info (module);

            if (info->backend_type == control->priv->backend_type)
                break;

            module = NULL;
            modules = modules->next;
        }
    } else {
        /* The highest priority module is on the top of the list */
        module = MATE_MIXER_BACKEND_MODULE (modules->data);
    }
    if (module == NULL) {
        /* Most likely the selected backend is not installed */
        control_change_state (control, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    if (info == NULL)
        info = mate_mixer_backend_module_get_info (module);

    control->priv->module  = g_object_ref (module);
    control->priv->backend = g_object_new (info->g_type, NULL);

    mate_mixer_backend_set_data (control->priv->backend, &control->priv->backend_data);

    /* This transitional state is always present, it will change to MATE_MIXER_STATE_READY
     * or MATE_MIXER_STATE_FAILED either instantly or asynchronously */
    control_change_state (control, MATE_MIXER_STATE_CONNECTING);

    /* The backend initialization might fail in case it is known right now that
     * the backend is unusable */
    if (!mate_mixer_backend_open (control->priv->backend)) {
        if (control->priv->backend_type == MATE_MIXER_BACKEND_UNKNOWN) {
            /* User didn't request a specific backend, so try another one */
            return control_try_next_backend (control);
        }

        /* User requested a specific backend and it failed */
        control_close (control);
        control_change_state (control, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    state = mate_mixer_backend_get_state (control->priv->backend);

    if (G_UNLIKELY (state != MATE_MIXER_STATE_READY &&
                    state != MATE_MIXER_STATE_CONNECTING)) {
        /* This would be a backend bug */
        g_warn_if_reached ();

        control_close (control);
        control_change_state (control, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    g_signal_connect (control->priv->backend,
                      "notify::state",
                      G_CALLBACK (control_state_changed_cb),
                      control);

    control_change_state (control, state);
    return TRUE;
}

/**
 * mate_mixer_control_close:
 * @control: a #MateMixerControl
 *
 * Closes connection to the currently used sound system. The state will be
 * set to %MATE_MIXER_STATE_IDLE.
 */
void
mate_mixer_control_close (MateMixerControl *control)
{
    g_return_if_fail (MATE_MIXER_IS_CONTROL (control));

    control_close (control);
    control_change_state (control, MATE_MIXER_STATE_IDLE);
}

/**
 * mate_mixer_control_get_state:
 * @control: a #MateMixerControl
 *
 * Gets the current backend connection state of the #MateMixerControl.
 *
 * Returns: The current connection state.
 */
MateMixerState
mate_mixer_control_get_state (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), MATE_MIXER_STATE_UNKNOWN);

    return control->priv->state;
}

/**
 * mate_mixer_control_get_device:
 * @control: a #MateMixerControl
 * @name: a device name
 *
 * Gets the devices with the given name.
 *
 * Returns: a #MateMixerDevice or %NULL if there is no such device.
 */
MateMixerDevice *
mate_mixer_control_get_device (MateMixerControl *control, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_control_list_devices (control);
    while (list) {
        MateMixerDevice *device = MATE_MIXER_DEVICE (list->data);

        if (!strcmp (name, mate_mixer_device_get_name (device)))
            return device;

        list = list->next;
    }
    return NULL;
}

/**
 * mate_mixer_control_get_stream:
 * @control: a #MateMixerControl
 * @name: a stream name
 *
 * Gets the stream with the given name.
 *
 * Returns: a #MateMixerStream or %NULL if there is no such stream.
 */
MateMixerStream *
mate_mixer_control_get_stream (MateMixerControl *control, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_control_list_streams (control);
    while (list) {
        MateMixerStream *stream = MATE_MIXER_STREAM (list->data);

        if (!strcmp (name, mate_mixer_stream_get_name (stream)))
            return stream;

        list = list->next;
    }
    return NULL;
}

/**
 * mate_mixer_control_list_devices:
 * @control: a #MateMixerControl
 *
 * Gets a list of devices. Each list item is a #MateMixerDevice representing a
 * hardware or software sound device in the system.
 *
 * The returned #GList is owned by the #MateMixerControl and may be invalidated
 * at any time.
 *
 * Returns: a #GList of all devices in the system or %NULL if there are none or
 * you are not connected to a sound system.
 */
const GList *
mate_mixer_control_list_devices (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    if (control->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    /* This list is cached here and invalidated when the backend notifies us
     * about a change */
    if (control->priv->devices == NULL)
        control->priv->devices =
            mate_mixer_backend_list_devices (MATE_MIXER_BACKEND (control->priv->backend));

    return (const GList *) control->priv->devices;
}

/**
 * mate_mixer_control_list_streams:
 * @control: a #MateMixerControl
 *
 * Gets a list of streams. Each list item is a #MateMixerStream representing an
 * input or output of a sound device.
 *
 * The returned #GList is owned by the #MateMixerControl and may be invalidated
 * at any time.
 *
 * Returns: a #GList of all streams in the system or %NULL if there are none or
 * you are not connected to a sound system.
 */
const GList *
mate_mixer_control_list_streams (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    if (control->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    /* This list is cached here and invalidated when the backend notifies us
     * about a change */
    if (control->priv->streams == NULL)
        control->priv->streams =
            mate_mixer_backend_list_streams (MATE_MIXER_BACKEND (control->priv->backend));

    return (const GList *) control->priv->streams;
}

/**
 * mate_mixer_control_get_default_input_stream:
 * @control: a #MateMixerControl
 *
 * Gets the default input stream. The returned stream is where sound input is
 * directed to by default.
 *
 * Returns: a #MateMixerStream or %NULL if there are no input streams in
 * the system.
 */
MateMixerStream *
mate_mixer_control_get_default_input_stream (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    if (control->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    return mate_mixer_backend_get_default_input_stream (control->priv->backend);
}

/**
 * mate_mixer_control_set_default_input_stream:
 * @control: a #MateMixerControl
 * @stream: a #MateMixerStream to set as the default input stream
 *
 * Changes the default input stream in the system. The @stream must be an
 * input non-client stream.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_control_set_default_input_stream (MateMixerControl *control,
                                             MateMixerStream  *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (control->priv->state != MATE_MIXER_STATE_READY)
        return FALSE;

    if (MATE_MIXER_IS_CLIENT_STREAM (stream)) {
        g_warning ("Unable to set client stream as the default input stream");
        return FALSE;
    }
    if (!(mate_mixer_stream_get_flags (stream) & MATE_MIXER_STREAM_INPUT)) {
        g_warning ("Unable to set non-input stream as the default input stream");
        return FALSE;
    }

    return mate_mixer_backend_set_default_input_stream (control->priv->backend, stream);
}

/**
 * mate_mixer_control_get_default_output_stream:
 * @control: a #MateMixerControl
 *
 * Gets the default output stream. The returned stream is where sound output is
 * directed to by default.
 *
 * Returns: a #MateMixerStream or %NULL if there are no output streams in
 * the system.
 */
MateMixerStream *
mate_mixer_control_get_default_output_stream (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    if (control->priv->state != MATE_MIXER_STATE_READY)
        return NULL;

    return mate_mixer_backend_get_default_output_stream (control->priv->backend);
}

/**
 * mate_mixer_control_set_default_output_stream:
 * @control: a #MateMixerControl
 * @stream: a #MateMixerStream to set as the default output stream
 *
 * Changes the default output stream in the system. The @stream must be an
 * output non-client stream.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_control_set_default_output_stream (MateMixerControl *control,
                                              MateMixerStream  *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (control->priv->state != MATE_MIXER_STATE_READY)
        return FALSE;

    if (MATE_MIXER_IS_CLIENT_STREAM (stream)) {
        g_warning ("Unable to set client stream as the default output stream");
        return FALSE;
    }
    if (!(mate_mixer_stream_get_flags (stream) & MATE_MIXER_STREAM_OUTPUT)) {
        g_warning ("Unable to set non-output stream as the default output stream");
        return FALSE;
    }

    return mate_mixer_backend_set_default_input_stream (control->priv->backend, stream);
}

/**
 * mate_mixer_control_get_backend_name:
 * @control: a #MateMixerControl
 *
 * Gets the name of the currently used backend. This function will not
 * work until connected to a sound system.
 *
 * Returns: the name or %NULL on error.
 */
const gchar *
mate_mixer_control_get_backend_name (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    if (!control->priv->backend_chosen)
        return NULL;

    return mate_mixer_backend_module_get_info (control->priv->module)->name;
}

/**
 * mate_mixer_control_get_backend_type:
 * @control: a #MateMixerControl
 *
 * Gets the type of the currently used backend. This function will not
 * work until connected to a sound system.
 *
 * Returns: the backend type or %MATE_MIXER_BACKEND_UNKNOWN on error.
 */
MateMixerBackendType
mate_mixer_control_get_backend_type (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), MATE_MIXER_BACKEND_UNKNOWN);

    if (!control->priv->backend_chosen)
        return MATE_MIXER_BACKEND_UNKNOWN;

    return mate_mixer_backend_module_get_info (control->priv->module)->backend_type;
}

static void
control_state_changed_cb (MateMixerBackend *backend,
                          GParamSpec       *pspec,
                          MateMixerControl *control)
{
    MateMixerState state = mate_mixer_backend_get_state (backend);

    switch (state) {
    case MATE_MIXER_STATE_CONNECTING:
        g_debug ("Backend %s changed state to CONNECTING",
                 mate_mixer_backend_module_get_info (control->priv->module)->name);

        if (control->priv->backend_chosen) {
            /* Invalidate cached data when reconnecting */
            control_free_devices (control);
            control_free_devices (control);
        }
        control_change_state (control, state);
        break;

    case MATE_MIXER_STATE_READY:
        g_debug ("Backend %s changed state to READY",
                 mate_mixer_backend_module_get_info (control->priv->module)->name);

        control_change_state (control, state);
        break;

    case MATE_MIXER_STATE_FAILED:
        g_debug ("Backend %s changed state to FAILED",
                 mate_mixer_backend_module_get_info (control->priv->module)->name);

        if (control->priv->backend_type == MATE_MIXER_BACKEND_UNKNOWN) {
            /* User didn't request a specific backend, so try another one */
            control_try_next_backend (control);
        } else {
            /* User requested a specific backend and it failed */
            control_close (control);
            control_change_state (control, state);
        }
        break;

    default:
        break;
    }
}

static void
control_device_added_cb (MateMixerBackend *backend,
                         const gchar      *name,
                         MateMixerControl *control)
{
    control_free_devices (control);

    g_signal_emit (G_OBJECT (control),
                   signals[DEVICE_ADDED],
                   0,
                   name);
}

static void
control_device_changed_cb (MateMixerBackend *backend,
                           const gchar      *name,
                           MateMixerControl *control)
{
    g_signal_emit (G_OBJECT (control),
                   signals[DEVICE_CHANGED],
                   0,
                   name);
}

static void
control_device_removed_cb (MateMixerBackend *backend,
                           const gchar      *name,
                           MateMixerControl *control)
{
    control_free_devices (control);

    g_signal_emit (G_OBJECT (control),
                   signals[DEVICE_REMOVED],
                   0,
                   name);
}

static void
control_stream_added_cb (MateMixerBackend *backend,
                         const gchar      *name,
                         MateMixerControl *control)
{
    control_free_streams (control);

    g_signal_emit (G_OBJECT (control),
                   signals[STREAM_ADDED],
                   0,
                   name);
}

static void
control_stream_changed_cb (MateMixerBackend *backend,
                           const gchar      *name,
                           MateMixerControl *control)
{
    g_signal_emit (G_OBJECT (control),
                   signals[STREAM_CHANGED],
                   0,
                   name);
}

static void
control_stream_removed_cb (MateMixerBackend *backend,
                           const gchar      *name,
                           MateMixerControl *control)
{
    control_free_streams (control);

    g_signal_emit (G_OBJECT (control),
                   signals[STREAM_REMOVED],
                   0,
                   name);
}

static gboolean
control_try_next_backend (MateMixerControl *control)
{
    const GList            *modules;
    MateMixerBackendModule *module = NULL;
    MateMixerState          state;

    modules = mate_mixer_get_modules ();
    while (modules) {
        if (control->priv->module == modules->data) {
            /* Found the last tested backend, try to use the next one with a lower
             * priority unless we have reached the end of the list */
            if (modules->next)
                module = MATE_MIXER_BACKEND_MODULE (modules->next->data);
            break;
        }
        modules = modules->next;
    }
    control_close (control);

    if (module == NULL) {
        /* This shouldn't happen under normal circumstances as the lowest
         * priority module is the "Null" module which never fails to initialize,
         * but in a broken installation this module could be missing */
        control_change_state (control, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    control->priv->module  = g_object_ref (module);
    control->priv->backend =
        g_object_new (mate_mixer_backend_module_get_info (module)->g_type, NULL);

    mate_mixer_backend_set_data (control->priv->backend, &control->priv->backend_data);

    if (!mate_mixer_backend_open (control->priv->backend))
        return control_try_next_backend (control);

    state = mate_mixer_backend_get_state (control->priv->backend);

    if (G_UNLIKELY (state != MATE_MIXER_STATE_READY &&
                    state != MATE_MIXER_STATE_CONNECTING)) {
        /* This would be a backend bug */
        g_warn_if_reached ();

        control_close (control);
        return control_try_next_backend (control);
    }

    g_signal_connect (control->priv->backend,
                      "notify::state",
                      G_CALLBACK (control_state_changed_cb),
                      control);

    control_change_state (control, state);
    return TRUE;
}

static void
control_change_state (MateMixerControl *control, MateMixerState state)
{
    if (control->priv->state == state)
        return;

    control->priv->state = state;

    if (state == MATE_MIXER_STATE_READY && !control->priv->backend_chosen) {
        /* It is safe to connect to the backend signals after reaching the READY
         * state, because the app is not allowed to query any data before that state;
         * therefore we won't end up in an inconsistent state by caching a list and
         * then missing a notification about a change in the list */
        g_signal_connect (control->priv->backend,
                          "device-added",
                          G_CALLBACK (control_device_added_cb),
                          control);
        g_signal_connect (control->priv->backend,
                          "device-changed",
                          G_CALLBACK (control_device_changed_cb),
                          control);
        g_signal_connect (control->priv->backend,
                          "device-removed",
                          G_CALLBACK (control_device_removed_cb),
                          control);
        g_signal_connect (control->priv->backend,
                          "stream-added",
                          G_CALLBACK (control_stream_added_cb),
                          control);
        g_signal_connect (control->priv->backend,
                          "stream-changed",
                          G_CALLBACK (control_stream_changed_cb),
                          control);
        g_signal_connect (control->priv->backend,
                          "stream-removed",
                          G_CALLBACK (control_stream_removed_cb),
                          control);

        control->priv->backend_chosen = TRUE;
    }

    g_object_notify_by_pspec (G_OBJECT (control), properties[PROP_STATE]);
}

static void
control_close (MateMixerControl *control)
{
    control_free_backend (control);
    control_free_devices (control);
    control_free_streams (control);

    g_clear_object (&control->priv->module);
}

static void
control_free_backend (MateMixerControl *control)
{
    if (control->priv->backend == NULL)
        return;

    mate_mixer_backend_close (control->priv->backend);

    g_clear_object (&control->priv->backend);
}

static void
control_free_devices (MateMixerControl *control)
{
    if (control->priv->devices == NULL)
        return;

    g_list_free_full (control->priv->devices, g_object_unref);

    control->priv->devices = NULL;
}

static void
control_free_streams (MateMixerControl *control)
{
    if (control->priv->streams == NULL)
        return;

    g_list_free_full (control->priv->streams, g_object_unref);

    control->priv->streams = NULL;
}
