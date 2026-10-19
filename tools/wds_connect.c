#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <libqrtr-glib.h>

static GMainLoop *loop;
static QmiDevice *qmi_dev;
static QmiClientWds *wds_client;

static QmiWdsIpFamily target_ip_family = QMI_WDS_IP_FAMILY_IPV4;
static int test_step = 0;

static void start_call(QmiWdsIpFamily ip_fam);

static void on_start_network_ready(QmiClientWds *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageWdsStartNetworkOutput *output = qmi_client_wds_start_network_finish(client, res, &error);

    if (output) {
        GError *res_error = NULL;
        if (qmi_message_wds_start_network_output_get_result(output, &res_error)) {
            guint32 handle = 0;
            qmi_message_wds_start_network_output_get_packet_data_handle(output, &handle, NULL);
            printf("\n======================================================\n");
            printf("*** SUCCESS! PACKET DATA CALL ESTABLISHED! ***\n");
            printf("Packet Data Handle: 0x%08x\n", handle);
            printf("======================================================\n\n");
            qmi_message_wds_start_network_output_unref(output);
            g_main_loop_quit(loop);
            return;
        } else {
            printf("  -> Start Network FAILED: %s\n", res_error ? res_error->message : "unknown");
        }

        QmiWdsCallEndReason cer = 0;
        if (qmi_message_wds_start_network_output_get_call_end_reason(output, &cer, NULL)) {
            printf("  -> Call End Reason: %u (%s)\n", cer, qmi_wds_call_end_reason_get_string(cer));
        }

        QmiWdsVerboseCallEndReasonType vtype;
        gint16 vreason;
        if (qmi_message_wds_start_network_output_get_verbose_call_end_reason(output, &vtype, &vreason, NULL)) {
            printf("  -> Verbose Call End Reason: type=%u (%s), reason=%d\n",
                   vtype, qmi_wds_verbose_call_end_reason_type_get_string(vtype), vreason);
        }
        qmi_message_wds_start_network_output_unref(output);
    } else {
        printf("  -> Start Network finish failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    }

    test_step++;
    if (test_step == 1) {
        printf("\n--- Retrying with IPv6 ---\n");
        start_call(QMI_WDS_IP_FAMILY_IPV6);
    } else if (test_step == 2) {
        printf("\n--- Retrying with UNSPECIFIED ---\n");
        start_call(QMI_WDS_IP_FAMILY_UNSPECIFIED);
    } else {
        printf("All attempts finished.\n");
        g_main_loop_quit(loop);
    }
}

static void on_set_ip_family_ready(QmiClientWds *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageWdsSetIpFamilyOutput *output = qmi_client_wds_set_ip_family_finish(client, res, &error);
    if (!output || !qmi_message_wds_set_ip_family_output_get_result(output, &error)) {
        printf("  -> set_ip_family failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        printf("  -> set_ip_family SUCCESS!\n");
    }
    if (output) qmi_message_wds_set_ip_family_output_unref(output);

    printf("[WDS] Starting network interface (APN: internet, IP Family: %s)...\n",
           qmi_wds_ip_family_get_string(target_ip_family));

    QmiMessageWdsStartNetworkInput *input = qmi_message_wds_start_network_input_new();
    qmi_message_wds_start_network_input_set_apn(input, "internet", NULL);
    qmi_message_wds_start_network_input_set_profile_index_3gpp(input, 1, NULL);
    if (target_ip_family != QMI_WDS_IP_FAMILY_UNSPECIFIED) {
        qmi_message_wds_start_network_input_set_ip_family_preference(input, target_ip_family, NULL);
    }

    qmi_client_wds_start_network(wds_client, input, 15, NULL, (GAsyncReadyCallback)on_start_network_ready, NULL);
    qmi_message_wds_start_network_input_unref(input);
}

static void start_call(QmiWdsIpFamily ip_fam) {
    target_ip_family = ip_fam;
    printf("[WDS] Setting Client IP Family to: %s (0x%02x)...\n",
           qmi_wds_ip_family_get_string(ip_fam), ip_fam);

    QmiMessageWdsSetIpFamilyInput *input = qmi_message_wds_set_ip_family_input_new();
    qmi_message_wds_set_ip_family_input_set_preference(input, ip_fam, NULL);
    qmi_client_wds_set_ip_family(wds_client, input, 10, NULL, (GAsyncReadyCallback)on_set_ip_family_ready, NULL);
    qmi_message_wds_set_ip_family_input_unref(input);
}

static void on_bind_mux_ready(QmiClientWds *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageWdsBindMuxDataPortOutput *output = qmi_client_wds_bind_mux_data_port_finish(client, res, &error);
    if (!output || !qmi_message_wds_bind_mux_data_port_output_get_result(output, &error)) {
        printf("[WDS] bind_mux_data_port failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[WDS] bind_mux_data_port SUCCESS! (ep=EMBEDDED, iface=1, mux_id=2)\n");
    qmi_message_wds_bind_mux_data_port_output_unref(output);

    start_call(QMI_WDS_IP_FAMILY_IPV4);
}

static void on_bind_sub_ready(QmiClientWds *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageWdsBindSubscriptionOutput *output = qmi_client_wds_bind_subscription_finish(client, res, &error);
    if (!output || !qmi_message_wds_bind_subscription_output_get_result(output, &error)) {
        printf("[WDS] bind_subscription failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[WDS] bind_subscription SUCCESS! (PRIMARY=1)\n");
    qmi_message_wds_bind_subscription_output_unref(output);

    printf("[WDS] Binding mux data port (EMBEDDED, iface=1, mux_id=2)...\n");
    QmiMessageWdsBindMuxDataPortInput *input = qmi_message_wds_bind_mux_data_port_input_new();
    qmi_message_wds_bind_mux_data_port_input_set_endpoint_info(input, QMI_DATA_ENDPOINT_TYPE_EMBEDDED, 1, NULL);
    qmi_message_wds_bind_mux_data_port_input_set_mux_id(input, 2, NULL);

    qmi_client_wds_bind_mux_data_port(wds_client, input, 10, NULL, (GAsyncReadyCallback)on_bind_mux_ready, NULL);
    qmi_message_wds_bind_mux_data_port_input_unref(input);
}

static void on_wds_client_ready(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    wds_client = QMI_CLIENT_WDS(qmi_device_allocate_client_finish(dev, res, &error));
    if (!wds_client) {
        printf("[WDS] Allocate WDS client failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[WDS] Allocated WDS client (CID: %u)\n", qmi_client_get_cid(QMI_CLIENT(wds_client)));

    printf("[WDS] Binding subscription (PRIMARY=1)...\n");
    QmiMessageWdsBindSubscriptionInput *input = qmi_message_wds_bind_subscription_input_new();
    qmi_message_wds_bind_subscription_input_set_subscription_id(input, QMI_SUBSCRIPTION_TYPE_PRIMARY, NULL);
    qmi_client_wds_bind_subscription(wds_client, input, 10, NULL, (GAsyncReadyCallback)on_bind_sub_ready, NULL);
    qmi_message_wds_bind_subscription_input_unref(input);
}

static void on_device_open(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    if (!qmi_device_open_finish(dev, res, &error)) {
        printf("[WDS] Failed to open QMI device: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[WDS] Device opened. Allocating WDS client...\n");
    qmi_device_allocate_client(dev, QMI_SERVICE_WDS, QMI_CID_NONE, 10, NULL, (GAsyncReadyCallback)on_wds_client_ready, NULL);
}

static void on_device_new(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    qmi_dev = qmi_device_new_from_node_finish(res, &error);
    if (!qmi_dev) {
        printf("[WDS] Failed to create QmiDevice: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[WDS] Opening QmiDevice...\n");
    qmi_device_open(qmi_dev, QMI_DEVICE_OPEN_FLAGS_AUTO, 10, NULL, (GAsyncReadyCallback)on_device_open, NULL);
}

static void on_qrtr_bus_ready(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QrtrBus *bus = qrtr_bus_new_finish(res, &error);
    if (!bus) {
        printf("[WDS] Failed to connect to QRTR: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    QrtrNode *node = qrtr_bus_peek_node(bus, 0);
    if (!node) {
        printf("[WDS] Node 0 not found\n");
        g_main_loop_quit(loop);
        return;
    }
    printf("[WDS] Found Node 0. Creating QmiDevice...\n");
    qmi_device_new_from_node(node, NULL, (GAsyncReadyCallback)on_device_new, NULL);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    loop = g_main_loop_new(NULL, FALSE);
    printf("[WDS] Connecting to QRTR bus...\n");
    qrtr_bus_new(1000, NULL, (GAsyncReadyCallback)on_qrtr_bus_ready, NULL);
    g_main_loop_run(loop);

    if (wds_client) g_object_unref(wds_client);
    if (qmi_dev) g_object_unref(qmi_dev);
    g_main_loop_unref(loop);
    return 0;
}
