#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <libqrtr-glib.h>

#define CHUNK_SIZE 1024

typedef enum {
    CMD_LIST,
    CMD_LOAD,
    CMD_ACTIVATE,
    CMD_DEACTIVATE,
} ToolCommand;

typedef struct {
    GMainLoop *loop;
    QrtrBus *bus;
    QmiDevice *device;
    QmiClientPdc *client;
    guint32 token;
    ToolCommand cmd;

    /* For load / activate */
    const char *file_path;
    guint8 *file_data;
    gsize file_size;
    gsize offset;
    GArray *checksum_id;

    gulong load_ind_id;
    gulong set_ind_id;
    gulong act_ind_id;
    gulong list_ind_id;
    gulong refresh_ind_id;
} ToolCtx;

static ToolCtx ctx;

static void send_next_chunk(void);
static void do_set_selected(void);
static void do_activate(void);
static void do_deactivate(void);
static void do_list_configs(QmiPdcConfigurationType type);
static void do_get_selected(QmiPdcConfigurationType type);

/* --- List callbacks --- */
static void list_ready_cb(QmiClientPdc *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessagePdcListConfigsOutput *output = qmi_client_pdc_list_configs_finish(client, res, &error);
    if (!output) {
        fprintf(stderr, "[PDC] list_configs_finish failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (!qmi_message_pdc_list_configs_output_get_result(output, &error)) {
        fprintf(stderr, "[PDC] list_configs_output_get_result failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        qmi_message_pdc_list_configs_output_unref(output);
        g_main_loop_quit(ctx.loop);
        return;
    }
    qmi_message_pdc_list_configs_output_unref(output);
}

static void on_list_configs_indication(QmiClientPdc *client, QmiIndicationPdcListConfigsOutput *output, gpointer user_data) {
    guint16 error_code = 0;
    GError *error = NULL;
    GArray *configs = NULL;

    if (!qmi_indication_pdc_list_configs_output_get_indication_result(output, &error_code, &error)) {
        fprintf(stderr, "[PDC] List indication error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (error_code != 0) {
        fprintf(stderr, "[PDC] List indication error_code: %s (%d)\n", qmi_protocol_error_get_string((QmiProtocolError)error_code), error_code);
        g_main_loop_quit(ctx.loop);
        return;
    }

    if (qmi_indication_pdc_list_configs_output_get_configs(output, &configs, NULL) && configs) {
        printf("[PDC] Found %u configurations:\n", configs->len);
        for (guint i = 0; i < configs->len; i++) {
            QmiIndicationPdcListConfigsOutputConfigsElement *el = &g_array_index(configs, QmiIndicationPdcListConfigsOutputConfigsElement, i);
            printf("  [%02u] Type %u, ID: ", i, el->config_type);
            for (guint j = 0; j < el->id->len; j++) {
                printf("%02X", g_array_index(el->id, guint8, j));
            }
            printf("\n");
        }
    } else {
        printf("[PDC] No configurations found for this type.\n");
    }

    static int phase = 0;
    if (phase == 0) {
        phase = 1;
        printf("[PDC] Querying software configurations...\n");
        do_list_configs(QMI_PDC_CONFIGURATION_TYPE_SOFTWARE);
    } else {
        printf("[PDC] Querying currently active software configuration...\n");
        do_get_selected(QMI_PDC_CONFIGURATION_TYPE_SOFTWARE);
    }
}

static void get_selected_ready_cb(QmiClientPdc *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessagePdcGetSelectedConfigOutput *output = qmi_client_pdc_get_selected_config_finish(client, res, &error);
    if (!output) {
        fprintf(stderr, "[PDC] get_selected_config_finish failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (!qmi_message_pdc_get_selected_config_output_get_result(output, &error)) {
        fprintf(stderr, "[PDC] get_selected_config_output_get_result failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        qmi_message_pdc_get_selected_config_output_unref(output);
        g_main_loop_quit(ctx.loop);
        return;
    }
    qmi_message_pdc_get_selected_config_output_unref(output);
}

static void on_get_selected_indication(QmiClientPdc *client, QmiIndicationPdcGetSelectedConfigOutput *output, gpointer user_data) {
    guint16 error_code = 0;
    GError *error = NULL;
    GArray *active_id = NULL;

    if (!qmi_indication_pdc_get_selected_config_output_get_indication_result(output, &error_code, &error)) {
        fprintf(stderr, "[PDC] Get selected indication error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (error_code != 0) {
        fprintf(stderr, "[PDC] Get selected indication error_code: %s (%d)\n", qmi_protocol_error_get_string((QmiProtocolError)error_code), error_code);
        g_main_loop_quit(ctx.loop);
        return;
    }

    if (qmi_indication_pdc_get_selected_config_output_get_active_id(output, &active_id, NULL) && active_id) {
        printf("[PDC] Current Active Config ID: ");
        for (guint j = 0; j < active_id->len; j++) {
            printf("%02X", g_array_index(active_id, guint8, j));
        }
        printf("\n");
    } else {
        printf("[PDC] No active config ID returned!\n");
    }
    g_main_loop_quit(ctx.loop);
}

static void do_get_selected(QmiPdcConfigurationType type) {
    QmiMessagePdcGetSelectedConfigInput *input = qmi_message_pdc_get_selected_config_input_new();
    qmi_message_pdc_get_selected_config_input_set_token(input, ctx.token++, NULL);
    qmi_message_pdc_get_selected_config_input_set_config_type(input, type, NULL);
    qmi_client_pdc_get_selected_config(ctx.client, input, 10, NULL, (GAsyncReadyCallback)get_selected_ready_cb, NULL);
    qmi_message_pdc_get_selected_config_input_unref(input);
}

static void do_list_configs(QmiPdcConfigurationType type) {
    QmiMessagePdcListConfigsInput *input = qmi_message_pdc_list_configs_input_new();
    qmi_message_pdc_list_configs_input_set_token(input, ctx.token++, NULL);
    qmi_message_pdc_list_configs_input_set_config_type(input, type, NULL);
    qmi_client_pdc_list_configs(ctx.client, input, 10, NULL, (GAsyncReadyCallback)list_ready_cb, NULL);
    qmi_message_pdc_list_configs_input_unref(input);
}

/* --- Load callbacks --- */
static void load_ready_cb(QmiClientPdc *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessagePdcLoadConfigOutput *output = qmi_client_pdc_load_config_finish(client, res, &error);
    if (!output) {
        fprintf(stderr, "[PDC] load_config_finish failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (!qmi_message_pdc_load_config_output_get_result(output, &error)) {
        fprintf(stderr, "[PDC] load_config_output_get_result returned error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        qmi_message_pdc_load_config_output_unref(output);
        g_main_loop_quit(ctx.loop);
        return;
    }
    qmi_message_pdc_load_config_output_unref(output);
}

static void send_next_chunk(void) {
    GError *error = NULL;
    gsize remaining = ctx.file_size - ctx.offset;
    gsize this_chunk = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;

    QmiMessagePdcLoadConfigInput *input = qmi_message_pdc_load_config_input_new();
    qmi_message_pdc_load_config_input_set_token(input, ctx.token++, NULL);

    GArray *chunk_arr = g_array_new(FALSE, FALSE, sizeof(guint8));
    g_array_append_vals(chunk_arr, ctx.file_data + ctx.offset, this_chunk);

    if (!qmi_message_pdc_load_config_input_set_config_chunk(input,
                                                            QMI_PDC_CONFIGURATION_TYPE_SOFTWARE,
                                                            ctx.checksum_id,
                                                            (guint32)ctx.file_size,
                                                            chunk_arr,
                                                            &error)) {
        fprintf(stderr, "[PDC] set_config_chunk failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_array_unref(chunk_arr);
        qmi_message_pdc_load_config_input_unref(input);
        g_main_loop_quit(ctx.loop);
        return;
    }
    g_array_unref(chunk_arr);

    printf("[PDC] Uploading chunk: offset %zu / %zu (%zu bytes)...\n", ctx.offset, ctx.file_size, this_chunk);
    ctx.offset += this_chunk;

    qmi_client_pdc_load_config(ctx.client, input, 15, NULL, (GAsyncReadyCallback)load_ready_cb, NULL);
    qmi_message_pdc_load_config_input_unref(input);
}

static void on_load_config_indication(QmiClientPdc *client, QmiIndicationPdcLoadConfigOutput *output, gpointer user_data) {
    guint16 error_code = 0;
    GError *error = NULL;
    guint32 remaining = 0;
    gboolean frame_reset = FALSE;

    if (!qmi_indication_pdc_load_config_output_get_indication_result(output, &error_code, &error)) {
        fprintf(stderr, "[PDC] Load indication failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (error_code != 0) {
        fprintf(stderr, "[PDC] Load indication error_code: %s (%d)\n", qmi_protocol_error_get_string((QmiProtocolError)error_code), error_code);
        g_main_loop_quit(ctx.loop);
        return;
    }

    if (qmi_indication_pdc_load_config_output_get_frame_reset(output, &frame_reset, NULL) && frame_reset) {
        fprintf(stderr, "[PDC] Warning: frame reset requested by modem\n");
    }

    qmi_indication_pdc_load_config_output_get_remaining_size(output, &remaining, NULL);
    printf("[PDC] Modem ACKed chunk. Remaining according to modem: %u bytes\n", remaining);

    if (remaining == 0 || ctx.offset >= ctx.file_size) {
        printf("[PDC] *** UPLOAD COMPLETE! Now selecting config... ***\n");
        do_set_selected();
    } else {
        send_next_chunk();
    }
}

/* --- Set Selected & Activate callbacks --- */
static void set_selected_ready_cb(QmiClientPdc *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessagePdcSetSelectedConfigOutput *output = qmi_client_pdc_set_selected_config_finish(client, res, &error);
    if (!output) {
        fprintf(stderr, "[PDC] set_selected_config_finish failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (!qmi_message_pdc_set_selected_config_output_get_result(output, &error)) {
        fprintf(stderr, "[PDC] set_selected_config_output_get_result error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        qmi_message_pdc_set_selected_config_output_unref(output);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] set_selected_config request accepted by modem, waiting for indication...\n");
    qmi_message_pdc_set_selected_config_output_unref(output);
}

static void on_set_selected_indication(QmiClientPdc *client, QmiIndicationPdcSetSelectedConfigOutput *output, gpointer user_data) {
    guint16 error_code = 0;
    GError *error = NULL;
    if (!qmi_indication_pdc_set_selected_config_output_get_indication_result(output, &error_code, &error)) {
        fprintf(stderr, "[PDC] Set selected indication failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (error_code != 0) {
        fprintf(stderr, "[PDC] Set selected indication error_code: %s (%d)\n", qmi_protocol_error_get_string((QmiProtocolError)error_code), error_code);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] *** CONFIG SELECTED SUCCESSFULLY! Now activating... ***\n");
    do_activate();
}

static void do_set_selected(void) {
    QmiMessagePdcSetSelectedConfigInput *input = qmi_message_pdc_set_selected_config_input_new();
    qmi_message_pdc_set_selected_config_input_set_token(input, ctx.token++, NULL);
    qmi_message_pdc_set_selected_config_input_set_type_with_id_v2(input,
                                                                  QMI_PDC_CONFIGURATION_TYPE_SOFTWARE,
                                                                  ctx.checksum_id,
                                                                  NULL);
    qmi_client_pdc_set_selected_config(ctx.client, input, 15, NULL, (GAsyncReadyCallback)set_selected_ready_cb, NULL);
    qmi_message_pdc_set_selected_config_input_unref(input);
}

static void activate_ready_cb(QmiClientPdc *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessagePdcActivateConfigOutput *output = qmi_client_pdc_activate_config_finish(client, res, &error);
    if (!output) {
        printf("[PDC] activate_config_finish: %s (modem may be rebooting baseband)\n", error ? error->message : "none");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    if (!qmi_message_pdc_activate_config_output_get_result(output, &error)) {
        fprintf(stderr, "[PDC] activate_config_output_get_result error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        qmi_message_pdc_activate_config_output_unref(output);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] activate_config request accepted by modem, waiting for indication...\n");
    qmi_message_pdc_activate_config_output_unref(output);
}

static void on_activate_indication(QmiClientPdc *client, QmiIndicationPdcActivateConfigOutput *output, gpointer user_data) {
    guint16 error_code = 0;
    GError *error = NULL;
    if (!qmi_indication_pdc_activate_config_output_get_indication_result(output, &error_code, &error)) {
        fprintf(stderr, "[PDC] Activate indication error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else if (error_code != 0) {
        fprintf(stderr, "[PDC] Activate indication error_code: %s (%d)\n", qmi_protocol_error_get_string((QmiProtocolError)error_code), error_code);
    } else {
        printf("[PDC] *** ACTIVATION SUCCESSFUL! Baseband refreshed! ***\n");
    }
    g_main_loop_quit(ctx.loop);
}

static void do_activate(void) {
    QmiMessagePdcActivateConfigInput *input = qmi_message_pdc_activate_config_input_new();
    qmi_message_pdc_activate_config_input_set_token(input, ctx.token++, NULL);
    qmi_message_pdc_activate_config_input_set_config_type(input, QMI_PDC_CONFIGURATION_TYPE_SOFTWARE, NULL);
    qmi_client_pdc_activate_config(ctx.client, input, 15, NULL, (GAsyncReadyCallback)activate_ready_cb, NULL);
    qmi_message_pdc_activate_config_input_unref(input);
}

static void on_refresh_indication(QmiClientPdc *client, QmiIndicationPdcRefreshOutput *output, gpointer user_data) {
    QmiPdcRefreshEventType event_type;
    guint sub_id = 0, slot_id = 0;
    if (qmi_indication_pdc_refresh_output_get_refresh_event(output, &event_type, NULL)) {
        printf("[PDC] *** REFRESH INDICATION: type=%u ***\n", event_type);
    }
    if (qmi_indication_pdc_refresh_output_get_subscription_id(output, &sub_id, NULL)) {
        printf("[PDC] Refresh subscription ID: %u\n", sub_id);
    }
    if (qmi_indication_pdc_refresh_output_get_slot_id(output, &slot_id, NULL)) {
        printf("[PDC] Refresh slot ID: %u\n", slot_id);
    }
}

static void register_ready_cb(QmiClientPdc *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessagePdcRegisterOutput *output = qmi_client_pdc_register_finish(client, res, &error);
    if (!output) {
        fprintf(stderr, "[PDC] Failed to register for PDC indications: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Registered for PDC indications successfully!\n");
    qmi_message_pdc_register_output_unref(output);

    if (ctx.cmd == CMD_LIST) {
        printf("[PDC] Querying platform configurations...\n");
        do_list_configs(QMI_PDC_CONFIGURATION_TYPE_PLATFORM);
    } else if (ctx.cmd == CMD_LOAD) {
        send_next_chunk();
    } else if (ctx.cmd == CMD_ACTIVATE) {
        do_set_selected();
    }
}

static void do_register(void) {
    QmiMessagePdcRegisterInput *input = qmi_message_pdc_register_input_new();
    qmi_message_pdc_register_input_set_enable_reporting(input, TRUE, NULL);
    qmi_message_pdc_register_input_set_enable_refresh(input, TRUE, NULL);
    qmi_client_pdc_register(ctx.client, input, 10, NULL, (GAsyncReadyCallback)register_ready_cb, NULL);
    qmi_message_pdc_register_input_unref(input);
}

/* --- Service startup --- */
static void client_allocated_cb(QmiDevice *device, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    ctx.client = QMI_CLIENT_PDC(qmi_device_allocate_client_finish(device, res, &error));
    if (!ctx.client) {
        fprintf(stderr, "[PDC] Failed to allocate PDC client: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Connected to PDC service on modem!\n");

    ctx.load_ind_id    = g_signal_connect(ctx.client, "load-config", G_CALLBACK(on_load_config_indication), NULL);
    ctx.set_ind_id     = g_signal_connect(ctx.client, "set-selected-config", G_CALLBACK(on_set_selected_indication), NULL);
    ctx.act_ind_id     = g_signal_connect(ctx.client, "activate-config", G_CALLBACK(on_activate_indication), NULL);
    ctx.list_ind_id    = g_signal_connect(ctx.client, "list-configs", G_CALLBACK(on_list_configs_indication), NULL);
    ctx.refresh_ind_id = g_signal_connect(ctx.client, "refresh", G_CALLBACK(on_refresh_indication), NULL);
    g_signal_connect(ctx.client, "get-selected-config", G_CALLBACK(on_get_selected_indication), NULL);

    do_register();
}

static void device_open_cb(QmiDevice *device, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    if (!qmi_device_open_finish(device, res, &error)) {
        fprintf(stderr, "[PDC] Failed to open QMI device: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Device opened. Allocating PDC client...\n");
    qmi_device_allocate_client(device, QMI_SERVICE_PDC, QMI_CID_NONE, 10, NULL, (GAsyncReadyCallback)client_allocated_cb, NULL);
}

static void device_new_cb(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    ctx.device = qmi_device_new_from_node_finish(res, &error);
    if (!ctx.device) {
        fprintf(stderr, "[PDC] Failed to create QmiDevice from node: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Created QmiDevice. Opening with EXPECT_INDICATIONS...\n");
    qmi_device_open(ctx.device, QMI_DEVICE_OPEN_FLAGS_AUTO | QMI_DEVICE_OPEN_FLAGS_EXPECT_INDICATIONS, 10, NULL, (GAsyncReadyCallback)device_open_cb, NULL);
}

static void qrtr_bus_ready_cb(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    ctx.bus = qrtr_bus_new_finish(res, &error);
    if (!ctx.bus) {
        fprintf(stderr, "[PDC] Failed to connect to QRTR bus: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    QrtrNode *node = qrtr_bus_peek_node(ctx.bus, 0);
    if (!node) {
        fprintf(stderr, "[PDC] Node 0 not found on QRTR bus\n");
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Found Node 0. Initializing QmiDevice...\n");
    qmi_device_new_from_node(node, NULL, (GAsyncReadyCallback)device_new_cb, NULL);
}

static GArray *hex_to_bytes(const char *hex) {
    size_t len = strlen(hex);
    if (len % 2 != 0) return NULL;
    GArray *arr = g_array_sized_new(FALSE, FALSE, sizeof(guint8), len / 2);
    for (size_t i = 0; i < len; i += 2) {
        guint8 byte = 0;
        if (sscanf(hex + i, "%2hhx", &byte) != 1) {
            g_array_unref(arr);
            return NULL;
        }
        g_array_append_val(arr, byte);
    }
    return arr;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (argc < 2) {
        fprintf(stderr, "Usage:\n  %s list\n  %s load <path_to_mbn_file>\n  %s activate <hex_sha1_id>\n", argv[0], argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        ctx.cmd = CMD_LIST;
    } else if (strcmp(argv[1], "load") == 0 && argc >= 3) {
        ctx.cmd = CMD_LOAD;
        ctx.file_path = argv[2];
        GError *error = NULL;
        if (!g_file_get_contents(ctx.file_path, (gchar **)&ctx.file_data, &ctx.file_size, &error)) {
            fprintf(stderr, "Failed to read file %s: %s\n", ctx.file_path, error ? error->message : "unknown");
            return 1;
        }
        printf("[PDC] Loaded %s: %zu bytes\n", ctx.file_path, ctx.file_size);

        gsize hash_size = g_checksum_type_get_length(G_CHECKSUM_SHA1);
        GChecksum *cs = g_checksum_new(G_CHECKSUM_SHA1);
        g_checksum_update(cs, ctx.file_data, ctx.file_size);
        ctx.checksum_id = g_array_sized_new(FALSE, FALSE, sizeof(guint8), hash_size);
        g_array_set_size(ctx.checksum_id, hash_size);
        g_checksum_get_digest(cs, &g_array_index(ctx.checksum_id, guint8, 0), &hash_size);
        g_checksum_free(cs);

        printf("[PDC] SHA1 ID: ");
        for (guint i = 0; i < ctx.checksum_id->len; i++) {
            printf("%02X", g_array_index(ctx.checksum_id, guint8, i));
        }
        printf("\n");
    } else if (strcmp(argv[1], "activate") == 0 && argc >= 3) {
        ctx.cmd = CMD_ACTIVATE;
        ctx.checksum_id = hex_to_bytes(argv[2]);
        if (!ctx.checksum_id) {
            fprintf(stderr, "Invalid hex SHA1 string: %s\n", argv[2]);
            return 1;
        }
        printf("[PDC] Activating config ID: %s\n", argv[2]);
    } else {
        fprintf(stderr, "Invalid arguments.\n");
        return 1;
    }

    ctx.loop = g_main_loop_new(NULL, FALSE);
    ctx.token = 1;
    ctx.offset = 0;

    printf("[PDC] Connecting to QRTR bus...\n");
    qrtr_bus_new(1000, NULL, (GAsyncReadyCallback)qrtr_bus_ready_cb, NULL);

    g_main_loop_run(ctx.loop);
    printf("[PDC] Done.\n");
    return 0;
}
