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
#include "pb_encode.h"
#include <zephyr/sys/byteorder.h>

/******************************************************************************/
/* Local Constant, Macro and Type Definitions                                 */
/******************************************************************************/

/******************************************************************************/
/* Local Function Prototypes                                                  */
/******************************************************************************/
static int on_command(Command *command, Response *response);
static int on_write(void *context, uint8_t *data, uint16_t len);

/******************************************************************************/
/* Local Variable Definitions                                                 */
/******************************************************************************/
static struct ingestion ingestion;
static bool cb_called = false;
static uint8_t cb_data[512];
static uint16_t cb_data_len;
static bool rpc_received = false;
static struct ingestion_transport fake_transport = {.write = on_write};

static struct ingestion_rpc fake_rpc = {.on_command = on_command};

static Command received;

static uint8_t valid_packet[100];
static uint32_t valid_packet_len;

/******************************************************************************/
/* Global Function Definitions                                                */
/******************************************************************************/
static int on_write(void *context, uint8_t *data, uint16_t len)
{
	cb_called = true;
	// __ASSERT(len < sizeof(cb_data), "Can't write that much data");

	memcpy(cb_data, data, len);
	cb_data_len = len;

	return len;
}

static int on_command(Command *command, Response *response)
{
	/* Buffers command received */
	memcpy(command, &received, sizeof(Command));

	/* Only responds pong */
	response->opcode = Opcode_PONG;

	/* Cache our reception */
	rpc_received = true;

	return 0;
}

static void before(void *fixture)
{
	/* Reset the transport stubs */
	cb_called = false;
	memset(cb_data, 0, sizeof(cb_data));
	cb_data_len = 0;

	/* Reset the rpc stubs */
	memset(&received, 0, sizeof(Command));
	rpc_received = true;

	/* Initialise the ingestion */
	zassert_ok(ingestion_init(&ingestion, &fake_transport, &fake_rpc, NULL));

	/* Prepare a valid packet to be used to test ingestion */
	Command ping = {.opcode = Opcode_PING};

	pb_ostream_t ostream = pb_ostream_from_buffer(&valid_packet[sizeof(uint16_t)],
						      sizeof(valid_packet) - sizeof(uint16_t));

	zassert_true(pb_encode(&ostream, Command_fields, &ping));

	/* Write size in our packet*/
	sys_put_be16(ostream.bytes_written, &valid_packet[0]);

	valid_packet_len = ostream.bytes_written + sizeof(uint16_t);
}

ZTEST(in_tests, test_ingestion_NULL_cb_trigger_error)
{
	zassert_true(ingestion_init(&ingestion, NULL, &fake_rpc, NULL) == -EINVAL);
	zassert_true(ingestion_init(&ingestion, &fake_transport, NULL, NULL) == -EINVAL);
}

ZTEST(in_tests, test_ingestion_can_receive_pkt)
{
	ingestion_feed(&ingestion, valid_packet, sizeof(valid_packet_len));

	k_sleep(K_MSEC(1));

	zassert_true(rpc_received);
}

ZTEST(in_tests, test_packet_can_be_chunked)
{
	uint8_t *first_chunk = &valid_packet[0];
	uint32_t first_chunk_len = sizeof(uint16_t);
	uint8_t *second_chunk = &valid_packet[sizeof(uint16_t)];
	uint8_t second_chunk_len = valid_packet_len - sizeof(uint16_t);

	ingestion_feed(&ingestion, first_chunk, first_chunk_len);
	ingestion_feed(&ingestion, second_chunk, second_chunk_len);

	k_sleep(K_MSEC(1));

	zassert_true(rpc_received);
}

ZTEST(in_tests, test_packet_can_be_chunked_with_delay)
{
	uint8_t *first_chunk = &valid_packet[0];
	uint32_t first_chunk_len = sizeof(uint16_t);
	uint8_t *second_chunk = &valid_packet[sizeof(uint16_t)];
	uint8_t second_chunk_len = valid_packet_len - sizeof(uint16_t);

	ingestion_feed(&ingestion, first_chunk, first_chunk_len);
	k_sleep(K_MSEC(10));
	ingestion_feed(&ingestion, second_chunk, second_chunk_len);
	k_sleep(K_MSEC(1));

	zassert_true(rpc_received);
}

ZTEST(in_tests, test_ingestion_uncomplete_packet_timeout)
{
	uint8_t *first_chunk = &valid_packet[0];
	uint32_t first_chunk_len = sizeof(uint16_t);

	ingestion_feed(&ingestion, first_chunk, first_chunk_len);

	k_sleep(K_MSEC(CONFIG_AMPOULE_INGESTION_TIMEOUT_MS + 1));

	zassert_false(cb_called);
}

ZTEST(in_tests, test_ingestion_timeout_allow_for_resending)
{
	uint8_t garbage[] = {0, 0x01};

	ingestion_feed(&ingestion, garbage, sizeof(garbage));

	k_sleep(K_MSEC(CONFIG_AMPOULE_INGESTION_TIMEOUT_MS + 1));

	zassert_false(cb_called);

	ingestion_feed(&ingestion, valid_packet, valid_packet_len);
	k_sleep(K_MSEC(1));

	zassert_true(rpc_received);
}

/******************************************************************************/
/* Local Function Definitions                                                 */
/******************************************************************************/

ZTEST_SUITE(in_tests, NULL, NULL, before, NULL, NULL);
