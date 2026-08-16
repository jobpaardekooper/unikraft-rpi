/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * gpio_if.h — GPIO KOBJ method descriptors for the shim layer.
 *
 * In real FreeBSD these are generated from gpio_if.m by kobj_makemethods.
 * Here we just declare the extern kobjop_desc symbols; the definitions live
 * in kobj_descriptors.c.
 */
#pragma once
#include <sys/kobj.h>

extern struct kobjop_desc gpio_get_bus_desc;
extern struct kobjop_desc gpio_pin_max_desc;
extern struct kobjop_desc gpio_pin_getname_desc;
extern struct kobjop_desc gpio_pin_getflags_desc;
extern struct kobjop_desc gpio_pin_getcaps_desc;
extern struct kobjop_desc gpio_pin_setflags_desc;
extern struct kobjop_desc gpio_pin_get_desc;
extern struct kobjop_desc gpio_pin_set_desc;
extern struct kobjop_desc gpio_pin_toggle_desc;
