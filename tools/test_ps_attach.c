#include <stdio.h>
#include <glib.h>
#include <libqmi-glib.h>
#include <libqrtr-glib.h>

static GMainLoop *loop;

static void on_set(QmiClientNas *c, GAsyncResult *res, gpointer data) {
    GError *err = NULL;
    QmiMessageNasSetSystemSelectionPreferenceOutput *out =
        qmi_client_nas_set_system_selection_preference_finish(c, res, &err);
    if (!out) {
        printf("Finish err: %s\n", err ? err->message : "unknown");
    } else {
        if (!qmi_message_nas_set_system_selection_preference_output_get_result(out, &err)) {
            printf("Result err: %s (code=%d)\n", err ? err->message : "unknown", err ? err->code : 0);
        } else {
            printf("*** SUCCESS! service_domain_preference set to PS_ATTACH! ***\n");
        }
        qmi_message_nas_set_system_selection_preference_output_unref(out);
    }
    g_main_loop_quit(loop);
}

static void on_client(QmiDevice *dev, GAsyncResult *res, gpointer data) {
    QmiClientNas *c = QMI_CLIENT_NAS(qmi_device_allocate_client_finish(dev, res, NULL));
    QmiMessageNasSetSystemSelectionPreferenceInput *in = qmi_message_nas_set_system_selection_preference_input_new();
    qmi_message_nas_set_system_selection_preference_input_set_service_domain_preference(in, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ATTACH, NULL);
    qmi_message_nas_set_system_selection_preference_input_set_change_duration(in, QMI_NAS_CHANGE_DURATION_PERMANENT, NULL);
    qmi_client_nas_set_system_selection_preference(c, in, 10, NULL, (GAsyncReadyCallback)on_set, NULL);
    qmi_message_nas_set_system_selection_preference_input_unref(in);
}

static void on_open(QmiDevice *dev, GAsyncResult *res, gpointer data) {
    qmi_device_open_finish(dev, res, NULL);
    qmi_device_allocate_client(dev, QMI_SERVICE_NAS, QMI_CID_NONE, 10, NULL, (GAsyncReadyCallback)on_client, NULL);
}

static void on_dev(GObject *s, GAsyncResult *res, gpointer data) {
    QmiDevice *dev = qmi_device_new_from_node_finish(res, NULL);
    qmi_device_open(dev, QMI_DEVICE_OPEN_FLAGS_AUTO, 10, NULL, (GAsyncReadyCallback)on_open, NULL);
}

static void on_bus(GObject *s, GAsyncResult *res, gpointer data) {
    QrtrBus *bus = qrtr_bus_new_finish(res, NULL);
    QrtrNode *node = qrtr_bus_get_node(bus, 0);
    qmi_device_new_from_node(node, NULL, (GAsyncReadyCallback)on_dev, NULL);
}

int main(void) {
    loop = g_main_loop_new(NULL, FALSE);
    qrtr_bus_new(1000, NULL, (GAsyncReadyCallback)on_bus, NULL);
    g_main_loop_run(loop);
    return 0;
}
