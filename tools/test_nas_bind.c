#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <libqrtr-glib.h>

static GMainLoop *loop;
static QmiDevice *qmi_dev;
static QmiClientNas *nas_client;
static guint8 nas_cid = 0;
static guint16 tx_id = 1;
static int sub_id = 0;

typedef struct {
    const char *desc;
    gboolean has_srv_domain;
    QmiNasServiceDomainPreference srv_domain;
    gboolean has_duration;
    QmiNasChangeDuration duration;
    gboolean has_mode_pref;
    QmiNasRatModePreference mode_pref;
    gboolean use_raw;
    guint8 raw_tlv_id;
    guint32 raw_tlv_val;
} TestCase;

static TestCase test_cases[] = {
    { "service_domain=CS_PS (2), NO duration", TRUE, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_CS_PS, FALSE, 0, FALSE, 0, FALSE },
    { "service_domain=CS_PS (2), duration=PERMANENT", TRUE, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_CS_PS, TRUE, QMI_NAS_CHANGE_DURATION_PERMANENT, FALSE, 0, FALSE },
    { "service_domain=PS_ATTACH (3), NO duration", TRUE, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ATTACH, FALSE, 0, FALSE, 0, FALSE },
    { "service_domain=PS_ATTACH (3), duration=PERMANENT", TRUE, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ATTACH, TRUE, QMI_NAS_CHANGE_DURATION_PERMANENT, FALSE, 0, FALSE },
    { "mode_pref=GSM|UMTS|LTE, NO duration", FALSE, 0, FALSE, 0, TRUE, QMI_NAS_RAT_MODE_PREFERENCE_GSM | QMI_NAS_RAT_MODE_PREFERENCE_UMTS | QMI_NAS_RAT_MODE_PREFERENCE_LTE, FALSE },
    { "mode_pref=GSM|UMTS|LTE + CS_PS", TRUE, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_CS_PS, FALSE, 0, TRUE, QMI_NAS_RAT_MODE_PREFERENCE_GSM | QMI_NAS_RAT_MODE_PREFERENCE_UMTS | QMI_NAS_RAT_MODE_PREFERENCE_LTE, FALSE },
    { "mode_pref=GSM|UMTS|LTE + PS_ATTACH", TRUE, QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ATTACH, FALSE, 0, TRUE, QMI_NAS_RAT_MODE_PREFERENCE_GSM | QMI_NAS_RAT_MODE_PREFERENCE_UMTS | QMI_NAS_RAT_MODE_PREFERENCE_LTE, FALSE },
    { "Raw TLV 0x18 (srv_domain)=2 (CS_PS)", FALSE, 0, FALSE, 0, FALSE, 0, TRUE, 0x18, 2 },
    { "Raw TLV 0x18 (srv_domain)=3 (PS_ATTACH)", FALSE, 0, FALSE, 0, FALSE, 0, TRUE, 0x18, 3 },
    { "Raw TLV 0x12 (alt srv_domain)=3", FALSE, 0, FALSE, 0, FALSE, 0, TRUE, 0x12, 3 },
    { NULL }
};

static int current_case = 0;

static void run_next_case(void);

static void on_raw_cmd_ready(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessage *resp = qmi_device_command_full_finish(dev, res, &error);
    if (!resp) {
        printf("  -> RAW CMD ERROR: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        gsize raw_len = 0;
        const guint8 *raw = qmi_message_get_raw(resp, &raw_len, NULL);
        printf("  -> RAW CMD RESPONSE (len=%zu): ", raw_len);
        if (raw) {
            for (gsize i = 0; i < raw_len; i++) printf("%02x ", raw[i]);
            printf("\n");
        }
        qmi_message_unref(resp);
    }
    current_case++;
    run_next_case();
}

static void on_libqmi_pref_ready(QmiClientNas *c, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessageNasSetSystemSelectionPreferenceOutput *out =
        qmi_client_nas_set_system_selection_preference_finish(c, res, &error);
    if (!out) {
        printf("  -> LIBQMI FINISH ERROR: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
    } else {
        if (qmi_message_nas_set_system_selection_preference_output_get_result(out, &error)) {
            printf("  -> *** LIBQMI RESULT: SUCCESS! ***\n");
        } else {
            printf("  -> LIBQMI RESULT ERROR: %s (code=%d)\n",
                   error ? error->message : "unknown", error ? error->code : 0);
            g_clear_error(&error);
        }
        qmi_message_nas_set_system_selection_preference_output_unref(out);
    }
    current_case++;
    run_next_case();
}

static void run_next_case(void) {
    if (!test_cases[current_case].desc) {
        printf("\n=== All test cases finished! ===\n");
        g_main_loop_quit(loop);
        return;
    }

    TestCase *tc = &test_cases[current_case];
    printf("[%d] Testing: %s...\n", current_case, tc->desc);

    if (tc->use_raw) {
        QmiMessage *req = qmi_message_new(QMI_SERVICE_NAS, nas_cid, tx_id++, 0x0033);
        guint32 val = tc->raw_tlv_val;
        qmi_message_add_raw_tlv(req, tc->raw_tlv_id, (const guint8 *)&val, sizeof(val), NULL);
        qmi_device_command_full(qmi_dev, req, NULL, 10, NULL, (GAsyncReadyCallback)on_raw_cmd_ready, NULL);
        qmi_message_unref(req);
    } else {
        QmiMessageNasSetSystemSelectionPreferenceInput *in = qmi_message_nas_set_system_selection_preference_input_new();
        if (tc->has_srv_domain)
            qmi_message_nas_set_system_selection_preference_input_set_service_domain_preference(in, tc->srv_domain, NULL);
        if (tc->has_duration)
            qmi_message_nas_set_system_selection_preference_input_set_change_duration(in, tc->duration, NULL);
        if (tc->has_mode_pref)
            qmi_message_nas_set_system_selection_preference_input_set_mode_preference(in, tc->mode_pref, NULL);

        qmi_client_nas_set_system_selection_preference(
            nas_client, in, 10, NULL, (GAsyncReadyCallback)on_libqmi_pref_ready, NULL);
        qmi_message_nas_set_system_selection_preference_input_unref(in);
    }
}

static void on_bind_sub_resp(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiMessage *resp = qmi_device_command_full_finish(dev, res, &error);
    if (!resp) {
        printf("[NAS] bind_subscription(sub=%d) FAILED: %s\n", sub_id, error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[NAS] bind_subscription(sub=%d) ACCEPTED!\n\n", sub_id);
    qmi_message_unref(resp);

    run_next_case();
}

static void on_nas_client(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QmiClient *client = qmi_device_allocate_client_finish(dev, res, &error);
    if (!client) {
        printf("[NAS] Allocate client failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    nas_client = QMI_CLIENT_NAS(client);
    nas_cid = qmi_client_get_cid(client);
    printf("[NAS] Allocated NAS client with CID: %u\n", nas_cid);

    printf("[NAS] Binding client to subscription %d via 0x0045...\n", sub_id);
    QmiMessage *req = qmi_message_new(QMI_SERVICE_NAS, nas_cid, tx_id++, 0x0045);
    guint8 val = (guint8)sub_id;
    qmi_message_add_raw_tlv(req, 0x01, (const guint8 *)&val, sizeof(val), NULL);
    qmi_device_command_full(qmi_dev, req, NULL, 10, NULL, (GAsyncReadyCallback)on_bind_sub_resp, NULL);
    qmi_message_unref(req);
}

static void on_dev_open(QmiDevice *dev, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    if (!qmi_device_open_finish(dev, res, &error)) {
        printf("[NAS] Open device failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    printf("[NAS] QmiDevice open. Allocating NAS client...\n");
    qmi_device_allocate_client(dev, QMI_SERVICE_NAS, QMI_CID_NONE, 10, NULL,
                               (GAsyncReadyCallback)on_nas_client, NULL);
}

static void on_dev_new(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    qmi_dev = qmi_device_new_from_node_finish(res, &error);
    if (!qmi_dev) {
        printf("[NAS] Device new failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    qmi_device_open(qmi_dev, QMI_DEVICE_OPEN_FLAGS_AUTO, 10, NULL, (GAsyncReadyCallback)on_dev_open, NULL);
}

static void on_bus_ready(GObject *unused, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    QrtrBus *bus = qrtr_bus_new_finish(res, &error);
    if (!bus) {
        printf("[NAS] Bus failed: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        g_main_loop_quit(loop);
        return;
    }
    QrtrNode *node = qrtr_bus_get_node(bus, 0);
    qmi_device_new_from_node(node, NULL, (GAsyncReadyCallback)on_dev_new, NULL);
}

int main(int argc, char **argv) {
    if (argc > 1) sub_id = atoi(argv[1]);
    printf("Starting NAS preference test matrix on sub=%d...\n", sub_id);

    loop = g_main_loop_new(NULL, FALSE);
    qrtr_bus_new(1000, NULL, (GAsyncReadyCallback)on_bus_ready, NULL);
    g_main_loop_run(loop);
    return 0;
}
