#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <libqrtr-glib.h>

typedef struct {
    GMainLoop *loop;
    QrtrBus *bus;
    QmiDevice *device;
    QmiClientNas *nas_client;
} ToolCtx;

static ToolCtx ctx;

static void on_serving_system_ready(QmiClientNas *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageNasGetServingSystemOutput *output = qmi_client_nas_get_serving_system_finish(client, res, &error);
    if (!output) {
        printf("[NAS] get_serving_system failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        QmiNasRegistrationState reg_state;
        QmiNasAttachState cs_attach, ps_attach;
        QmiNasNetworkType selected_network;
        GArray *radio_interfaces = NULL;

        if (qmi_message_nas_get_serving_system_output_get_serving_system(
                output, &reg_state, &cs_attach, &ps_attach, &selected_network, &radio_interfaces, NULL)) {
            printf("[NAS] Serving System State:\n");
            printf("      Registration: %u (%s)\n", reg_state, qmi_nas_registration_state_get_string(reg_state));
            printf("      CS domain:    %u (%s)\n", cs_attach, qmi_nas_attach_state_get_string(cs_attach));
            printf("      PS domain:    %u (%s)\n", ps_attach, qmi_nas_attach_state_get_string(ps_attach));
        }
        qmi_message_nas_get_serving_system_output_unref(output);
    }
    g_main_loop_quit(ctx.loop);
}

static void on_attach_ready(QmiClientNas *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageNasAttachDetachOutput *output = qmi_client_nas_attach_detach_finish(client, res, &error);
    if (!output) {
        printf("[NAS] attach_detach finish failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        if (!qmi_message_nas_attach_detach_output_get_result(output, &error)) {
            printf("[NAS] attach_detach result error: %s\n", error ? error->message : "unknown");
            g_clear_error(&error);
        } else {
            printf("[NAS] *** PS ATTACH COMMAND ACCEPTED BY MODEM! ***\n");
        }
        qmi_message_nas_attach_detach_output_unref(output);
    }

    printf("[NAS] Querying serving system...\n");
    qmi_client_nas_get_serving_system(ctx.nas_client, NULL, 10, NULL, (GAsyncReadyCallback)on_serving_system_ready, NULL);
}

static void on_set_sys_pref_ready(QmiClientNas *client, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageNasSetSystemSelectionPreferenceOutput *output =
        qmi_client_nas_set_system_selection_preference_finish(client, res, &error);
    if (!output) {
        printf("[NAS] set_system_selection_preference finish failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        if (!qmi_message_nas_set_system_selection_preference_output_get_result(output, &error)) {
            printf("[NAS] set_system_selection_preference result error: %s\n", error ? error->message : "unknown");
            g_clear_error(&error);
        } else {
            printf("[NAS] *** SYSTEM SELECTION PREFERENCE (Data-Centric + PS Attach) SET! ***\n");
        }
        qmi_message_nas_set_system_selection_preference_output_unref(output);
    }

    printf("[NAS] Sending PS domain ATTACH request...\n");
    QmiMessageNasAttachDetachInput *attach_in = qmi_message_nas_attach_detach_input_new();
    qmi_message_nas_attach_detach_input_set_action(attach_in, QMI_NAS_PS_ATTACH_ACTION_ATTACH, NULL);
    qmi_client_nas_attach_detach(ctx.nas_client, attach_in, 10, NULL, (GAsyncReadyCallback)on_attach_ready, NULL);
    qmi_message_nas_attach_detach_input_unref(attach_in);
}

static void on_nas_client_created(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiClient *client = qmi_device_allocate_client_finish(dev, res, &error);
    if (!client) {
        fprintf(stderr, "[NAS] Allocate NAS client failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    ctx.nas_client = QMI_CLIENT_NAS(client);
    printf("[NAS] Connected to NAS service on modem!\n");

    printf("[NAS] Setting System Selection Preference (Data-Centric)...\n");
    QmiMessageNasSetSystemSelectionPreferenceInput *input = qmi_message_nas_set_system_selection_preference_input_new();
    qmi_message_nas_set_system_selection_preference_input_set_usage_preference(
        input, QMI_NAS_USAGE_PREFERENCE_DATA_CENTRIC, NULL);
    qmi_message_nas_set_system_selection_preference_input_set_service_domain_preference(
        input, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ATTACH, NULL);
    qmi_message_nas_set_system_selection_preference_input_set_change_duration(
        input, QMI_NAS_CHANGE_DURATION_PERMANENT, NULL);

    qmi_client_nas_set_system_selection_preference(
        ctx.nas_client, input, 10, NULL, (GAsyncReadyCallback)on_set_sys_pref_ready, NULL);
    qmi_message_nas_set_system_selection_preference_input_unref(input);
}

static void on_device_open(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    if (!qmi_device_open_finish(dev, res, &error)) {
        fprintf(stderr, "[NAS] Failed to open QMI device: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[NAS] Device opened. Allocating NAS client...\n");
    qmi_device_allocate_client(dev, QMI_SERVICE_NAS, QMI_CID_NONE, 10, NULL,
                               (GAsyncReadyCallback)on_nas_client_created, NULL);
}

static void on_device_new(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    ctx.device = qmi_device_new_from_node_finish(res, &error);
    if (!ctx.device) {
        fprintf(stderr, "[NAS] Failed to create QmiDevice from node: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[NAS] Created QmiDevice. Opening with EXPECT_INDICATIONS...\n");
    qmi_device_open(ctx.device, QMI_DEVICE_OPEN_FLAGS_AUTO | QMI_DEVICE_OPEN_FLAGS_EXPECT_INDICATIONS,
                    10, NULL, (GAsyncReadyCallback)on_device_open, NULL);
}

static void on_qrtr_bus_ready(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    ctx.bus = qrtr_bus_new_finish(res, &error);
    if (!ctx.bus) {
        fprintf(stderr, "[NAS] Failed to connect to QRTR bus: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(ctx.loop);
        return;
    }
    QrtrNode *node = qrtr_bus_peek_node(ctx.bus, 0);
    if (!node) {
        fprintf(stderr, "[NAS] Node 0 not found on QRTR bus\n");
        g_main_loop_quit(ctx.loop);
        return;
    }
    printf("[NAS] Found Node 0. Initializing QmiDevice...\n");
    qmi_device_new_from_node(node, NULL, (GAsyncReadyCallback)on_device_new, NULL);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("[NAS] Connecting to QRTR bus...\n");
    ctx.loop = g_main_loop_new(NULL, FALSE);

    qrtr_bus_new(1000, NULL, (GAsyncReadyCallback)on_qrtr_bus_ready, NULL);
    g_main_loop_run(ctx.loop);

    if (ctx.nas_client) g_object_unref(ctx.nas_client);
    if (ctx.device) g_object_unref(ctx.device);
    if (ctx.bus) g_object_unref(ctx.bus);
    g_main_loop_unref(ctx.loop);
    return 0;
}
