#ifndef LIBHPX_TESTS_TESTS_H_
#define LIBHPX_TESTS_TESTS_H_

#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include <hpx/hpx.h>
#include "domain.h"

extern hpx_action_t t02_init_sources;
extern hpx_action_t t03_initDomain;
extern hpx_action_t t04_root;
extern hpx_action_t t04_get_rank;

int t02_init_sources_action(void*);
int t03_initDomain_action(const InitArgs*);
int t04_root_action(void*);
int t04_get_rank_action(void*);

void hpxtest_core_setup(void);
void hpxtest_core_teardown(void);

void add_02_TestMemAlloc(TCase *);
void add_03_TestGlobalMemAlloc(TCase *);
void add_04_TestMemMove(TCase *);
void add_05_TestParcel(TCase *);

#endif /* LIBHPX_TESTS_TESTS_H_ */
