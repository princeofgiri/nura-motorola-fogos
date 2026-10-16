#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <libqmi-glib.h>

__attribute__((constructor)) static void mm_fix_init(void) {
    fprintf(stderr, ">>> [libmm-slot-fix] INITIALIZED <<<\n");
}

static guint8 cached_iccid_raw[10] = { 0x98, 0x26, 0x01, 0x00, 0x84, 0x09, 0x19, 0x14, 0x89, 0xF9 };
static gboolean have_cached_iccid = TRUE;

static void fix_slot_array(GArray *arr) {
    if (!arr) return;
    for (guint i = 0; i < arr->len; i++) {
        QmiPhysicalSlotStatusSlot *s = &g_array_index(arr, QmiPhysicalSlotStatusSlot, i);
        gboolean has_sim = (s->physical_card_status == QMI_UIM_PHYSICAL_CARD_STATE_PRESENT) &&
                           (s->iccid && s->iccid->len > 0);
        if (!has_sim) {
            fprintf(stderr, "[libmm-slot-fix] Marking slot %u INACTIVE (card_status=%d, iccid_len=%u)\n",
                    i + 1, s->physical_card_status, s->iccid ? s->iccid->len : 0);
            s->physical_slot_status = QMI_UIM_SLOT_STATE_INACTIVE;
        } else {
            fprintf(stderr, "[libmm-slot-fix] Slot %u ACTIVE with SIM (card_status=%d, iccid_len=%u)\n",
                    i + 1, s->physical_card_status, s->iccid ? s->iccid->len : 0);
            s->physical_slot_status = QMI_UIM_SLOT_STATE_ACTIVE;
            if (s->iccid && s->iccid->len >= 10) {
                memcpy(cached_iccid_raw, s->iccid->data, 10);
                have_cached_iccid = TRUE;
            }
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

    if (!real_func)
        return FALSE;

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

    if (!real_func)
        return FALSE;

    gboolean ret = real_func(self, value_physical_slot_status, error);
    if (ret && value_physical_slot_status && *value_physical_slot_status) {
        fix_slot_array(*value_physical_slot_status);
    }
    return ret;
}

static void fix_cards(guint16 *value_card_status_index_gw_primary,
                      guint16 *value_card_status_index_1x_primary,
                      GArray *cards) {
    if (!cards) return;
    for (guint i = 0; i < cards->len; i++) {
        QmiMessageUimGetCardStatusOutputCardStatusCardsElement *card =
            &g_array_index(cards, QmiMessageUimGetCardStatusOutputCardStatusCardsElement, i);
        if (card->card_state == QMI_UIM_CARD_STATE_PRESENT) {
            card->upin_state = QMI_UIM_PIN_STATE_DISABLED;
            if (card->applications) {
                for (guint j = 0; j < card->applications->len; j++) {
                    QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElementV2 *app =
                        &g_array_index(card->applications, QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElementV2, j);
                    if (app->state == QMI_UIM_CARD_APPLICATION_STATE_DETECTED ||
                        app->state == QMI_UIM_CARD_APPLICATION_STATE_UNKNOWN) {
                        fprintf(stderr, "[libmm-slot-fix] Promoting card[%u].app[%u] from %d -> READY\n", i, j, app->state);
                        app->state = QMI_UIM_CARD_APPLICATION_STATE_READY;
                    }
                    if (app->pin1_state == QMI_UIM_PIN_STATE_NOT_INITIALIZED || app->pin1_state == 0) {
                        fprintf(stderr, "[libmm-slot-fix] Setting card[%u].app[%u] pin1_state to DISABLED\n", i, j);
                        app->pin1_state = QMI_UIM_PIN_STATE_DISABLED;
                    }
                }
            }
        }
    }
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

    if (!real_func)
        return FALSE;

    gboolean ret = real_func(self,
                             value_card_status_index_gw_primary,
                             value_card_status_index_1x_primary,
                             value_card_status_index_gw_secondary,
                             value_card_status_index_1x_secondary,
                             value_card_status_cards,
                             error);

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

    if (!real_func)
        return FALSE;

    gboolean ret = real_func(self,
                             value_card_status_index_gw_primary,
                             value_card_status_index_1x_primary,
                             value_card_status_index_gw_secondary,
                             value_card_status_index_1x_secondary,
                             value_card_status_cards,
                             error);

    if (ret && value_card_status_cards && *value_card_status_cards) {
        fix_cards(value_card_status_index_gw_primary, value_card_status_index_1x_primary, *value_card_status_cards);
    }

    return ret;
}

gboolean qmi_indication_pdc_list_configs_output_get_configs(
    QmiIndicationPdcListConfigsOutput *self,
    GArray **value_configs,
    GError **error)
{
    fprintf(stderr, "[libmm-slot-fix] Bypassing PDC carrier config query to prevent qrtr timeouts\n");
    if (value_configs)
        *value_configs = NULL;
    return TRUE;
}

gboolean qmi_message_dms_get_operating_mode_output_get_mode(
    QmiMessageDmsGetOperatingModeOutput *self,
    QmiDmsOperatingMode *value_mode,
    GError **error)
{
    static gboolean (*real_func)(QmiMessageDmsGetOperatingModeOutput *, QmiDmsOperatingMode *, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_message_dms_get_operating_mode_output_get_mode");

    if (!real_func)
        return FALSE;

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

    if (!real_func)
        return FALSE;

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

gboolean qmi_message_uim_read_transparent_output_get_result(
    QmiMessageUimReadTransparentOutput *self,
    GError **error)
{
    static gboolean (*real_func)(QmiMessageUimReadTransparentOutput *, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_message_uim_read_transparent_output_get_result");

    if (!real_func)
        return FALSE;

    gboolean ret = real_func(self, error);
    if (!ret && error && *error) {
        if ((*error)->code == 37 || g_str_has_suffix((*error)->message, "UimUninitialized")) {
            fprintf(stderr, "[libmm-slot-fix] Intercepted UimUninitialized error in read_transparent result -> SUCCESS\n");
            g_clear_error(error);
            return TRUE;
        }
    }
    return ret;
}

gboolean qmi_message_uim_read_transparent_output_get_read_result(
    QmiMessageUimReadTransparentOutput *self,
    GArray **value_read_result,
    GError **error)
{
    static gboolean (*real_func)(QmiMessageUimReadTransparentOutput *, GArray **, GError **) = NULL;
    if (!real_func)
        real_func = dlsym(RTLD_NEXT, "qmi_message_uim_read_transparent_output_get_read_result");

    if (!real_func)
        return FALSE;

    gboolean ret = real_func(self, value_read_result, error);
    if ((!ret || !value_read_result || !*value_read_result) && have_cached_iccid) {
        fprintf(stderr, "[libmm-slot-fix] Providing synthesized ICCID bytes for read_transparent\n");
        if (error && *error)
            g_clear_error(error);
        if (value_read_result) {
            GArray *arr = g_array_sized_new(FALSE, FALSE, sizeof(guint8), sizeof(cached_iccid_raw));
            g_array_append_vals(arr, cached_iccid_raw, sizeof(cached_iccid_raw));
            *value_read_result = arr;
        }
        return TRUE;
    }
    return ret;
}
