#define _DEFAULT_SOURCE

#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <mosquitto.h>
#include <cjson/cJSON.h>

#define CONSUMER "qUPS-guard"
#define POLLINTERVAL 1000
#define PFO_DEBOUNCE_INTERVAL 50000
#define SHUTDOWN_DELAY 0

pthread_t g_thread, g_shdthread;

struct gpiod_chip *chip = NULL;
uint8_t lastval_pfo = 255, lastval_lim = 255;
struct timespec ts;
struct timespec start_time, test_time;
struct gpiod_line_request *in_request = NULL;
struct gpiod_line_request *shd_request = NULL;
struct gpiod_edge_event_buffer *evbuf = NULL;

static bool shutdown_enabled = true;
static uint8_t shutdown_delay = SHUTDOWN_DELAY;
static uint8_t shutdown_pulse = 0;
static uint8_t low_ups_suppressed = 0;
static struct timespec last_pfo_change;
static bool pfo_change_initialized = false;

static char chip_path[64] = "/dev/gpiochip0";
static struct mosquitto *g_mosq = NULL;
static bool mqtt_enabled = true;
static char mqtt_broker[128] = "127.0.0.1";
static int mqtt_port = 1883;
static char mqtt_user[64] = "";
static char mqtt_pass[64] = "";
static char node_id[64] = "qups_guard";
static char state_topic[128] = "qups/state";
static char discovery_prefix[64] = "homeassistant";

struct DIPsw
{
    const char *DIP;
    unsigned int pfo_n;
    unsigned int lim_n;
    unsigned int shd_n;
} DIP_sw;

char dip_sw[4] = "";
static bool dip_configured = false;

struct DIPsw DIPswa[10] = {
    {"10", 17, 27, 22}, {"01", 23, 24, 25}, {"11", 5, 6, 26},
    {"111", 4, 24, 23}, {"011", 14, 18, 15}, {"101", 25, 7, 8},
    {"001", 17, 22, 27}, {"110", 10, 11, 9}, {"010", 12, 20, 16},
    {"100", 19, 21, 26}};

double diffcltime(struct timespec a, struct timespec b)
{
    long long elapsed_nanoseconds = (b.tv_sec - a.tv_sec) * 1000000000LL +
                                    (b.tv_nsec - a.tv_nsec);
    return (double)(elapsed_nanoseconds / 1000.0);
}

static bool set_dip_switch(const char *code)
{
    size_t dip_len = strlen(code);
    if (dip_len != 2 && dip_len != 3)
        return false;

    strncpy(dip_sw, code, sizeof(dip_sw) - 1);
    dip_sw[sizeof(dip_sw) - 1] = '\0';

    for (size_t i = 0; i < sizeof(DIPswa) / sizeof(DIPswa[0]); ++i)
    {
        if (strcmp(dip_sw, DIPswa[i].DIP) == 0)
        {
            DIP_sw = DIPswa[i];
            dip_configured = true;
            return true;
        }
    }
    return false;
}

static void copy_config_string(char *destination, size_t destination_size,
                               const char *source)
{
    strncpy(destination, source, destination_size - 1);
    destination[destination_size - 1] = '\0';
}

static void load_config_file(const char *filepath)
{
    FILE *file = fopen(filepath, "rb");
    if (!file)
    {
        syslog(LOG_WARNING, "Config file %s not found. Using defaults/CLI flags.", filepath);
        return;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return;
    }

    char *data = malloc((size_t)length + 1);
    if (!data)
    {
        fclose(file);
        return;
    }

    size_t bytes_read = fread(data, 1, (size_t)length, file);
    fclose(file);
    data[bytes_read] = '\0';

    cJSON *json = cJSON_Parse(data);
    free(data);
    if (!json)
    {
        syslog(LOG_ERR, "JSON parse error in config file: %s",
               cJSON_GetErrorPtr());
        return;
    }

    cJSON *gpio = cJSON_GetObjectItemCaseSensitive(json, "gpio");
    if (gpio)
    {
        cJSON *dip = cJSON_GetObjectItemCaseSensitive(gpio, "dip");
        if (cJSON_IsString(dip) && dip->valuestring)
            set_dip_switch(dip->valuestring);

        cJSON *path = cJSON_GetObjectItemCaseSensitive(gpio, "chip_path");
        if (cJSON_IsString(path) && path->valuestring)
            copy_config_string(chip_path, sizeof(chip_path), path->valuestring);
    }

    cJSON *ups = cJSON_GetObjectItemCaseSensitive(json, "ups");
    if (ups)
    {
        cJSON *delay = cJSON_GetObjectItemCaseSensitive(ups, "shutdown_delay");
        if (cJSON_IsNumber(delay) && delay->valueint >= 0)
            shutdown_delay = (uint8_t)delay->valueint;
    }

    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(json, "mqtt");
    if (mqtt)
    {
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(mqtt, "enabled");
        if (cJSON_IsBool(enabled))
            mqtt_enabled = cJSON_IsTrue(enabled);

        cJSON *broker = cJSON_GetObjectItemCaseSensitive(mqtt, "broker");
        if (cJSON_IsString(broker) && broker->valuestring)
            copy_config_string(mqtt_broker, sizeof(mqtt_broker), broker->valuestring);

        cJSON *port = cJSON_GetObjectItemCaseSensitive(mqtt, "port");
        if (cJSON_IsNumber(port) && port->valueint > 0)
            mqtt_port = port->valueint;

        cJSON *username = cJSON_GetObjectItemCaseSensitive(mqtt, "username");
        if (cJSON_IsString(username) && username->valuestring)
            copy_config_string(mqtt_user, sizeof(mqtt_user), username->valuestring);

        cJSON *password = cJSON_GetObjectItemCaseSensitive(mqtt, "password");
        if (cJSON_IsString(password) && password->valuestring)
            copy_config_string(mqtt_pass, sizeof(mqtt_pass), password->valuestring);

        cJSON *node = cJSON_GetObjectItemCaseSensitive(mqtt, "node_id");
        if (cJSON_IsString(node) && node->valuestring)
            copy_config_string(node_id, sizeof(node_id), node->valuestring);

        cJSON *topic = cJSON_GetObjectItemCaseSensitive(mqtt, "state_topic");
        if (cJSON_IsString(topic) && topic->valuestring)
            copy_config_string(state_topic, sizeof(state_topic), topic->valuestring);

        cJSON *prefix = cJSON_GetObjectItemCaseSensitive(mqtt, "discovery_prefix");
        if (cJSON_IsString(prefix) && prefix->valuestring)
            copy_config_string(discovery_prefix, sizeof(discovery_prefix), prefix->valuestring);
    }

    cJSON_Delete(json);
    syslog(LOG_INFO, "Config file successfully parsed: %s", filepath);
}

static void publish_mqtt_state(const char *mains, const char *battery_low,
                               const char *status)
{
    if (!mqtt_enabled || !g_mosq)
        return;

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"mains_power\":\"%s\",\"battery_low\":\"%s\",\"status\":\"%s\"}",
             mains, battery_low, status);

    int result = mosquitto_publish(g_mosq, NULL, state_topic, strlen(payload),
                                   payload, 1, true);
    if (result != MOSQ_ERR_SUCCESS)
        syslog(LOG_WARNING, "MQTT publish failed: %s", mosquitto_strerror(result));
}

static void publish_ha_discovery(void)
{
    if (!mqtt_enabled || !g_mosq)
        return;

    const char *device =
        "\"device\":{" 
        "\"identifiers\":[\"aqex_qups_guard\"],"
        "\"name\":\"AQEX qUPS Guard\","
        "\"manufacturer\":\"AQEX Electronics\","
        "\"model\":\"qUPS Series\"}";
    char topic[256];
    char payload[1024];

    snprintf(topic, sizeof(topic), "%s/binary_sensor/%s/mains_power/config",
             discovery_prefix, node_id);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"qUPS Mains Power\",\"unique_id\":\"%s_mains_power\","
             "\"state_topic\":\"%s\",\"value_template\":\"{{ value_json.mains_power }}\","
             "\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
             "\"device_class\":\"plug\",%s}",
             node_id, state_topic, device);
    mosquitto_publish(g_mosq, NULL, topic, strlen(payload), payload, 1, true);

    snprintf(topic, sizeof(topic), "%s/binary_sensor/%s/battery_low/config",
             discovery_prefix, node_id);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"qUPS Energy level\",\"unique_id\":\"%s_battery_low\","
             "\"state_topic\":\"%s\",\"value_template\":\"{{ value_json.battery_low }}\","
             "\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
             "\"device_class\":\"problem\",%s}",
             node_id, state_topic, device);
    mosquitto_publish(g_mosq, NULL, topic, strlen(payload), payload, 1, true);

    snprintf(topic, sizeof(topic), "%s/sensor/%s/status/config", discovery_prefix, node_id);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"qUPS Status\",\"unique_id\":\"%s_status\","
             "\"state_topic\":\"%s\",\"value_template\":\"{{ value_json.status }}\","
             "\"icon\":\"mdi:uninterruptible-power-supply\",%s}",
             node_id, state_topic, device);
    mosquitto_publish(g_mosq, NULL, topic, strlen(payload), payload, 1, true);
}

static void mqtt_init(void)
{
    if (!mqtt_enabled)
        return;

    mosquitto_lib_init();
    g_mosq = mosquitto_new("qups-guard2-ha", true, NULL);
    if (!g_mosq)
    {
        syslog(LOG_ERR, "Failed to create Mosquitto instance.");
        return;
    }

    if (mqtt_user[0] != '\0')
        mosquitto_username_pw_set(g_mosq, mqtt_user, mqtt_pass);

    mosquitto_reconnect_delay_set(g_mosq, 2, 30, true);
    int result = mosquitto_connect(g_mosq, mqtt_broker, mqtt_port, 60);
    if (result != MOSQ_ERR_SUCCESS)
        syslog(LOG_WARNING, "Could not connect to MQTT broker: %s",
               mosquitto_strerror(result));

    if (mosquitto_loop_start(g_mosq) != MOSQ_ERR_SUCCESS)
    {
        syslog(LOG_ERR, "Failed to start MQTT loop.");
        mosquitto_destroy(g_mosq);
        g_mosq = NULL;
        mosquitto_lib_cleanup();
        return;
    }
    publish_ha_discovery();
}

void *g_shdcallback(void *args)
{
    (void)args;
    while (true)
    {
        if (shutdown_pulse)
        {
            clock_gettime(CLOCK_MONOTONIC, &test_time);
            double elapsed = diffcltime(start_time, test_time);
            if (elapsed > POLLINTERVAL)
            {
                publish_mqtt_state("OFF", "ON", "Shutting Down");
                sleep(1);
                sleep(shutdown_delay);
                syslog(LOG_INFO, "Shutdown delay expired, shutting down system.");

                if (shutdown_enabled)
                {
                    if (system("sudo shutdown -h now") == 0)
                        syslog(LOG_INFO, "Shutdown sequence successfully initiated.");
                }
                else
                {
                    syslog(LOG_INFO, "Shutdown sequence disabled by --noshutdown flag.");
                }
                shutdown_pulse = 0;
            }
        }
        fflush(stdout);
        usleep(POLLINTERVAL);
    }
}

void *g_callback(void *args)
{
    (void)args;
    const int buffer_capacity = 64;
    evbuf = gpiod_edge_event_buffer_new(buffer_capacity);
    if (!evbuf)
    {
        syslog(LOG_ERR, "Failed to allocate GPIO event buffer.");
        return NULL;
    }

    int64_t timeout_ns = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    while (true)
    {
        int result = gpiod_line_request_wait_edge_events(in_request, timeout_ns);
        if (result == 1)
        {
            int events_read = gpiod_line_request_read_edge_events(
                in_request, evbuf, buffer_capacity);
            if (events_read <= 0)
                continue;

            size_t event_count = gpiod_edge_event_buffer_get_num_events(evbuf);
            for (size_t i = 0; i < event_count; ++i)
            {
                struct gpiod_edge_event *event =
                    gpiod_edge_event_buffer_get_event(evbuf, i);
                enum gpiod_edge_event_type event_type =
                    gpiod_edge_event_get_event_type(event);
                unsigned int offset = gpiod_edge_event_get_line_offset(event);

                if (offset == DIP_sw.pfo_n)
                {
                    struct timespec now;
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    double elapsed = pfo_change_initialized
                                         ? diffcltime(last_pfo_change, now)
                                         : PFO_DEBOUNCE_INTERVAL;
                    if (elapsed < PFO_DEBOUNCE_INTERVAL)
                        continue;

                    uint8_t current = (uint8_t)gpiod_line_request_get_value(
                        in_request, DIP_sw.pfo_n);
                    if (lastval_pfo != current)
                    {
                        if (event_type == GPIOD_EDGE_EVENT_FALLING_EDGE)
                        {
                            syslog(LOG_INFO, "UPS line power NOK!");
                            publish_mqtt_state("OFF", lastval_lim == 0 ? "ON" : "OFF",
                                               "On Backup");
                        }
                        else if (event_type == GPIOD_EDGE_EVENT_RISING_EDGE)
                        {
                            low_ups_suppressed = 0;
                            syslog(LOG_INFO, "UPS line power OK.");
                            publish_mqtt_state("ON", lastval_lim == 0 ? "ON" : "OFF",
                                               "Online");
                        }
                        lastval_pfo = current;
                        last_pfo_change = now;
                        pfo_change_initialized = true;
                    }
                }
                else if (offset == DIP_sw.lim_n)
                {
                    uint8_t current = (uint8_t)gpiod_line_request_get_value(
                        in_request, DIP_sw.lim_n);
                    if (event_type == GPIOD_EDGE_EVENT_FALLING_EDGE)
                    {
                        shutdown_pulse = 1;
                        clock_gettime(CLOCK_MONOTONIC, &start_time);
                        if (lastval_pfo == 0 && low_ups_suppressed == 0)
                        {
                            syslog(LOG_INFO, "UPS energy level LOW.");
                            publish_mqtt_state("OFF", "ON", "Low Energy Warning");
                            low_ups_suppressed = 1;
                        }
                    }
                    else if (event_type == GPIOD_EDGE_EVENT_RISING_EDGE)
                    {
                        syslog(LOG_INFO, "UPS energy level HIGH.");
                        shutdown_pulse = 0;
                        publish_mqtt_state(lastval_pfo == 1 ? "ON" : "OFF", "OFF",
                                           lastval_pfo == 1 ? "Online" : "On Backup");
                    }
                    lastval_lim = current;
                }
            }
        }
        fflush(stdout);
        usleep(POLLINTERVAL);
    }
}

int g_gpiorelease(void)
{
    if (in_request)
        gpiod_line_request_release(in_request);
    if (shd_request)
        gpiod_line_request_release(shd_request);
    if (evbuf)
        gpiod_edge_event_buffer_free(evbuf);
    if (chip)
        gpiod_chip_close(chip);

    if (mqtt_enabled && g_mosq)
    {
        mosquitto_loop_stop(g_mosq, true);
        mosquitto_destroy(g_mosq);
        g_mosq = NULL;
        mosquitto_lib_cleanup();
    }
    return 0;
}

int g_gpioinit(void)
{
    chip = gpiod_chip_open(chip_path);
    if (!chip)
    {
        chip = gpiod_chip_open("/dev/gpiochip0");
        if (!chip)
            chip = gpiod_chip_open("/dev/gpiochip4");
        if (!chip)
        {
            syslog(LOG_ERR, "Open GPIO chip failed");
            return -1;
        }
    }

    struct gpiod_chip_info *info = gpiod_chip_get_info(chip);
    if (info)
    {
        syslog(LOG_INFO, "Chip name: %s - label: %s - %zu lines",
               gpiod_chip_info_get_name(info), gpiod_chip_info_get_label(info),
               gpiod_chip_info_get_num_lines(info));
        gpiod_chip_info_free(info);
    }

    struct gpiod_request_config *output_request = gpiod_request_config_new();
    struct gpiod_line_settings *output_settings = gpiod_line_settings_new();
    struct gpiod_line_config *output_config = gpiod_line_config_new();
    if (!output_request || !output_settings || !output_config)
        return -1;

    gpiod_request_config_set_consumer(output_request, CONSUMER);
    gpiod_line_settings_set_direction(output_settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(output_settings, GPIOD_LINE_VALUE_ACTIVE);
    unsigned int output_offset = DIP_sw.shd_n;
    gpiod_line_config_add_line_settings(output_config, &output_offset, 1,
                                        output_settings);
    shd_request = gpiod_chip_request_lines(chip, output_request, output_config);
    gpiod_line_settings_free(output_settings);
    gpiod_line_config_free(output_config);
    gpiod_request_config_free(output_request);
    if (!shd_request)
        return -1;

    struct gpiod_request_config *input_request = gpiod_request_config_new();
    struct gpiod_line_settings *input_settings = gpiod_line_settings_new();
    struct gpiod_line_config *input_config = gpiod_line_config_new();
    if (!input_request || !input_settings || !input_config)
        return -1;

    gpiod_request_config_set_consumer(input_request, CONSUMER);
    gpiod_line_settings_set_direction(input_settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(input_settings, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_bias(input_settings, GPIOD_LINE_BIAS_DISABLED);
    unsigned int input_offsets[2] = {DIP_sw.pfo_n, DIP_sw.lim_n};
    gpiod_line_config_add_line_settings(input_config, input_offsets, 2,
                                        input_settings);
    in_request = gpiod_chip_request_lines(chip, input_request, input_config);
    gpiod_line_settings_free(input_settings);
    gpiod_line_config_free(input_config);
    gpiod_request_config_free(input_request);
    if (!in_request)
        return -1;

    ts.tv_sec = 10;
    ts.tv_nsec = 0;
    lastval_pfo = (uint8_t)gpiod_line_request_get_value(in_request, DIP_sw.pfo_n);
    lastval_lim = (uint8_t)gpiod_line_request_get_value(in_request, DIP_sw.lim_n);

    if (lastval_pfo == 0)
        syslog(LOG_INFO, "UPS line power NOK!");
    else
        syslog(LOG_INFO, "UPS line power OK!");
    if (lastval_lim == 0)
        syslog(LOG_INFO, "UPS energy level LOW.");
    else
        syslog(LOG_INFO, "UPS energy level HIGH.");

    if (lastval_pfo == 0 && lastval_lim == 0)
        low_ups_suppressed = 1;

    publish_mqtt_state(lastval_pfo == 1 ? "ON" : "OFF",
                       lastval_lim == 1 ? "OFF" : "ON",
                       lastval_pfo == 1 ? "Online" : "On Backup");
    return 0;
}

int g_gpio_events(void)
{
    if (pthread_create(&g_thread, NULL, &g_callback, NULL) != 0)
    {
        syslog(LOG_ERR, "GPIO event thread init failed.");
        return -1;
    }
    if (pthread_create(&g_shdthread, NULL, &g_shdcallback, NULL) != 0)
    {
        syslog(LOG_ERR, "Shutdown thread init failed.");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    openlog(CONSUMER, LOG_PID | LOG_NDELAY, LOG_USER);

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            load_config_file(argv[++i]);
        }
    }

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            ++i;
        else if (strcmp(argv[i], "--shutdown-delay") == 0 && i + 1 < argc)
            shutdown_delay = (uint8_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--noshutdown") == 0)
            shutdown_enabled = false;
        else if (strcmp(argv[i], "--dip") == 0 && i + 1 < argc)
        {
            if (!set_dip_switch(argv[++i]))
            {
                fprintf(stderr, "Invalid DIP setting provided.\n");
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "--chip") == 0 && i + 1 < argc)
            copy_config_string(chip_path, sizeof(chip_path), argv[++i]);
        else if (strcmp(argv[i], "--mqtt-broker") == 0 && i + 1 < argc)
            copy_config_string(mqtt_broker, sizeof(mqtt_broker), argv[++i]);
        else if (strcmp(argv[i], "--mqtt-port") == 0 && i + 1 < argc)
            mqtt_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--mqtt-user") == 0 && i + 1 < argc)
            copy_config_string(mqtt_user, sizeof(mqtt_user), argv[++i]);
        else if (strcmp(argv[i], "--mqtt-pass") == 0 && i + 1 < argc)
            copy_config_string(mqtt_pass, sizeof(mqtt_pass), argv[++i]);
        else if (strcmp(argv[i], "--mqtt-disable") == 0)
            mqtt_enabled = false;
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (!dip_configured)
    {
        fprintf(stderr, "Error: DIP switch configuration not provided. Use --dip.\n");
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Used pins (BCM) - pfo: %d, lim: %d, shd: %d",
           DIP_sw.pfo_n, DIP_sw.lim_n, DIP_sw.shd_n);
    syslog(LOG_INFO, "Shutdown delay %d seconds", shutdown_delay);

    mqtt_init();
    if (g_gpioinit() != 0 || g_gpio_events() != 0)
    {
        g_gpiorelease();
        closelog();
        return EXIT_FAILURE;
    }

    pthread_join(g_thread, NULL);
    pthread_join(g_shdthread, NULL);
    g_gpiorelease();
    closelog();
    return 0;
}
