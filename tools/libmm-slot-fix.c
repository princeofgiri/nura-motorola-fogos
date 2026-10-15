#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <libqmi-glib.h>

__attribute__((constructor)) static void mm_fix_init(void) {
    fprintf(stderr, ">>> [libmm-slot-fix] INITIALIZED <<<\n");
}

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
        GArray *cards = *value_card_status_cards;
        for (guint i = 0; i < cards->len; i++) {
            QmiMessageUimGetCardStatusOutputCardStatusCardsElement *card =
                &g_array_index(cards, QmiMessageUimGetCardStatusOutputCardStatusCardsElement, i);
            if (card->card_state == QMI_UIM_CARD_STATE_PRESENT && card->applications) {
                for (guint j = 0; j < card->applications->len; j++) {
                    QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElementV2 *app =
                        &g_array_index(card->applications, QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElementV2, j);
                    if (app->state == QMI_UIM_CARD_APPLICATION_STATE_DETECTED) {
                        fprintf(stderr, "[libmm-slot-fix] Promoting card[%u].app[%u] from DETECTED -> READY\n", i, j);
                        app->state = QMI_UIM_CARD_APPLICATION_STATE_READY;
                    }
                }
            }
        }
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
