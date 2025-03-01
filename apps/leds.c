/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  leds.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(01/09/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "01/09/2025 04:12:02 PM"
 *                 
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include <gpiod.h>

#define DELAY     300

#define ON        1
#define OFF       0

/* Three LEDs number */
enum
{
    LED_R = 0,
    LED_G,
    LED_B,
    LEDCNT,
};

enum
{
    ACTIVE_HIGH, /* High level will turn led on */
    ACTIVE_LOW,  /* Low level will turn led on */
};

/* Three LEDs hardware information */
typedef struct led_s
{
    const char               *name;      /* RGB 3-color LED name  */
    int                       chip_num;  /* RGB 3-color LED connect chip */
    int                       gpio_num;  /* RGB 3-color LED connect line */
    int                       active;    /* RGB 3-color LED active level */
    struct gpiod_line_request *request;  /* libgpiod gpio request handler */
} led_t;

static led_t leds_info[LEDCNT] =
{
    {"blue",   0, 23, ACTIVE_HIGH, NULL }, /* GPIO1_IO23 on chip0 line 23, active high */
    {"green", 4, 1,  ACTIVE_HIGH, NULL }, /* GPIO5_IO01 on chip4 line 1, active high */
    {"red",  4, 8,  ACTIVE_HIGH, NULL }, /* GPIO5_IO08 on chip4 line 8, active high */
};

/* Three LEDs API context */
typedef struct leds_s
{
    led_t               *leds;  /* led pointer to leds_info */
    int                  count; /* led count */
} leds_t;


/* function declaration  */
int init_led(leds_t *leds);
int term_led(leds_t *leds);
int turn_led(leds_t *leds, int which, int cmd);
static inline void msleep(unsigned long ms);


int g_stop = 0;

void sig_handler(int signum)
{
    switch( signum )
    {
        case SIGINT:
        case SIGTERM:
            g_stop = 1;

        default:
            break;
    }

    return ;
}

int main(int argc, char *argv[])
{
    int                 rv;
    leds_t              leds =
    {
        .leds  = leds_info,
        .count = LEDCNT,
    };

    if( (rv=init_led(&leds)) < 0 )
    {
        printf("initial leds gpio failure, rv=%d\n", rv);
        return 1;
    }
    printf("initial RGB Led gpios okay\n");

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    while( !g_stop )
    {
        turn_led(&leds, LED_R, ON);
        msleep(DELAY);
        turn_led(&leds, LED_R, OFF);
        msleep(DELAY);

        turn_led(&leds, LED_G, ON);
        msleep(DELAY);
        turn_led(&leds, LED_G, OFF);
        msleep(DELAY);

        turn_led(&leds, LED_B, ON);
        msleep(DELAY);
        turn_led(&leds, LED_B, OFF);
        msleep(DELAY);
    }

    term_led(&leds);
    return 0;
}

int term_led(leds_t *leds)
{
    int            i;
    led_t         *led;

    printf("terminate RGB Led gpios\n");

    if( !leds )
    {
        printf("Invalid input arguments\n");
        return -1;
    }

    for(i=0; i<leds->count; i++)
    {
        led = &leds->leds[i];

        if( led->request )
        {
            turn_led(leds, i, OFF);
            gpiod_line_request_release(led->request);
        }
    }

    return 0;
}


int init_led(leds_t *leds)
{
    led_t                       *led;
    int                          i, rv = 0;
    char                         chip_dev[32];
    struct gpiod_chip           *chip;      /* gpio chip */
    struct gpiod_line_settings  *settings;  /* gpio direction, bias, active_low, value */
    struct gpiod_line_config    *line_cfg;  /* gpio line */
    struct gpiod_request_config *req_cfg;   /* gpio consumer, it can be NULL */


    if( !leds )
    {
        printf("Invalid input arguments\n");
        return -1;
    }


    /* defined in libgpiod-2.0/lib/line-settings.c:

        struct gpiod_line_settings {
            enum gpiod_line_direction direction;
            enum gpiod_line_edge edge_detection;
            enum gpiod_line_drive drive;
            enum gpiod_line_bias bias;
            bool active_low;
            enum gpiod_line_clock event_clock;
            long debounce_period_us;
            enum gpiod_line_value output_value;
        };
     */
    settings = gpiod_line_settings_new();
    if (!settings)
    {
        printf("unable to allocate line settings\n");
        rv = -2;
        goto cleanup;
    }

    /* defined in libgpiod-2.0/lib/line-config.c

        struct gpiod_line_config {
            struct per_line_config line_configs[LINES_MAX];
            size_t num_configs;
            enum gpiod_line_value output_values[LINES_MAX];
            size_t num_output_values;
            struct settings_node *sref_list;
        };
    */

    line_cfg = gpiod_line_config_new();
    if (!line_cfg)
    {
        printf("unable to allocate the line config structure");
        rv = -2;
        goto cleanup;
    }


    /* defined in libgpiod-2.0/lib/request-config.c:

        struct gpiod_request_config {
            char consumer[GPIO_MAX_NAME_SIZE];
            size_t event_buffer_size;
        };
     */
    req_cfg = gpiod_request_config_new();
    if (!req_cfg)
    {
        printf("unable to allocate the request config structure");
        rv = -2;
        goto cleanup;
    }

    for(i=0; i<leds->count; i++)
    {
        led = &leds->leds[i];

        snprintf(chip_dev, sizeof(chip_dev), "/dev/gpiochip%d", led->chip_num);
        chip = gpiod_chip_open(chip_dev);
        if( !chip )
        {
            printf("open gpiochip failure, maybe you need running as root\n");
            rv = -3;
            goto cleanup;
        }

        /* Set as output direction, active low and default level as inactive */
        gpiod_line_settings_reset(settings);
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_active_low(settings, led->active);
        gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

        /* set gpio line */
        gpiod_line_config_reset(line_cfg);
        gpiod_line_config_add_line_settings(line_cfg, &led->gpio_num, 1, settings);

        /* Can be NULL for default settings. */
        gpiod_request_config_set_consumer(req_cfg, led->name);

        /* Request a set of lines for exclusive usage. */
        led->request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

        gpiod_chip_close(chip);
        //printf("request %5s led[%d] for gpio output okay\n", led->name, led->gpio);
    }

cleanup:

    if( rv< 0 )
        term_led(leds);

    if( line_cfg )
        gpiod_line_config_free(line_cfg);

    if( req_cfg )
        gpiod_request_config_free(req_cfg);

    if( settings )
        gpiod_line_settings_free(settings);

    return rv;
}

int turn_led(leds_t *leds, int which, int cmd)
{
    led_t         *led;
    int            rv = 0;
    int            value = 0;

    if( !leds || which<0 || which>=leds->count )
    {
        printf("Invalid input arguments\n");
        return -1;
    }

    led = &leds->leds[which];

    value = OFF==cmd ? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;

    gpiod_line_request_set_value(led->request, led->gpio_num, value);

    return 0;
}

static inline void msleep(unsigned long ms)
{
    struct timespec cSleep;
    unsigned long ulTmp;

    cSleep.tv_sec = ms / 1000;
    if (cSleep.tv_sec == 0)
    {
        ulTmp = ms * 10000;
        cSleep.tv_nsec = ulTmp * 100;
    }
    else
    {
        cSleep.tv_nsec = 0;
    }

    nanosleep(&cSleep, 0);

    return ;
}
