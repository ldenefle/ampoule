/**
 * @file ingestion
 * @author Lucas Denefle - ldenefle@gmail.com
 * @date 2023-12-15 14:01:32
 * @brief
 *
 */

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include "pb_decode.h"
#include "pb_encode.h"

#include <ampoule/ingestion.h>
#include "ampoule/command.h"

/******************************************************************************/
/* Local Constant, Macro and Type Definitions                                 */
/******************************************************************************/
LOG_MODULE_REGISTER(ingestion, CONFIG_AMPOULE_LOG_LEVEL);

/******************************************************************************/
/* Local Function Prototypes                                                  */
/******************************************************************************/
static void ingestion_process(struct k_work *work);
static void ingestion_timeout(struct k_work *work);

/******************************************************************************/
/* Local Variable Definitions                                                 */
/******************************************************************************/

/******************************************************************************/
/* Global Function Definitions                                                */
/******************************************************************************/
int ingestion_init(struct ingestion *ingestion, struct ingestion_transport *transport,
		   struct ingestion_rpc *rpc, void *context)
{
	if (transport == NULL || rpc == NULL) {
		return -EINVAL;
	}

	ingestion->transport = transport;
	ingestion->transport_context = context;

	ingestion->rpc = rpc;

	ingestion->bytes_read = 0;
	ingestion->state = RCV_LENGTH_HIGH;

	k_work_init(&ingestion->ingest_work, ingestion_process);
	k_work_init_delayable(&ingestion->timeout_work, ingestion_timeout);

	ring_buf_init(&ingestion->rb, INGESTION_PACKET_MAX_SIZE, &ingestion->current[0]);

	return 0;
}

int ingestion_feed(struct ingestion *ingestion, uint8_t *data, uint16_t len)
{
	ring_buf_put(&ingestion->rb, data, len);

	k_work_submit(&ingestion->ingest_work);

	LOG_HEXDUMP_DBG(data, len, "Feed data");

	return 0;
}

/******************************************************************************/
/* Local Function Definitions                                                 */
/******************************************************************************/
static void ingestion_timeout(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct ingestion *ingestion = CONTAINER_OF(dwork, struct ingestion, timeout_work);

	ring_buf_reset(&ingestion->rb);
	ingestion->state = RCV_LENGTH_HIGH;
}

static int ingestion_parse(struct ingestion *ingestion, uint8_t *data, uint16_t len)
{
	int ret;
	bool status;
	uint8_t output[512];
	ampoule_Command command;
	ampoule_Response response;

	pb_istream_t istream = pb_istream_from_buffer(data, len);

	status = pb_decode(&istream, ampoule_Command_fields, &command);
	if (!status) {
		return -EINVAL;
	}

	ingestion->rpc->on_command(&command, &response);

	pb_ostream_t ostream =
		pb_ostream_from_buffer(&output[2], sizeof(output) - sizeof(uint16_t));
	status = pb_encode(&ostream, ampoule_Response_fields, &response);
	if (!status) {
		return -EINVAL;
	}

	uint16_t bytes_to_write = ostream.bytes_written;
	sys_put_be16(bytes_to_write, &output[0]);

	bytes_to_write += sizeof(uint16_t);

	int bytes_written = 0;

	do {
		ret = ingestion->transport->write(ingestion->transport_context,
						  &output[bytes_written],
						  bytes_to_write - bytes_written);
		if (ret < 0) {
			break;
		}
		bytes_written += ret;
	} while (bytes_written < bytes_to_write);

	return 0;
}

static void ingestion_process(struct k_work *work)
{
	struct ingestion *ingestion = CONTAINER_OF(work, struct ingestion, ingest_work);
	int rc;

	do {
		switch (ingestion->state) {
		case RCV_LENGTH_HIGH: {
			uint8_t high;
			rc = ring_buf_get(&ingestion->rb, &high, sizeof(uint8_t));
			__ASSERT_NO_MSG(rc == 1);
			ingestion->expected_size = high << 8;
			ingestion->state = RCV_LENGTH_LOW;
			k_work_schedule(&ingestion->timeout_work,
					K_MSEC(CONFIG_AMPOULE_INGESTION_TIMEOUT_MS));
		} break;
		case RCV_LENGTH_LOW: {
			uint8_t low;
			rc = ring_buf_get(&ingestion->rb, &low, sizeof(uint8_t));
			__ASSERT_NO_MSG(rc == 1);
			ingestion->expected_size += low;
			ingestion->state = RCV_DATA;
		}

		break;
		case RCV_DATA: {
			k_sleep(K_MSEC(1));

			if (ring_buf_size_get(&ingestion->rb) >= ingestion->expected_size) {
				ingestion->state = PARSING;
				k_work_cancel_delayable(&ingestion->timeout_work);
				k_work_submit(&ingestion->ingest_work);
				return;
			}
		} break;
		case PARSING: {
			uint8_t *data;
			rc = ring_buf_get_claim(&ingestion->rb, &data, ingestion->expected_size);
			ingestion_parse(ingestion, data, rc);

			ring_buf_get_finish(&ingestion->rb, ingestion->expected_size);
			ingestion->state = RCV_LENGTH_HIGH;
		} break;
		}
	} while (ring_buf_size_get(&ingestion->rb) != 0);
}
