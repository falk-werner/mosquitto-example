#include <mosquitto.h>

#include <getopt.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MQTT_DEFAULT_PORT (1883)
#define MQTT_KEEPALIVE    (60 * 1000)
#define MQTT_QOS             (0)

enum command {
    COMMAND_PUB,
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
    char * message;
    bool retain;
    enum command cmd;
    int exit_code;
};

static void print_usage(void)
{
    printf(
        "mqtt_pub, (c) 2024 Falk Werner <github.com/falk-werner>\n"
        "Publish message to MQTT topic\n"
        "\n"
        "Usage:\n"
        "    mqtt_pub [-h host] [-p port] [-u user] [-P password]\n"
        "             [-i client-id] [-r]\n"
        "             -t topic -m message\n"
        "\n"
        "Options:\n"
        "    -h, --host     : hostname of MQTT broker (default: localhost)\n"
        "    -p, --port     : port of MQTT broker (default: 1883)\n"
        "    -u, --user     : name of the MQTT user (default: <unset>)\n"
        "    -p, --password : password of the MQTT user (default: <unset>)\n"
        "    -i, --client-id: MQTT client id (default: <unset>)\n"
        "    -r, --retain   : retain message (default: message is not retained)\n"
        "    -t, --topic    : MQTT topic to publish (required)\n"
        "    -m, --message  : message to public (required)\n"
        "\n"
        "Example:\n"
        "    mqtt_pub -t test -m hello\n"
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
    ctx->message = NULL;
    ctx->retain = false;
    ctx->cmd = COMMAND_PUB;
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
        {"message", no_argument, 0, 'm'},
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
            case 'm':
                free(ctx->message);
                ctx->message = strdup(optarg);
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

    if ((ctx->cmd == COMMAND_PUB) &&
        ((ctx->topic == NULL) || ((ctx->message == NULL))) )
    {
        fprintf(stderr, "error: topic or message not specified\n");
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
    free(ctx->message);

    return ctx->exit_code;
}

static void mqtt_pub(struct context * ctx)
{
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

    rc = mosquitto_connect(mosq, ctx->host, ctx->port, MQTT_KEEPALIVE);
    if (MOSQ_ERR_SUCCESS != rc)
    {
        fprintf(stderr, "error: failed to connect to MQTT broker\n");
        ctx->exit_code = EXIT_FAILURE;
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return;
    }

    int const message_length = (int) strlen(ctx->message);
    rc = mosquitto_publish(mosq, NULL, ctx->topic,
        message_length, ctx->message, MQTT_QOS, ctx->retain);
    if (MOSQ_ERR_SUCCESS != rc)
    {
        fprintf(stderr, "error: failed to publish message\n");
        ctx->exit_code = EXIT_FAILURE;
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
        case COMMAND_PUB:
            mqtt_pub(&ctx);
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