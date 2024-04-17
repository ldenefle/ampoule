/**
 * @file main
 * @author Lucas Denefle - ldenefle@gmail.com
 * @date 2023-12-15 14:05:20
 * @brief
 *
 */

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <zephyr/ztest.h>
#include "ampoule/ingestion.h"

/******************************************************************************/
/* Local Constant, Macro and Type Definitions                                 */
/******************************************************************************/

/******************************************************************************/
/* Local Function Prototypes                                                  */
/******************************************************************************/

/******************************************************************************/
/* Local Variable Definitions                                                 */
/******************************************************************************/
static struct ingestion ingestion;
static bool cb_called = false;
static uint8_t cb_data[512];
static uint16_t cb_data_len;

/******************************************************************************/
/* Global Function Definitions                                                */
/******************************************************************************/
static int cb(uint8_t *data, uint16_t len)
{
	cb_called = true;
    // __ASSERT(len < sizeof(cb_data), "Can't write that much data");

    memcpy(cb_data, data, len);
    cb_data_len = len;
	return 0;
}

static void before(void *fixture)
{
	cb_called = false;
    memset(cb_data, 0, sizeof(cb_data));
    cb_data_len = 0;
}

ZTEST(in_tests, test_ingestion_NULL_cb_trigger_error)
{
	zassert_true(ingestion_init(&ingestion, NULL) == -EINVAL);
}

ZTEST(in_tests, test_ingestion_non_null_cb_trigger_success)
{
	zassert_ok(ingestion_init(&ingestion, cb));
}

ZTEST(in_tests, test_ingestion_can_receive_pkt)
{
	zassert_ok(ingestion_init(&ingestion, cb));
	uint8_t packet[3] = {0, 0x01, 0xAA};

	ingestion_feed(&ingestion, packet, sizeof(packet));

	k_sleep(K_MSEC(1));

	zassert_true(cb_called);
    zassert_true(cb_data_len == 1);
    zassert_true(cb_data[0] == 0xAA);
}

ZTEST(in_tests, test_packet_can_be_chunked)
{
    ingestion_init(&ingestion, cb);
    uint8_t first_chunk[2] = {0x00, 0x02};
    uint8_t second_chunk[2] = {0xAA, 0xBB};

	ingestion_feed(&ingestion, first_chunk, sizeof(first_chunk));
	ingestion_feed(&ingestion, second_chunk, sizeof(second_chunk));

	k_sleep(K_MSEC(1));

	zassert_true(cb_called);
    zassert_true(cb_data_len == 2);
    zassert_true(cb_data[0] == 0xAA);
    zassert_true(cb_data[1] == 0xBB);
}

ZTEST(in_tests, test_packet_can_be_chunked_with_delay)
{
    ingestion_init(&ingestion, cb);
    uint8_t first_chunk[2] = {0x00, 0x02};
    uint8_t second_chunk[2] = {0xAA, 0xBB};

	ingestion_feed(&ingestion, first_chunk, sizeof(first_chunk));
	k_sleep(K_MSEC(10));
	ingestion_feed(&ingestion, second_chunk, sizeof(second_chunk));
	k_sleep(K_MSEC(1));

	zassert_true(cb_called);
    zassert_true(cb_data_len == 2);
    zassert_true(cb_data[0] == 0xAA);
    zassert_true(cb_data[1] == 0xBB);
}

ZTEST(in_tests, test_ingestion_uncomplete_packet_timeout)
{
	zassert_ok(ingestion_init(&ingestion, cb));
	uint8_t garbage[] = {0, 0x01};

	ingestion_feed(&ingestion, garbage, sizeof(garbage));

	k_sleep(K_MSEC(CONFIG_AMPOULE_INGESTION_TIMEOUT_MS + 1));

	zassert_false(cb_called);
}

ZTEST(in_tests, test_ingestion_timeout_allow_for_resending)
{
	zassert_ok(ingestion_init(&ingestion, cb));
	uint8_t garbage[] = {0, 0x01};

	ingestion_feed(&ingestion, garbage, sizeof(garbage));

	k_sleep(K_MSEC(CONFIG_AMPOULE_INGESTION_TIMEOUT_MS + 1));

	zassert_false(cb_called);

	uint8_t packet[] = {0, 0x01, 0};

	ingestion_feed(&ingestion, packet, sizeof(packet));
	k_sleep(K_MSEC(1));

	zassert_true(cb_called);
}

/******************************************************************************/
/* Local Function Definitions                                                 */
/******************************************************************************/

ZTEST_SUITE(in_tests, NULL, NULL, before, NULL, NULL);
