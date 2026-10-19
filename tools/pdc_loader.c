#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <libqmi-glib.h>

#define CHUNK_SIZE 1024

typedef struct {
    GMainLoop *loop;
    QmiDevice *device;
    QmiClientPdc *client;
    guint32 token;

    guint8 *file_data;
    gsize file_size;
    gsize offset;
    GArray *checksum_id;

    gulong load_ind_id;
    gulong set_ind_id;
    gulong act_ind_id;
} LoaderCtx;

static LoaderCtx ctx;

static void send_next_chunk(void);
static void do_set_selected(void);
static void do_activate(void);

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
        fprintf(stderr, "[PDC] load_config result error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
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
    qmi_indication_pdc_load_config_output_get_remaining_size(output, &remaining, NULL);
    printf("[PDC] Modem ACKed chunk. Remaining on modem: %u bytes\n", remaining);

    if (remaining == 0 || ctx.offset >= ctx.file_size) {
        printf("[PDC] *** UPLOAD COMPLETE! Now selecting config... ***\n");
        do_set_selected();
    } else {
        send_next_chunk();
    }
}

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
        fprintf(stderr, "[PDC] set_selected_config result error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
    }
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
        printf("[PDC] activate_config_finish: %s (modem likely resetting now, which is normal)\n", error ? error->message : "none");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    qmi_message_pdc_activate_config_output_unref(output);
}

static void on_activate_indication(QmiClientPdc *client, QmiIndicationPdcActivateConfigOutput *output, gpointer user_data) {
    printf("[PDC] *** ACTIVATION INDICATION RECEIVED! SUCCESS! ***\n");
    g_main_loop_quit(ctx.loop);
}

static void do_activate(void) {
    QmiMessagePdcActivateConfigInput *input = qmi_message_pdc_activate_config_input_new();
    qmi_message_pdc_activate_config_input_set_token(input, ctx.token++, NULL);
    qmi_message_pdc_activate_config_input_set_config_type(input, QMI_PDC_CONFIGURATION_TYPE_SOFTWARE, NULL);
    qmi_client_pdc_activate_config(ctx.client, input, 15, NULL, (GAsyncReadyCallback)activate_ready_cb, NULL);
    qmi_message_pdc_activate_config_input_unref(input);
}

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

    ctx.load_ind_id = g_signal_connect(ctx.client, "load-config", G_CALLBACK(on_load_config_indication), NULL);
    ctx.set_ind_id  = g_signal_connect(ctx.client, "set-selected-config", G_CALLBACK(on_set_selected_indication), NULL);
    ctx.act_ind_id  = g_signal_connect(ctx.client, "activate-config", G_CALLBACK(on_activate_indication), NULL);

    send_next_chunk();
}

static void device_open_cb(QmiDevice *device, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    if (!qmi_device_open_finish(device, res, &error)) {
        fprintf(stderr, "[PDC] Failed to open QMI device: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Opened device qrtr://0. Allocating PDC client...\n");
    qmi_device_allocate_client(device, QMI_SERVICE_PDC, QMI_CID_NONE, 10, NULL, (GAsyncReadyCallback)client_allocated_cb, NULL);
}

#include <libqrtr-glib.h>

static void device_new_cb(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    ctx.device = qmi_device_new_from_node_finish(res, &error);
    if (!ctx.device) {
        fprintf(stderr, "[PDC] Failed to create QmiDevice from node: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Created QmiDevice from node 0. Opening...\n");
    qmi_device_open(ctx.device, QMI_DEVICE_OPEN_FLAGS_EXPECT_INDICATIONS, 10, NULL, (GAsyncReadyCallback)device_open_cb, NULL);
}

static void bus_new_cb(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QrtrBus *bus = qrtr_bus_new_finish(res, &error);
    if (!bus) {
        fprintf(stderr, "[PDC] qrtr_bus_new failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Connected to QRTR bus. Looking up node 0...\n");
    QrtrNode *node = qrtr_bus_get_node(bus, 0);
    if (!node) {
        fprintf(stderr, "[PDC] Node 0 not found on QRTR bus\n");
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[PDC] Found node 0. Creating QmiDevice...\n");
    qmi_device_new_from_node(node, NULL, (GAsyncReadyCallback)device_new_cb, NULL);
    g_object_unref(node);
    g_object_unref(bus);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path_to_mbn_file>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    GError *error = NULL;
    if (!g_file_get_contents(file_path, (gchar **)&ctx.file_data, &ctx.file_size, &error)) {
        fprintf(stderr, "Failed to read file %s: %s\n", file_path, error ? error->message : "unknown");
        return 1;
    }
    printf("[PDC] Loaded %s: %zu bytes\n", file_path, ctx.file_size);

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

    ctx.loop = g_main_loop_new(NULL, FALSE);
    ctx.token = 1;
    ctx.offset = 0;

    qrtr_bus_new(1000, NULL, (GAsyncReadyCallback)bus_new_cb, NULL);

    g_main_loop_run(ctx.loop);
    printf("[PDC] Done.\n");
    return 0;
}
