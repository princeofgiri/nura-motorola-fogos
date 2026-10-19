#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <libqrtr-glib.h>

static GMainLoop *loop;
static QmiDevice *qmi_dev;
static QmiClient *dsd_client;
static guint32 target_sub = 0; // Default to sub 0 (Primary SIM)

static void print_message_tlvs(QmiMessage *msg) {
    gsize raw_len = 0;
    const guint8 *raw = qmi_message_get_raw(msg, &raw_len, NULL);
    printf("[DSD] MsgID: 0x%04x, Svc: %u, Len: %zu\n",
           qmi_message_get_message_id(msg),
           qmi_message_get_service(msg),
           raw_len);
    if (raw) {
        printf("[DSD] Raw hex: ");
        for (gsize i = 0; i < raw_len; i++) printf("%02x ", raw[i]);
        printf("\n");
    }
}

static void on_notify_ready(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessage *resp = qmi_device_command_full_finish(dev, res, &error);
    if (!resp) {
        printf("[DSD] notify_data_settings failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        printf("[DSD] *** notify_data_settings RESPONSE ***\n");
        print_message_tlvs(resp);
        qmi_message_unref(resp);
    }
    g_main_loop_quit(loop);
}

static void on_switch_dds_ready(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessage *resp = qmi_device_command_full_finish(dev, res, &error);
    if (!resp) {
        printf("[DSD] switch_dds failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        printf("[DSD] *** switch_dds RESPONSE ***\n");
        print_message_tlvs(resp);
        qmi_message_unref(resp);
    }

    printf("[DSD] Sending QMI_DSD_NOTIFY_DATA_SETTINGS_REQ (0x0034)...\n");
    QmiMessage *req = qmi_message_new(QMI_SERVICE_DSD,
                                      qmi_client_get_cid(dsd_client),
                                      3,
                                      0x0034);
    guint32 rat_pref = 1; // 1 = enabled
    qmi_message_add_raw_tlv(req, 0x10, (const guint8 *)&rat_pref, sizeof(rat_pref), NULL);
    qmi_device_command_full(qmi_dev, req, NULL, 10, NULL, (GAsyncReadyCallback)on_notify_ready, NULL);
    qmi_message_unref(req);
}

static void on_bind_sub_ready(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessage *resp = qmi_device_command_full_finish(dev, res, &error);
    if (!resp) {
        printf("[DSD] bind_subscription failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        printf("[DSD] *** bind_subscription RESPONSE ***\n");
        print_message_tlvs(resp);
        qmi_message_unref(resp);
    }

    printf("[DSD] Sending QMI_DSD_SWITCH_DDS_REQ (0x004e, sub=%u)...\n", target_sub);
    QmiMessage *req = qmi_message_new(QMI_SERVICE_DSD,
                                      qmi_client_get_cid(dsd_client),
                                      2,
                                      0x004e);
    qmi_message_add_raw_tlv(req, 0x01, (const guint8 *)&target_sub, sizeof(target_sub), NULL);
    qmi_device_command_full(qmi_dev, req, NULL, 10, NULL, (GAsyncReadyCallback)on_switch_dds_ready, NULL);
    qmi_message_unref(req);
}

static void on_get_dds_ready(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessage *resp = qmi_device_command_full_finish(dev, res, &error);
    if (!resp) {
        printf("[DSD] get_current_dds failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        printf("[DSD] *** get_current_dds RESPONSE ***\n");
        print_message_tlvs(resp);
        qmi_message_unref(resp);
    }

    printf("[DSD] Sending QMI_DSD_BIND_SUBSCRIPTION_REQ (0x0027, sub=%u)...\n", target_sub);
    QmiMessage *req = qmi_message_new(QMI_SERVICE_DSD,
                                      qmi_client_get_cid(dsd_client),
                                      1,
                                      0x0027);
    qmi_message_add_raw_tlv(req, 0x01, (const guint8 *)&target_sub, sizeof(target_sub), NULL);
    qmi_device_command_full(qmi_dev, req, NULL, 10, NULL, (GAsyncReadyCallback)on_bind_sub_ready, NULL);
    qmi_message_unref(req);
}

static void on_dsd_client_allocated(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    dsd_client = qmi_device_allocate_client_finish(dev, res, &error);
    if (!dsd_client) {
        fprintf(stderr, "[DSD] Allocate DSD client failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[DSD] Allocated DSD client with CID: %u\n", qmi_client_get_cid(dsd_client));

    printf("[DSD] Sending QMI_DSD_GET_CURRENT_DDS_REQ (0x004f)...\n");
    QmiMessage *req = qmi_message_new(QMI_SERVICE_DSD,
                                      qmi_client_get_cid(dsd_client),
                                      0,
                                      0x004f);
    qmi_device_command_full(qmi_dev, req, NULL, 10, NULL, (GAsyncReadyCallback)on_get_dds_ready, NULL);
    qmi_message_unref(req);
}

static void on_device_open(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    if (!qmi_device_open_finish(dev, res, &error)) {
        fprintf(stderr, "[DSD] Failed to open QMI device: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[DSD] QmiDevice opened. Allocating DSD client...\n");
    qmi_device_allocate_client(dev, QMI_SERVICE_DSD, QMI_CID_NONE, 10, NULL,
                               (GAsyncReadyCallback)on_dsd_client_allocated, NULL);
}

static void on_device_new(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    qmi_dev = qmi_device_new_from_node_finish(res, &error);
    if (!qmi_dev) {
        fprintf(stderr, "[DSD] Failed to create QmiDevice: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    qmi_device_open(qmi_dev, QMI_DEVICE_OPEN_FLAGS_AUTO, 10, NULL, (GAsyncReadyCallback)on_device_open, NULL);
}

static void on_qrtr_bus_ready(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QrtrBus *bus = qrtr_bus_new_finish(res, &error);
    if (!bus) {
        fprintf(stderr, "[DSD] QRTR bus failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    QrtrNode *node = qrtr_bus_peek_node(bus, 0);
    qmi_device_new_from_node(node, NULL, (GAsyncReadyCallback)on_device_new, NULL);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        target_sub = (guint32)atoi(argv[1]);
    }
    printf("[DSD] Operating on subscription: %u\n", target_sub);
    loop = g_main_loop_new(NULL, FALSE);
    qrtr_bus_new(1000, NULL, (GAsyncReadyCallback)on_qrtr_bus_ready, NULL);
    g_main_loop_run(loop);
    return 0;
}
