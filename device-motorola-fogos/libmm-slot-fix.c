#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <limits.h>
#include <gio/gio.h>
#include <libqmi-glib/libqmi-glib.h>

#include <sys/stat.h>
#include <unistd.h>

__attribute__((constructor))
static void mm_fix_init(void) {
    fprintf(stderr, ">>> [libmm-slot-fix] CLEAN SHIM INITIALIZED <<<\n");
}

/* 1. Fix physical slot status */
static void fix_slot_array(GArray *slots) {
    if (!slots) return;
    for (guint i = 0; i < slots->len; i++) {
        QmiPhysicalSlotStatusSlot *slot = &g_array_index(slots, QmiPhysicalSlotStatusSlot, i);
        gboolean has_sim = (slot->physical_card_status == QMI_UIM_PHYSICAL_CARD_STATE_PRESENT) &&
                           (slot->iccid && slot->iccid->len > 0);
        if (!has_sim) {
            fprintf(stderr, "[libmm-slot-fix] Marking slot %u INACTIVE (card_status=%d, iccid_len=%u)\n",
                    i + 1, slot->physical_card_status, slot->iccid ? slot->iccid->len : 0);
            slot->logical_slot = 0;
            slot->physical_slot_status = QMI_UIM_SLOT_STATE_INACTIVE;
        } else {
            fprintf(stderr, "[libmm-slot-fix] Slot %u ACTIVE with SIM (card_status=%d, iccid_len=%u)\n",
                    i + 1, slot->physical_card_status, slot->iccid ? slot->iccid->len : 0);
            slot->logical_slot = 1;
            slot->physical_slot_status = QMI_UIM_SLOT_STATE_ACTIVE;
        }
    }
}

gboolean qmi_message_uim_get_slot_status_output_get_physical_slot_status(
    QmiMessageUimGetSlotStatusOutput *self,
    GArray **value_physical_slot_status,
    GError **error)
{
    static gboolean (*real_func)(QmiMessageUimGetSlotStatusOutput *, GArray **, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_message_uim_get_slot_status_output_get_physical_slot_status");
    if (!real_func) return FALSE;
    gboolean ret = real_func(self, value_physical_slot_status, error);
    if (ret && value_physical_slot_status && *value_physical_slot_status) {
        fix_slot_array(*value_physical_slot_status);
    }
    return ret;
}

gboolean qmi_indication_uim_slot_status_output_get_physical_slot_status(
    QmiIndicationUimSlotStatusOutput *self,
    GArray **value_physical_slot_status,
    GError **error)
{
    static gboolean (*real_func)(QmiIndicationUimSlotStatusOutput *, GArray **, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_indication_uim_slot_status_output_get_physical_slot_status");
    if (!real_func) return FALSE;
    gboolean ret = real_func(self, value_physical_slot_status, error);
    if (ret && value_physical_slot_status && *value_physical_slot_status) {
        fix_slot_array(*value_physical_slot_status);
    }
    return ret;
}

/* 2. Fix card status index if 0xFFFF */
static void fix_cards(guint16 *value_card_status_index_gw_primary,
                      guint16 *value_card_status_index_1x_primary,
                      GArray *cards) {
    if (!cards) return;
    if (value_card_status_index_gw_primary && *value_card_status_index_gw_primary == 0xFFFF && cards->len > 0) {
        fprintf(stderr, "[libmm-slot-fix] Forcing index_gw_primary to 0x0000 (was 0xFFFF)\n");
        *value_card_status_index_gw_primary = 0;
    }
}

gboolean qmi_message_uim_get_card_status_output_get_card_status(
    QmiMessageUimGetCardStatusOutput *self,
    guint16 *value_card_status_index_gw_primary,
    guint16 *value_card_status_index_1x_primary,
    guint16 *value_card_status_index_gw_secondary,
    guint16 *value_card_status_index_1x_secondary,
    GArray **value_card_status_cards,
    GError **error)
{
    static gboolean (*real_func)(QmiMessageUimGetCardStatusOutput *, guint16 *, guint16 *, guint16 *, guint16 *, GArray **, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_message_uim_get_card_status_output_get_card_status");
    if (!real_func) return FALSE;
    gboolean ret = real_func(self, value_card_status_index_gw_primary, value_card_status_index_1x_primary,
                             value_card_status_index_gw_secondary, value_card_status_index_1x_secondary,
                             value_card_status_cards, error);
    if (ret && value_card_status_cards && *value_card_status_cards) {
        fix_cards(value_card_status_index_gw_primary, value_card_status_index_1x_primary, *value_card_status_cards);
    }
    return ret;
}

gboolean qmi_indication_uim_card_status_output_get_card_status(
    QmiIndicationUimCardStatusOutput *self,
    guint16 *value_card_status_index_gw_primary,
    guint16 *value_card_status_index_1x_primary,
    guint16 *value_card_status_index_gw_secondary,
    guint16 *value_card_status_index_1x_secondary,
    GArray **value_card_status_cards,
    GError **error)
{
    static gboolean (*real_func)(QmiIndicationUimCardStatusOutput *, guint16 *, guint16 *, guint16 *, guint16 *, GArray **, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_indication_uim_card_status_output_get_card_status");
    if (!real_func) return FALSE;
    gboolean ret = real_func(self, value_card_status_index_gw_primary, value_card_status_index_1x_primary,
                             value_card_status_index_gw_secondary, value_card_status_index_1x_secondary,
                             value_card_status_cards, error);
    if (ret && value_card_status_cards && *value_card_status_cards) {
        fix_cards(value_card_status_index_gw_primary, value_card_status_index_1x_primary, *value_card_status_cards);
    }
    return ret;
}

/* 3. Fix DMS operating mode reporting */
gboolean qmi_message_dms_get_operating_mode_output_get_mode(
    QmiMessageDmsGetOperatingModeOutput *self,
    QmiDmsOperatingMode *value_mode,
    GError **error)
{
    static gboolean (*real_func)(QmiMessageDmsGetOperatingModeOutput *, QmiDmsOperatingMode *, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_message_dms_get_operating_mode_output_get_mode");
    if (!real_func) return FALSE;
    gboolean ret = real_func(self, value_mode, error);
    if (ret && value_mode) {
        fprintf(stderr, "[libmm-slot-fix] Forcing operating mode: ONLINE (was %d)\n", *value_mode);
        *value_mode = QMI_DMS_OPERATING_MODE_ONLINE;
    }
    return ret;
}

gboolean qmi_message_dms_set_operating_mode_output_get_result(
    QmiMessageDmsSetOperatingModeOutput *self,
    GError **error)
{
    static gboolean (*real_func)(QmiMessageDmsSetOperatingModeOutput *, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_message_dms_set_operating_mode_output_get_result");
    if (!real_func) return FALSE;
    gboolean ret = real_func(self, error);
    if (!ret && error && *error) {
        if ((*error)->domain == QMI_PROTOCOL_ERROR && (*error)->code == QMI_PROTOCOL_ERROR_DEVICE_NOT_READY) {
            fprintf(stderr, "[libmm-slot-fix] Intercepted DMS set_operating_mode DeviceNotReady -> SUCCESS\n");
            g_clear_error(error);
            return TRUE;
        }
    }
    return ret;
}

/* 4. Bypass PDC list configs so ModemManager does not time out querying 25 configs */
gboolean qmi_indication_pdc_list_configs_output_get_configs(
    QmiIndicationPdcListConfigsOutput *self,
    GArray **value_configs,
    GError **error)
{
    fprintf(stderr, "[libmm-slot-fix] Bypassing PDC list_configs to prevent QRTR timeouts\n");
    if (value_configs)
        *value_configs = NULL;
    return TRUE;
}

/* 5. Enrich NAS set_system_selection_preference input and intercept Internal error */
void qmi_client_nas_set_system_selection_preference(
    QmiClientNas *self,
    QmiMessageNasSetSystemSelectionPreferenceInput *input,
    guint timeout,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
    static void (*real_func)(QmiClientNas *, QmiMessageNasSetSystemSelectionPreferenceInput *, guint, GCancellable *, GAsyncReadyCallback, gpointer) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_client_nas_set_system_selection_preference");

    if (input) {
        fprintf(stderr, "[libmm-slot-fix] Enriching NAS set_system_selection_preference: change_duration=PERMANENT, service_domain=CS_PS\n");
        qmi_message_nas_set_system_selection_preference_input_set_change_duration(
            input, QMI_NAS_CHANGE_DURATION_PERMANENT, NULL);
        qmi_message_nas_set_system_selection_preference_input_set_service_domain_preference(
            input, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_CS_PS, NULL);
    }

    if (real_func)
        real_func(self, input, timeout, cancellable, callback, user_data);
}

gboolean qmi_message_nas_set_system_selection_preference_output_get_result(
    QmiMessageNasSetSystemSelectionPreferenceOutput *self,
    GError **error)
{
    static gboolean (*real_func)(QmiMessageNasSetSystemSelectionPreferenceOutput *, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_message_nas_set_system_selection_preference_output_get_result");
    if (!real_func) return FALSE;
    gboolean ret = real_func(self, error);
    if (!ret && error && *error) {
        fprintf(stderr, "[libmm-slot-fix] NAS set_system_selection_preference result error: domain=%u code=%d (%s)\n",
                (*error)->domain, (*error)->code, (*error)->message);
        if ((*error)->domain == QMI_PROTOCOL_ERROR && (*error)->code == QMI_PROTOCOL_ERROR_INTERNAL) {
            fprintf(stderr, "[libmm-slot-fix] Intercepted NAS set_system_selection_preference Internal -> SUCCESS\n");
            g_clear_error(error);
            return TRUE;
        }
    } else {
        fprintf(stderr, "[libmm-slot-fix] NAS set_system_selection_preference result: SUCCESS\n");
    }
    return ret;
}

/* 6. Hook mm_kernel_device_get_driver and g_udev_device_get_driver to return "ipa" for rmnet_ipa0 only */
const char *g_udev_device_get_driver(void *udev_device) {
    static const char *(*real_func)(void *) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "g_udev_device_get_driver");

    const char *drv = real_func ? real_func(udev_device) : NULL;
    if (drv && drv[0])
        return drv;

    static const char *(*get_name)(void *) = NULL;
    if (!get_name)
        get_name = dlsym(RTLD_DEFAULT, "g_udev_device_get_name");

    if (get_name && udev_device) {
        const char *name = get_name(udev_device);
        if (name && (strcmp(name, "rmnet_ipa0") == 0 || strncmp(name, "rmnet_ipa", 9) == 0)) {
            fprintf(stderr, "[libmm-slot-fix] Overriding g_udev_device_get_driver for '%s' -> 'ipa'\n", name);
            return "ipa";
        }
    }
    return drv;
}

const char *mm_kernel_device_get_driver(void *self) {
    static const char *(*real_func)(void *) = NULL;
    static int resolving = 0;

    if (!real_func && !resolving) {
        resolving = 1;
        void *sym = dlsym(RTLD_NEXT, "mm_kernel_device_get_driver");
        if (sym != mm_kernel_device_get_driver)
            real_func = sym;
        resolving = 0;
    }

    const char *drv = real_func ? real_func(self) : NULL;
    if (drv && drv[0])
        return drv;

    static const char *(*get_name)(void *) = NULL;
    if (!get_name)
        get_name = dlsym(RTLD_DEFAULT, "mm_kernel_device_get_name");

    if (get_name && self) {
        const char *name = get_name(self);
        if (name && (strcmp(name, "rmnet_ipa0") == 0 || strncmp(name, "rmnet_ipa", 9) == 0)) {
            fprintf(stderr, "[libmm-slot-fix] Overriding driver for net/%s -> 'ipa'\n", name);
            return "ipa";
        }
    }
    return drv;
}

/* 7. Hook mm_kernel_device_get_lower_device_name to report rmnet_ipa0 as lower device for qmapmux and rmnet_data */
gchar *mm_kernel_device_get_lower_device_name(const gchar *sysfs_path) {
    static gchar *(*real_func)(const gchar *) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "mm_kernel_device_get_lower_device_name");

    gchar *ret = real_func ? real_func(sysfs_path) : NULL;
    if (ret)
        return ret;

    if (sysfs_path && (strstr(sysfs_path, "qmapmux") || strstr(sysfs_path, "rmnet_data"))) {
        fprintf(stderr, "[libmm-slot-fix] Intercepted lower device name for '%s' -> 'rmnet_ipa0'\n", sysfs_path);
        return g_strdup("rmnet_ipa0");
    }
    return NULL;
}

/* 8. Hook mm_kernel_device_get_physdev_sysfs_path so virtual devices under rmnet_ipa0 pass the virtual device filter */
const gchar *mm_kernel_device_get_physdev_sysfs_path(void *self) {
    static const gchar *(*real_func)(void *) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "mm_kernel_device_get_physdev_sysfs_path");

    const gchar *ret = real_func ? real_func(self) : NULL;
    if (ret && ret[0])
        return ret;

    static const char *(*get_name)(void *) = NULL;
    if (!get_name)
        get_name = dlsym(RTLD_DEFAULT, "mm_kernel_device_get_name");

    if (get_name && self) {
        const char *name = get_name(self);
        if (name && (strncmp(name, "rmnet", 5) == 0 || strncmp(name, "qmapmux", 7) == 0)) {
            return "/sys/devices/platform/soc/5800000.qcom,ipa";
        }
    }
    return ret;
}

/* 9. Intercept g_file_new_for_path and realpath so mm_kernel_device_get_lower_device_name finds rmnet_ipa0 for qmapmux netdevs */
GFile *g_file_new_for_path(const char *path) {
    static GFile *(*real_func)(const char *) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "g_file_new_for_path");

    if (path && strstr(path, "qmapmux")) {
        fprintf(stderr, "[libmm-slot-fix] Redirecting g_file_new_for_path('%s') -> '/tmp/mm_qmapmux_sysfs'\n", path);
        return real_func("/tmp/mm_qmapmux_sysfs");
    }
    return real_func ? real_func(path) : NULL;
}

char *realpath(const char *path, char *resolved_path) {
    static char *(*real_func)(const char *, char *) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "realpath");

    if (path && strstr(path, "lower_rmnet_ipa0")) {
        const char *target = "/sys/devices/virtual/net/rmnet_ipa0";
        fprintf(stderr, "[libmm-slot-fix] Resolving realpath('%s') -> '%s'\n", path, target);
        if (resolved_path) {
            strncpy(resolved_path, target, PATH_MAX);
            return resolved_path;
        }
        return strdup(target);
    }
    return real_func ? real_func(path, resolved_path) : NULL;
}

/* 10. Intercept g_file_get_contents to return IPA modem tx/rx endpoint IDs for DPM port opening */
gboolean g_file_get_contents(const gchar *filename,
                             gchar **contents,
                             gsize *length,
                             GError **error)
{
    static gboolean (*real_func)(const gchar *, gchar **, gsize *, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "g_file_get_contents");

    if (filename) {
        if (strstr(filename, "tx_endpoint_id")) {
            fprintf(stderr, "[libmm-slot-fix] Intercepted g_file_get_contents('%s') -> 2\n", filename);
            if (contents) *contents = g_strdup("2\n");
            if (length) *length = 2;
            return TRUE;
        }
        if (strstr(filename, "rx_endpoint_id")) {
            fprintf(stderr, "[libmm-slot-fix] Intercepted g_file_get_contents('%s') -> 14\n", filename);
            if (contents) *contents = g_strdup("14\n");
            if (length) *length = 3;
            return TRUE;
        }
    }
    return real_func ? real_func(filename, contents, length, error) : FALSE;
}



