#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <stdbool.h>

#define CONSUMER "qUPS-guard"
#define POLLINTERVAL 1000
#define INPUT_DEBOUNCE_INTERVAL 500000
#define SHUTDOWN_DELAY 0

static pthread_t g_thread, g_shdthread;
static struct gpiod_chip *chip;
static uint8_t lastval_pfo = 255, lastval_lim = 255;
static struct timespec ts;
static struct timespec start_time;
static struct gpiod_line_request *in_request;
static struct gpiod_line_request *shd_request;
static struct gpiod_edge_event_buffer *evbuf;
static uint8_t shutdown_delay = SHUTDOWN_DELAY;
static uint8_t shutdown_pulse;
static bool shutdown_enabled = true;
static uint8_t low_ups_suppressed;
static struct timespec last_pfo_change;
static struct timespec last_lim_change;
static bool pfo_change_initialized;
static bool lim_change_initialized;

struct DIPsw
{
    const char *DIP;
    unsigned int pfo_n;
    unsigned int lim_n;
    unsigned int shd_n;
} DIP_sw;

static struct DIPsw DIPswa[10] = {
    {"10", 17, 27, 22}, {"01", 23, 24, 25}, {"11", 5, 6, 26},
    {"111", 4, 24, 23}, {"011", 14, 18, 15}, {"101", 25, 7, 8},
    {"001", 17, 22, 27}, {"110", 10, 11, 9}, {"010", 12, 20, 16},
    {"100", 19, 21, 26}};

static double diffcltime(struct timespec a, struct timespec b)
{
    long long elapsed_nanoseconds = (b.tv_sec - a.tv_sec) * 1000000000LL +
                                    (b.tv_nsec - a.tv_nsec);
    return (double)(elapsed_nanoseconds / 1000.0);
}

static bool set_dip_switch(const char *code)
{
    for (size_t i = 0; i < sizeof(DIPswa) / sizeof(DIPswa[0]); ++i)
    {
        if (strcmp(code, DIPswa[i].DIP) == 0)
        {
            DIP_sw = DIPswa[i];
            return true;
        }
    }
    return false;
}

static void *g_shdcallback(void *args)
{
    (void)args;
    while (true)
    {
        if (shutdown_pulse)
        {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (diffcltime(start_time, now) > POLLINTERVAL)
            {
                syslog(LOG_INFO, "Limit LOW since %.2f ms - initiating shutdown with delay %d.",
                       diffcltime(start_time, now) / 1000, shutdown_delay);
                sleep(shutdown_delay);
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
        usleep(POLLINTERVAL);
    }
    return NULL;
}

static void *g_callback(void *args)
{
    (void)args;
    evbuf = gpiod_edge_event_buffer_new(64);
    if (!evbuf)
        return NULL;

    int64_t timeout_ns = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    while (true)
    {
        int result = gpiod_line_request_wait_edge_events(in_request, timeout_ns);
        if (result != 1)
            continue;

        int events_read = gpiod_line_request_read_edge_events(in_request, evbuf, 64);
        if (events_read <= 0)
            continue;

        size_t event_count = gpiod_edge_event_buffer_get_num_events(evbuf);
        for (size_t i = 0; i < event_count; ++i)
        {
            struct gpiod_edge_event *event =
                gpiod_edge_event_buffer_get_event(evbuf, i);
            unsigned int offset = gpiod_edge_event_get_line_offset(event);
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);

            if (offset == DIP_sw.pfo_n)
            {
                double elapsed = pfo_change_initialized
                                     ? diffcltime(last_pfo_change, now)
                                     : INPUT_DEBOUNCE_INTERVAL;
                if (elapsed < INPUT_DEBOUNCE_INTERVAL)
                    continue;

                uint8_t current = (uint8_t)gpiod_line_request_get_value(
                    in_request, DIP_sw.pfo_n);
                if (lastval_pfo == current)
                    continue;

                lastval_pfo = current;
                last_pfo_change = now;
                pfo_change_initialized = true;
                if (current == 0)
                    syslog(LOG_INFO, "UPS line power NOK!");
                else
                {
                    syslog(LOG_INFO, "UPS line power OK.");
                    low_ups_suppressed = 0;
                }
            }
            else if (offset == DIP_sw.lim_n)
            {
                double elapsed = lim_change_initialized
                                     ? diffcltime(last_lim_change, now)
                                     : INPUT_DEBOUNCE_INTERVAL;
                if (elapsed < INPUT_DEBOUNCE_INTERVAL)
                    continue;

                uint8_t current = (uint8_t)gpiod_line_request_get_value(
                    in_request, DIP_sw.lim_n);
                if (lastval_lim == current)
                    continue;

                lastval_lim = current;
                last_lim_change = now;
                lim_change_initialized = true;
                if (current == 0)
                {
                    shutdown_pulse = 1;
                    clock_gettime(CLOCK_MONOTONIC, &start_time);
                    if (lastval_pfo == 0 && low_ups_suppressed == 0)
                    {
                        syslog(LOG_INFO, "LOW UPS level detected! UPS energy level LOW.");
                        low_ups_suppressed = 1;
                    }
                    else
                        syslog(LOG_INFO, "UPS energy level LOW.");
                }
                else
                {
                    syslog(LOG_INFO, "UPS energy level HIGH.");
                    shutdown_pulse = 0;
                }
            }
        }
    }
}

static void g_gpiorelease(void)
{
    if (in_request)
        gpiod_line_request_release(in_request);
    if (shd_request)
        gpiod_line_request_release(shd_request);
    if (evbuf)
        gpiod_edge_event_buffer_free(evbuf);
    if (chip)
        gpiod_chip_close(chip);
}

static int g_gpioinit(void)
{
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip)
        chip = gpiod_chip_open("/dev/gpiochip4");
    if (!chip)
    {
        syslog(LOG_ERR, "Open chip failed");
        return -1;
    }

    struct gpiod_request_config *output_request = gpiod_request_config_new();
    struct gpiod_line_settings *output_settings = gpiod_line_settings_new();
    struct gpiod_line_config *output_config = gpiod_line_config_new();
    struct gpiod_request_config *input_request = gpiod_request_config_new();
    struct gpiod_line_settings *input_settings = gpiod_line_settings_new();
    struct gpiod_line_config *input_config = gpiod_line_config_new();
    if (!output_request || !output_settings || !output_config ||
        !input_request || !input_settings || !input_config)
        return -1;

    gpiod_request_config_set_consumer(output_request, CONSUMER);
    gpiod_line_settings_set_direction(output_settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(output_settings, GPIOD_LINE_VALUE_ACTIVE);
    gpiod_line_config_add_line_settings(output_config, &DIP_sw.shd_n, 1, output_settings);
    shd_request = gpiod_chip_request_lines(chip, output_request, output_config);
    gpiod_line_settings_free(output_settings);
    gpiod_line_config_free(output_config);
    gpiod_request_config_free(output_request);
    if (!shd_request)
        return -1;

    gpiod_request_config_set_consumer(input_request, CONSUMER);
    gpiod_line_settings_set_direction(input_settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(input_settings, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_bias(input_settings, GPIOD_LINE_BIAS_DISABLED);
    unsigned int offsets[2] = {DIP_sw.pfo_n, DIP_sw.lim_n};
    gpiod_line_config_add_line_settings(input_config, offsets, 2, input_settings);
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
    if (lastval_pfo == 0 && lastval_lim == 0)
        low_ups_suppressed = 1;
    return 0;
}

int main(int argc, char **argv)
{
    openlog(CONSUMER, LOG_PID | LOG_NDELAY, LOG_USER);
    bool dip_matched = false;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--shutdown-delay") == 0 && i + 1 < argc)
            shutdown_delay = (uint8_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--noshutdown") == 0)
            shutdown_enabled = false;
        else if (strcmp(argv[i], "--dip") == 0 && i + 1 < argc)
        {
            dip_matched = set_dip_switch(argv[++i]);
            if (!dip_matched)
            {
                fprintf(stderr, "Invalid DIP setting provided.\n");
                return EXIT_FAILURE;
            }
        }
        else
        {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (!dip_matched)
    {
        fprintf(stderr, "Used pins not specified - usage: --dip <DIP switch>\n");
        return EXIT_FAILURE;
    }
    if (g_gpioinit() != 0)
    {
        g_gpiorelease();
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Used pins (BCM) - pfo: %d, lim: %d, shd: %d",
           DIP_sw.pfo_n, DIP_sw.lim_n, DIP_sw.shd_n);
    syslog(LOG_INFO, "Shutdown delay %d seconds", shutdown_delay);
    if (pthread_create(&g_thread, NULL, g_callback, NULL) != 0 ||
        pthread_create(&g_shdthread, NULL, g_shdcallback, NULL) != 0)
    {
        syslog(LOG_ERR, "Thread init failed.");
        g_gpiorelease();
        return EXIT_FAILURE;
    }

    pthread_join(g_thread, NULL);
    pthread_join(g_shdthread, NULL);
    g_gpiorelease();
    closelog();
    return 0;
}
