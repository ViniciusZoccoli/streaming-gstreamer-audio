#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <signal.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

static GMainLoop *loop = NULL;
static GstElement *jitterbuffer = NULL;
static FILE *log_file = NULL;
static gint64 inicio_us = 0;

#define SEGUNDOS_SEM_PACOTES 3

static GstElement *pipeline_global = NULL;
static gboolean gravando = FALSE;
static gboolean parando = FALSE;

static gboolean forcar_saida(gpointer data) {
    (void) data;
    if (loop) g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

/* Encerra o receptor. Se estiver gravando WAV, primeiro envia EOS para a
 * pipeline: o wavenc so escreve o cabecalho final do arquivo ao receber EOS.
 * Quando o EOS chega ao bus, bus_call encerra o loop. Se demorar mais de
 * 3 s, encerra de qualquer jeito. */
static void solicitar_parada(void) {
    if (parando) return;
    parando = TRUE;
    if (gravando && pipeline_global) {
        g_print("Finalizando a gravacao do arquivo WAV...\n");
        gst_element_send_event(pipeline_global, gst_event_new_eos());
        g_timeout_add_seconds(3, forcar_saida, NULL);
    } else if (loop) {
        g_main_loop_quit(loop);
    }
}

/* Ctrl+C: no Linux usamos g_unix_signal_add (forma segura do GLib);
 * no Windows, signal() roda o tratador em outra thread, e as funcoes
 * chamadas (g_main_loop_quit, gst_element_send_event) sao thread-safe. */
#ifdef G_OS_UNIX
static gboolean on_sigint(gpointer data) {
    (void) data;
    g_print("\nEncerrando receptor (Ctrl+C)...\n");
    solicitar_parada();
    return G_SOURCE_REMOVE;
}
#else
static void handle_sigint(int sig) {
    (void) sig;
    g_print("\nEncerrando receptor (Ctrl+C)...\n");
    solicitar_parada();
}
#endif

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
    (void) bus;
    GMainLoop *mloop = (GMainLoop *) data;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug_info;
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("ERRO no receptor: %s\n", err->message);
            g_printerr("  detalhes: %s\n", debug_info ? debug_info : "nenhum");
            g_error_free(err);
            g_free(debug_info);
            g_main_loop_quit(mloop);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("Fim de fluxo (EOS) recebido.\n");
            g_main_loop_quit(mloop);
            break;
        default:
            break;
    }
    return TRUE;
}

static gboolean print_jitterbuffer_stats(gpointer data) {
    (void) data;
    if (!jitterbuffer) return TRUE;

    GstStructure *stats = NULL;
    g_object_get(jitterbuffer, "stats", &stats, NULL);
    if (stats) {
        guint64 num_pushed = 0, num_lost = 0, num_late = 0, num_duplicates = 0;
        gst_structure_get_uint64(stats, "num-pushed", &num_pushed);
        gst_structure_get_uint64(stats, "num-lost", &num_lost);
        gst_structure_get_uint64(stats, "num-late", &num_late);
        gst_structure_get_uint64(stats, "num-duplicates", &num_duplicates);

        gint64 segundos = (g_get_monotonic_time() - inicio_us) / G_USEC_PER_SEC;

        g_print("[%3" G_GINT64_FORMAT " s] [rtpjitterbuffer] entregues=%" G_GUINT64_FORMAT
                " perdidos=%" G_GUINT64_FORMAT
                " atrasados_descartados=%" G_GUINT64_FORMAT
                " duplicados=%" G_GUINT64_FORMAT "\n",
                segundos, num_pushed, num_lost, num_late, num_duplicates);

        /* Grava a mesma linha em CSV (separador ';', abre direto no Excel) */
        if (log_file) {
            fprintf(log_file, "%" G_GINT64_FORMAT ";%" G_GUINT64_FORMAT ";%" G_GUINT64_FORMAT
                    ";%" G_GUINT64_FORMAT ";%" G_GUINT64_FORMAT "\n",
                    segundos, num_pushed, num_lost, num_late, num_duplicates);
            fflush(log_file);
        }

        gst_structure_free(stats);

        /* Encerramento automatico: se ja recebeu audio e o contador de
         * entregues ficou parado por SEGUNDOS_SEM_PACOTES, o transmissor
         * terminou. Necessario porque no terminal do MSYS2 o Ctrl+C nem
         * sempre chega ao programa. */
        static guint64 ultimo_pushed = 0;
        static int segundos_parado = 0;
        if (num_pushed > 0 && num_pushed == ultimo_pushed) {
            segundos_parado++;
        } else {
            segundos_parado = 0;
        }
        ultimo_pushed = num_pushed;

        if (segundos_parado >= SEGUNDOS_SEM_PACOTES && !parando && loop &&
            g_main_loop_is_running(loop)) {
            g_print("Nenhum pacote novo ha %d s: transmissao terminou, encerrando.\n",
                    SEGUNDOS_SEM_PACOTES);
            solicitar_parada();
        }
    }
    return TRUE;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    const char *port = (argc > 1) ? argv[1] : "5000";
    const char *latency = (argc > 2) ? argv[2] : "200";
    const char *log_path = (argc > 3) ? argv[3] : NULL;
    const char *wav_path = (argc > 4) ? argv[4] : NULL;
    gravando = (wav_path != NULL);

    /* Com gravacao, o audio reconstruido passa por um "tee" que divide o
     * fluxo em dois ramos: um toca no alto-falante e outro grava em WAV.
     * O arquivo e definido depois via g_object_set (evita problemas com
     * barras invertidas de caminhos do Windows dentro da string). */
    gchar *launch_str = g_strdup_printf(
        "udpsrc port=%s "
        "caps=\"application/x-rtp,media=audio,clock-rate=44100,"
        "encoding-name=L16,channels=2,payload=96\" ! "
        "rtpjitterbuffer name=jitbuf latency=%s ! "
        "rtpL16depay ! audioconvert ! %s"
        "audioresample ! autoaudiosink sync=false%s",
        port, latency,
        gravando ? "tee name=t ! queue ! " : "",
        gravando ? " t. ! queue ! audioconvert ! wavenc ! filesink name=gravador" : "");

    g_print("Receptor - escutando RTP/UDP na porta %s (latencia do jitterbuffer: %s ms)\n",
            port, latency);
    g_print("Pipeline: %s\n", launch_str);

    GError *parse_err = NULL;
    GstElement *pipeline = gst_parse_launch(launch_str, &parse_err);
    g_free(launch_str);

    if (!pipeline) {
        g_printerr("ERRO ao criar a pipeline do receptor: %s\n",
                   parse_err ? parse_err->message : "motivo nao aparente");
        if (parse_err) g_error_free(parse_err);
        return -1;
    }

    jitterbuffer = gst_bin_get_by_name(GST_BIN(pipeline), "jitbuf");
    pipeline_global = pipeline;

    if (gravando) {
        GstElement *gravador = gst_bin_get_by_name(GST_BIN(pipeline), "gravador");
        g_object_set(gravador, "location", wav_path, NULL);
        gst_object_unref(gravador);
        g_print("O audio recebido sera gravado em: %s\n", wav_path);
    }

    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("ERRO: o receptor nao conseguiu ir para o estado PLAYING.\n");
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        if (jitterbuffer) gst_object_unref(jitterbuffer);
        return -1;
    }

    if (log_path) {
        log_file = fopen(log_path, "w");
        if (!log_file) {
            g_printerr("ERRO: nao foi possivel criar o arquivo de log '%s'.\n", log_path);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            if (jitterbuffer) gst_object_unref(jitterbuffer);
            return -1;
        }
        fprintf(log_file, "tempo_s;entregues;perdidos;atrasados_descartados;duplicados\n");
        g_print("Estatisticas tambem serao gravadas em: %s\n", log_path);
    }

    loop = g_main_loop_new(NULL, FALSE);
    inicio_us = g_get_monotonic_time();

    GstBus *bus = gst_element_get_bus(pipeline);
    guint bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    guint stats_timeout_id = g_timeout_add_seconds(1, print_jitterbuffer_stats, NULL);

#ifdef G_OS_UNIX
    g_unix_signal_add(SIGINT, on_sigint, NULL);
#else
    signal(SIGINT, handle_sigint);
#endif

    g_print("Receptor em execucao. Pressione Ctrl+C para encerrar.\n");
    g_main_loop_run(loop);

    g_print("Resumo final:\n");
    print_jitterbuffer_stats(NULL);

    g_source_remove(stats_timeout_id);
    g_source_remove(bus_watch_id);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    if (jitterbuffer) gst_object_unref(jitterbuffer);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    if (log_file) fclose(log_file);

    return 0;
}
