/*
 * Minimal C test framework - header-only, no external dependencies
 * 
 * Usage:
 *   #include "test.h"
 *   
 *   TEST(my_test) {
 *       ASSERT_TRUE(1 == 1);
 *       ASSERT_EQ_INT(42, compute_answer());
 *   }
 *   
 *   TEST_MAIN(
 *       my_test();
 *       another_test();
 *   )
 */

#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)

#define ASSERT_TRUE(cond)                                                                                              \
	do {                                                                                                               \
		tests_run++;                                                                                                   \
		if (!(cond)) {                                                                                                 \
			tests_failed++;                                                                                            \
			fprintf(stderr, "FAIL: ASSERT_TRUE(%s) at %s:%d\n", #cond, __FILE__, __LINE__);                           \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define ASSERT_FALSE(cond)                                                                                             \
	do {                                                                                                               \
		tests_run++;                                                                                                   \
		if (cond) {                                                                                                    \
			tests_failed++;                                                                                            \
			fprintf(stderr, "FAIL: ASSERT_FALSE(%s) at %s:%d\n", #cond, __FILE__, __LINE__);                          \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define ASSERT_EQ_INT(expected, actual)                                                                                \
	do {                                                                                                               \
		tests_run++;                                                                                                   \
		int _exp = (int)(expected);                                                                                    \
		int _act = (int)(actual);                                                                                      \
		if (_exp != _act) {                                                                                            \
			tests_failed++;                                                                                            \
			fprintf(stderr, "FAIL: ASSERT_EQ_INT(%s == %s): expected=%d, got=%d at %s:%d\n", #expected, #actual, _exp, \
			    _act, __FILE__, __LINE__);                                                                             \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define ASSERT_NE_INT(not_expected, actual)                                                                            \
	do {                                                                                                               \
		tests_run++;                                                                                                   \
		int _nexp = (int)(not_expected);                                                                               \
		int _act = (int)(actual);                                                                                      \
		if (_nexp == _act) {                                                                                           \
			tests_failed++;                                                                                            \
			fprintf(stderr, "FAIL: ASSERT_NE_INT(%s != %s): both are %d at %s:%d\n", #not_expected, #actual, _act,    \
			    __FILE__, __LINE__);                                                                                   \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define ASSERT_PTR_NOT_NULL(ptr)                                                                                       \
	do {                                                                                                               \
		tests_run++;                                                                                                   \
		if ((ptr) == NULL) {                                                                                           \
			tests_failed++;                                                                                            \
			fprintf(stderr, "FAIL: ASSERT_PTR_NOT_NULL(%s) at %s:%d\n", #ptr, __FILE__, __LINE__);                    \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define ASSERT_IN_RANGE(value, min, max)                                                                               \
	do {                                                                                                               \
		tests_run++;                                                                                                   \
		int _val = (int)(value);                                                                                       \
		int _min = (int)(min);                                                                                         \
		int _max = (int)(max);                                                                                         \
		if (_val < _min || _val > _max) {                                                                              \
			tests_failed++;                                                                                            \
			fprintf(stderr, "FAIL: ASSERT_IN_RANGE(%s in [%d, %d]): got %d at %s:%d\n", #value, _min, _max, _val,     \
			    __FILE__, __LINE__);                                                                                   \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define TEST_MAIN(...)                                                                                                 \
	int main(void)                                                                                                     \
	{                                                                                                                  \
		fprintf(stderr, "Running tests...\n");                                                                         \
		__VA_ARGS__                                                                                                    \
		fprintf(stderr, "\n=== Test Results ===\n");                                                                   \
		fprintf(stderr, "Tests run: %d\n", tests_run);                                                                 \
		fprintf(stderr, "Tests failed: %d\n", tests_failed);                                                           \
		if (tests_failed == 0) {                                                                                       \
			fprintf(stderr, "ALL TESTS PASSED\n");                                                                     \
		} else {                                                                                                       \
			fprintf(stderr, "SOME TESTS FAILED\n");                                                                    \
		}                                                                                                              \
		return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;                                                            \
	}

// Simple deterministic PRNG for fuzz tests (xorshift32)
static uint32_t test_rng_state = 0x12345678u;

static inline void test_rng_seed(uint32_t seed)
{
	test_rng_state = seed ? seed : 0x12345678u;
}

static inline uint32_t test_rng_next(void)
{
	uint32_t x = test_rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	test_rng_state = x;
	return x;
}

static inline int test_rng_range(int min, int max)
{
	return min + (int)(test_rng_next() % (uint32_t)(max - min + 1));
}

#endif /* TEST_H */
