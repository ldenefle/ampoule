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

/******************************************************************************/
/* Global Function Definitions                                                */
/******************************************************************************/
static int cb(uint8_t * data, uint16_t len){
    cb_called = true;
    return 0;
}

static void before(void * fixture){
    cb_called = false;
}

ZTEST(in_tests, test_ingestion_NULL_cb_trigger_error) {
    zassert_true(ingestion_init(&ingestion, NULL) == -EINVAL);
}

ZTEST(in_tests, test_ingestion_non_null_cb_trigger_success) {
    zassert_ok(ingestion_init(&ingestion, cb));
}

ZTEST(in_tests, test_ingestion_can_receive_pkt) {
    zassert_ok(ingestion_init(&ingestion, cb));
    uint8_t packet[3] = {0, 0x01, 0};

    ingestion_feed(&ingestion, packet, sizeof(packet));

    k_sleep(K_MSEC(1));
    zassert_true(cb_called);
}

ZTEST(in_tests, test_ingestion_uncomplete_packet_timeout) {
    zassert_ok(ingestion_init(&ingestion, cb));
    uint8_t garbage[3] = {0, 0x01, 0x02};

    ingestion_feed(&ingestion, garbage, sizeof(garbage));

    zassert_false(cb_called);
}

ZTEST(in_tests, test_ingestion_timeout_allow_for_resending) {
    zassert_ok(ingestion_init(&ingestion, cb));
    uint8_t garbage[3] = {0, 0x01, 0x02};

    ingestion_feed(&ingestion, garbage, sizeof(garbage));

    k_sleep(K_MSEC(500));
    uint8_t packet[3] = {0, 0x01, 0};

    ingestion_feed(&ingestion, packet, sizeof(packet));
    zassert_true(cb_called);
}

/******************************************************************************/
/* Local Function Definitions                                                 */
/******************************************************************************/

ZTEST_SUITE(in_tests, NULL, NULL, before, NULL, NULL);
