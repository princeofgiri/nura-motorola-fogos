#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <libqrtr-glib.h>

static GMainLoop *loop;
static QmiDevice *qmi_dev;
static QmiClientAtr *atr_client;
static const char *cmd_to_send = "AT+CFUN?\r\n";

static void on_atr_received(QmiClientAtr *client, QmiIndicationAtrReceivedOutput *output, gpointer user_data) {
    const gchar *msg = NULL;
    if (qmi_indication_atr_received_output_get_message(output, &msg, NULL) && msg) {
        printf("[ATR] Response:\n%s\n", msg);
    } else {
        printf("[ATR] Indication received without message.\n");
    }
    g_main_loop_quit(loop);
}

static void on_send_ready(QmiClientAtr *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageAtrSendOutput *out = qmi_client_atr_send_finish(client, res, &error);
    if (!out) {
        printf("[ATR] send finish error: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
    } else {
        if (!qmi_message_atr_send_output_get_result(out, &error)) {
            printf("[ATR] send result error: %s\n", error ? error->message : "unknown");
            g_clear_error(&error);
            g_main_loop_quit(loop);
        } else {
            printf("[ATR] Command sent successfully, waiting for response indication...\n");
        }
        qmi_message_atr_send_output_unref(out);
    }
}

static gboolean on_timeout(gpointer data) {
    printf("[ATR] Timeout waiting for indication (2 seconds)\n");
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static void on_atr_client_allocated(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiClient *client = qmi_device_allocate_client_finish(dev, res, &error);
    if (!client) {
        fprintf(stderr, "[ATR] Allocate ATR client failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    atr_client = QMI_CLIENT_ATR(client);
    printf("[ATR] Allocated ATR client with CID: %u\n", qmi_client_get_cid(client));

    g_signal_connect(atr_client, "atr-received", G_CALLBACK(on_atr_received), NULL);

    printf("[ATR] Sending command: %s\n", cmd_to_send);
    QmiMessageAtrSendInput *input = qmi_message_atr_send_input_new();
    qmi_message_atr_send_input_set_message(input, cmd_to_send, NULL);
    qmi_client_atr_send(atr_client, input, 10, NULL, (GAsyncReadyCallback)on_send_ready, NULL);
    qmi_message_atr_send_input_unref(input);

    g_timeout_add_seconds(3, on_timeout, NULL);
}

static void on_device_open(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    if (!qmi_device_open_finish(dev, res, &error)) {
        fprintf(stderr, "[ATR] Failed to open QMI device: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[ATR] QmiDevice opened. Allocating ATR client...\n");
    qmi_device_allocate_client(dev, QMI_SERVICE_ATR, QMI_CID_NONE, 10, NULL,
                               (GAsyncReadyCallback)on_atr_client_allocated, NULL);
}

static void on_device_new(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    qmi_dev = qmi_device_new_from_node_finish(res, &error);
    if (!qmi_dev) {
        fprintf(stderr, "[ATR] Failed to create QmiDevice: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    qmi_device_open(qmi_dev, QMI_DEVICE_OPEN_FLAGS_AUTO | QMI_DEVICE_OPEN_FLAGS_EXPECT_INDICATIONS,
                    10, NULL, (GAsyncReadyCallback)on_device_open, NULL);
}

static void on_qrtr_bus_ready(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QrtrBus *bus = qrtr_bus_new_finish(res, &error);
    if (!bus) {
        fprintf(stderr, "[ATR] QRTR bus failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    QrtrNode *node = qrtr_bus_peek_node(bus, 0);
    qmi_device_new_from_node(node, NULL, (GAsyncReadyCallback)on_device_new, NULL);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        cmd_to_send = argv[1];
    }
    loop = g_main_loop_new(NULL, FALSE);
    qrtr_bus_new(1000, NULL, (GAsyncReadyCallback)on_qrtr_bus_ready, NULL);
    g_main_loop_run(loop);
    return 0;
}
