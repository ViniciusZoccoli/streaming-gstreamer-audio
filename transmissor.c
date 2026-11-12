#include <gst/gst.h>
#include <glib.h>
#include <stdlib.h>

static int run_pipeline_and_wait(const char *description, const char *launch_str) {
    GError *parse_err = NULL;

    g_print("\n=== %s ===\n", description);
    g_print("Pipeline: %s\n", launch_str);

    GstElement *pipeline = gst_parse_launch(launch_str, &parse_err);
    if (!pipeline) {
        g_printerr("ERRO ao criar a pipeline '%s': %s\n", description,
                   parse_err ? parse_err->message : "motivo nao aparente");
        if (parse_err) g_error_free(parse_err);
        return -1;
    }

    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("ERRO: a pipeline '%s' nao conseguiu ir para o estado PLAYING.\n",
                   description);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return -1;
    }

    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    int exit_code = 0;
    if (msg != NULL) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError *err;
            gchar *debug_info;
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("ERRO durante a execucao de '%s': %s\n", description, err->message);
            g_printerr("  detalhes do erro: %s\n", debug_info ? debug_info : "nenhum");
            g_error_free(err);
            g_free(debug_info);
            exit_code = -1;
        } else {
            g_print("'%s' concluida com sucesso (EOS).\n", description);
        }
        gst_message_unref(msg);
    }

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return exit_code;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const char *port = (argc > 2) ? argv[2] : "5000";
    const char *num_buffers = (argc > 3) ? argv[3] : "500";

    /* Simulacao opcional de rede ruim (substitui o "tc netem" do Linux):
     * argv[4] = % de pacotes descartados, argv[5] = atraso maximo em ms.
     * Usa o elemento netsim (gst-plugins-bad), que funciona em qualquer SO. */
    double perda_pct = (argc > 4) ? g_ascii_strtod(argv[4], NULL) : 0.0;
    int atraso_max_ms = (argc > 5) ? atoi(argv[5]) : 0;

    if (perda_pct < 0.0 || perda_pct > 100.0 || atraso_max_ms < 0) {
        g_printerr("ERRO: perda deve estar entre 0 e 100 e atraso deve ser >= 0.\n");
        return -1;
    }

    gchar *simulacao;
    if (perda_pct > 0.0 || atraso_max_ms > 0) {
        gchar prob[G_ASCII_DTOSTR_BUF_SIZE];
        g_ascii_dtostr(prob, sizeof prob, perda_pct / 100.0);
        simulacao = g_strdup_printf(
            "netsim drop-probability=%s delay-probability=%s "
            "min-delay=0 max-delay=%d ! ",
            prob, atraso_max_ms > 0 ? "0.5" : "0", atraso_max_ms);
        g_print("Simulando rede ruim: %.1f%% de perda, atraso de ate %d ms "
                "em metade dos pacotes\n", perda_pct, atraso_max_ms);
    } else {
        simulacao = g_strdup("");
    }

    gchar *launch_str = g_strdup_printf(
        "audiotestsrc wave=sine freq=440 is-live=true num-buffers=%s ! "
        "audioconvert ! audioresample ! "
        "audio/x-raw,format=S16BE,rate=44100,channels=2 ! "
        "rtpL16pay ! "
        "%s"
        "udpsink host=%s port=%s",
        num_buffers, simulacao, host, port);
    g_free(simulacao);

    g_print("Transmissor - enviando audio RTP/UDP para %s:%s\n", host, port);

    int r = run_pipeline_and_wait("Transmissao de audio (RTP/UDP)", launch_str);

    g_free(launch_str);

    return r;
}
