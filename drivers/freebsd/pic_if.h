/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * pic_if.h — PIC (Programmable Interrupt Controller) KOBJ method descriptors.
 *
 * In real FreeBSD these are generated from pic_if.m.
 * Here we declare the extern kobjop_desc symbols; definitions in
 * kobj_descriptors.c.
 */
#pragma once
#include <sys/kobj.h>

extern struct kobjop_desc pic_disable_intr_desc;
extern struct kobjop_desc pic_enable_intr_desc;
extern struct kobjop_desc pic_map_intr_desc;
extern struct kobjop_desc pic_post_filter_desc;
extern struct kobjop_desc pic_post_ithread_desc;
extern struct kobjop_desc pic_pre_ithread_desc;
extern struct kobjop_desc pic_setup_intr_desc;
extern struct kobjop_desc pic_teardown_intr_desc;
