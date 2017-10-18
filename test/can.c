/**
 * @file      can.c
 * @brief     Unit test for the CAN schema parser and evaluator.
 * @author    Martin Kelly <mkelly@xevo.com>
 * @copyright Copyright (C) 2017 Xevo Inc. All Rights Reserved.
 */

#define _POSIX_C_SOURCE 200809L
#include <float.h>
#include <linux/limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xlib/xassert.h>
#include <yobd/yobd.h>

void print_err(const char *msg)
{
    fputs(msg, stderr);
}

XASSERT_DEFINE_ASSERTS(print_err)

bool float_eq(float a, float b)
{
    return fabs(a - b) < DBL_EPSILON;
}

int main(int argc, const char **argv)
{
    struct yobd_ctx *ctx;
    union {
        uint16_t as_uint16_t;
        uint8_t as_uint8_t;
    } engine_rpm;
    yobd_err err;
    struct can_frame frame;
    yobd_mode mode;
    yobd_pid pid;
    const struct yobd_pid_desc *pid_desc;
    const char *schema_file;
    const char *str;
    union {
        float as_float;
        uint8_t as_uint8_t;
        uint8_t as_bytes[sizeof(float)];
    } u;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s SCHEMA-FILE\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (strnlen(argv[1], PATH_MAX) == PATH_MAX) {
        fprintf(stderr, "File argument is longer than PATH_MAX\n");
        exit(EXIT_FAILURE);
    }
    schema_file = argv[1];

    ctx = NULL;
    err = yobd_parse_schema(schema_file, &ctx);
    XASSERT_EQ(err, YOBD_OK);
    XASSERT_NOT_NULL(ctx);

    err = yobd_get_pid_descriptor(ctx, 0x1, 0x0c, &pid_desc);
    XASSERT_EQ(err, YOBD_OK);
    XASSERT_NOT_NULL(pid_desc)
    XASSERT_EQ(strcmp(pid_desc->name, "Engine RPM"), 0);
    XASSERT_EQ(pid_desc->can_bytes, 2);
    XASSERT_EQ(pid_desc->interpreted_bytes, sizeof(float));
    XASSERT_EQ(pid_desc->type, YOBD_PID_DATA_TYPE_FLOAT);

    err = yobd_get_unit_str(ctx, pid_desc->unit, &str);
    XASSERT_EQ(err, YOBD_OK);
    XASSERT_EQ(strcmp(str, "rpm"), 0);

    memset(&frame, 0, sizeof(frame));
    err = yobd_make_can_query(ctx, 0x1, 0x0c, &frame);
    XASSERT_EQ(err, YOBD_OK);
    XASSERT_EQ(frame.can_id, 0x7df);
    XASSERT_EQ(frame.can_dlc, 8);
    XASSERT_EQ(frame.data[0], 2);
    XASSERT_EQ(frame.data[1], 0x1);
    XASSERT_EQ(frame.data[2], 0x0c);
    XASSERT_EQ(frame.data[3], 0xcc);
    XASSERT_EQ(frame.data[4], 0xcc);
    XASSERT_EQ(frame.data[5], 0xcc);
    XASSERT_EQ(frame.data[6], 0xcc);
    XASSERT_EQ(frame.data[7], 0xcc);

    memset(&frame, 0, sizeof(frame));
    engine_rpm.as_uint16_t = 0xabcd;
    err = yobd_make_can_response(
        ctx,
        0x1,
        0x0c,
        &engine_rpm.as_uint8_t,
        sizeof(engine_rpm),
        &frame);
    XASSERT_EQ(err, YOBD_OK);
    XASSERT_EQ(frame.can_id, 0x7df + 8);
    XASSERT_EQ(frame.can_dlc, 8);
    XASSERT_EQ(frame.data[0], 4);
    XASSERT_EQ(frame.data[1], 0x1 + 0x40);
    XASSERT_EQ(frame.data[2], 0x0c);
    XASSERT_EQ(frame.data[3], 0xcd);
    XASSERT_EQ(frame.data[4], 0xab);
    XASSERT_EQ(frame.data[5], 0xcc);
    XASSERT_EQ(frame.data[6], 0xcc);
    XASSERT_EQ(frame.data[7], 0xcc);

    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x7df;
    frame.can_dlc = 8;
    frame.data[0] = 1;
    frame.data[1] = 0x1;
    frame.data[2] = 0x0d;
    frame.data[3] = 60;
    err = yobd_parse_headers(ctx, &frame, &mode, &pid);
    XASSERT_EQ(err, YOBD_OK);
    XASSERT_EQ(mode, 0x1);
    XASSERT_EQ(pid, 0x0d);

    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x7df + 8;
    frame.can_dlc = 8;
    /* (256*77 + 130) / 4 == 4960.50 RPM */
    frame.data[0] = 4;
    frame.data[1] = 0x1 + 0x40;
    frame.data[2] = 0x0c;
    frame.data[3] = 77;
    frame.data[4] = 130;
    err = yobd_parse_can_response(ctx, &frame, u.as_bytes);
    XASSERT_EQ(err, YOBD_OK);
    XASSERT(float_eq(u.as_float, 4960.500));

    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x7df + 8;
    frame.can_dlc = 8;
    frame.data[0] = 3;
    frame.data[1] = 0x1 + 0x40;
    frame.data[2] = 0x0d;
    frame.data[3] = 60;
    err = yobd_parse_can_response(ctx, &frame, u.as_bytes);
    XASSERT_EQ(err, YOBD_OK);
    XASSERT_EQ(u.as_uint8_t, 60);

    yobd_free_ctx(ctx);

    return 0;
}
