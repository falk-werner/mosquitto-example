#include <mosquitto.h>

#include <getopt.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>

#define LOOP_INTERVAL (1000)

#define MQTT_DEFAULT_PORT (1883)
#define MQTT_KEEPALIVE    (60 * 1000)
#define MQTT_QOS             (0)

enum command {
    COMMAND_SUB,
    COMMAND_SHOW_HELP
};

struct context
{
    char * id;
    char * user;
    char * password;
    char * host;
    int port;
    char * topic;
    bool retain;
    enum command cmd;
    int exit_code;
};

static void print_usage(void)
{
    printf(
        "mqtt_sub, (c) 2024 Falk Werner <github.com/falk-werner>\n"
        "Subscribe to a MQTT topic\n"
        "\n"
        "Usage:\n"
        "    mqtt_sub [-h host] [-p port] [-u user] [-P password]\n"
        "             [-i client-id] [-r] -t topic\n"
        "\n"
        "Options:\n"
        "    -h, --host     : hostname of MQTT broker (default: localhost)\n"
        "    -p, --port     : port of MQTT broker (default: 1883)\n"
        "    -u, --user     : name of the MQTT user (default: <unset>)\n"
        "    -p, --password : password of the MQTT user (default: <unset>)\n"
        "    -i, --client-id: MQTT client id (default: <unset>)\n"
        "    -r, --retain   : retain message (default: message is not retained)\n"
        "    -t, --topic    : MQTT topic to publish (required)\n"
        "\n"
        "Example:\n"
        "    mqtt_sub -t test\n"
    );
}

static void context_init(struct context * ctx, int argc, char* argv[])
{
    ctx->id = NULL;
    ctx->user = NULL;
    ctx->password = NULL;
    ctx->host = strdup("localhost");
    ctx->port = MQTT_DEFAULT_PORT;
    ctx->topic = NULL;
    ctx->retain = false;
    ctx->cmd = COMMAND_SUB;
    ctx->exit_code = EXIT_SUCCESS;

    optind = 0;
    opterr = 0;

    struct option const long_opts[] = {
        {"client-id", no_argument, 0, 'i'},
        {"host", no_argument, 0, 'h'},
        {"port", no_argument, 0, 'p'},
        {"user", no_argument, 0, 'u'},
        {"password", no_argument, 0, 'P'},
        {"retain", no_argument, 0, 'r'},        
        {"topic", no_argument, 0, 't'},
        {"help", no_argument, 0, 'H'},
        {NULL, 0, NULL, 0}
    };

    bool done = false;
    while (!done)
    {
        int option_index = 0;
        int const c = getopt_long(argc, argv, "i:h:p:u:P:rt:m:H", long_opts, &option_index);
        switch (c)
        {
            case -1:
                done = true;
                break;
            case 'i':
                free(ctx->id);
                ctx->id = strdup(optarg);
                break;
            case 'h':
                free(ctx->host);
                ctx->host = strdup(optarg);
                break;
            case 'p':
                ctx->port = atoi(optarg);
                break;
            case 'u':
                free(ctx->user);
                ctx->user = strdup(optarg);
                break;
            case 'P':
                free(ctx->password);
                ctx->password = strdup(optarg);
                break;
            case 'r':
                ctx->retain = true;
                break;
            case 't':
                free(ctx->topic);
                ctx->topic = strdup(optarg);
                break;
            case 'H':
                ctx->cmd = COMMAND_SHOW_HELP;
                done = true;
                break;
            default:
                fprintf(stderr, "error: unknown option\n");
                ctx->exit_code = EXIT_FAILURE;
                ctx->cmd = COMMAND_SHOW_HELP;
                done = true;
        }
    }

    if ((ctx->cmd == COMMAND_SUB) && (ctx->topic == NULL))
    {
        fprintf(stderr, "error: missing topic\n");
        ctx->exit_code = EXIT_FAILURE;
        ctx->cmd = COMMAND_SHOW_HELP;
    }

}

static int context_cleanup(struct context * ctx)
{
    free(ctx->id);
    free(ctx->user);
    free(ctx->password);
    free(ctx->host);
    free(ctx->topic);

    return ctx->exit_code;
}

static void mqtt_on_message(struct mosquitto * mosq,
    void * user_data, struct mosquitto_message const * message)
{
    (void) mosq; // unused
    (void) user_data; // unused

    printf("message id: %d\n", message->mid);
    printf("topic     : %s\n", message->topic);
    printf("retained  : %s\n", message->retain ? "yes" : "no");

    if (0 < message->payloadlen)
    {
        printf("payload   : %.*s\n", message->payloadlen, (char const*) message->payload);
    }
    else
    {
        printf("payload   : <empty>\n");
    }

    printf("\n");
}

static bool g_shutdown_requested = false;
static void on_shutdown_requested(int signal_number)
{
    (void) signal_number; // ignored;
    g_shutdown_requested = true;
}

static void mqtt_sub(struct context * ctx)
{
    signal(SIGINT, &on_shutdown_requested);
    signal(SIGTERM, &on_shutdown_requested);

    int rc = mosquitto_lib_init();
    if (MOSQ_ERR_SUCCESS != rc)
    {
        fprintf(stderr, "error: failed to init mosquitto library\n");
        ctx->exit_code = EXIT_FAILURE;
        return;
    }

    struct mosquitto * mosq = mosquitto_new(ctx->id, true, NULL);
    if (NULL == mosq)
    {
        fprintf(stderr, "error: failed to create mosquitto instance\n");
        ctx->exit_code = EXIT_FAILURE;
        mosquitto_lib_cleanup();
        return;
    }

    rc = mosquitto_username_pw_set(mosq, ctx->user, ctx->password);
    if (MOSQ_ERR_SUCCESS != rc)
    {
        fprintf(stderr, "error: failed to set user and password\n");
        ctx->exit_code = EXIT_FAILURE;
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return;
    }

    mosquitto_message_callback_set(mosq, &mqtt_on_message);

    rc = mosquitto_connect(mosq, ctx->host, ctx->port, MQTT_KEEPALIVE);
    if (MOSQ_ERR_SUCCESS != rc)
    {
        fprintf(stderr, "error: failed to connect to MQTT broker\n");
        ctx->exit_code = EXIT_FAILURE;
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return;
    }

    rc = mosquitto_subscribe(mosq, NULL, ctx->topic, MQTT_QOS);
    if (MOSQ_ERR_SUCCESS != rc)
    {
        fprintf(stderr, "error: failed to subscribe\n");
        ctx->exit_code = EXIT_FAILURE;
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return;
    }

    while ((rc == MOSQ_ERR_SUCCESS) && (!g_shutdown_requested))
    {
        rc = mosquitto_loop(mosq, LOOP_INTERVAL, 1);
    }

    if ((MOSQ_ERR_SUCCESS == rc) && (errno != EINTR))
    {
        fprintf(stderr, "error: failed to execute message loop; %s\n", strerror(errno));
        ctx->exit_code = EXIT_FAILURE;
    }


    rc = mosquitto_unsubscribe(mosq, NULL, ctx->topic);
    if (MOSQ_ERR_SUCCESS != rc)
    {
        fprintf(stderr, "warning: failed to unsubscribe\n");
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

int main(int argc, char* argv[])
{
    struct context ctx;
    context_init(&ctx, argc, argv);

    switch (ctx.cmd)
    {
        case COMMAND_SUB:
            mqtt_sub(&ctx);
            break;
        case COMMAND_SHOW_HELP:
            // fall-through
        default:
            print_usage();
            break;
    }

    int const exit_code = context_cleanup(&ctx);
    return exit_code;
}