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
// #include <logging/log.h>
#include <zephyr/kernel.h>
#include <ampoule/ingestion.h>


/******************************************************************************/
/* Local Constant, Macro and Type Definitions                                 */
/******************************************************************************/
// LOG_MODULE_REGISTER(ingestion);

/******************************************************************************/
/* Local Function Prototypes                                                  */
/******************************************************************************/
static void ingestion_process(struct k_work * work);
static void ingestion_timeout(struct k_work * work);

/******************************************************************************/
/* Local Variable Definitions                                                 */
/******************************************************************************/


/******************************************************************************/
/* Global Function Definitions                                                */
/******************************************************************************/
int ingestion_init(struct ingestion * ingestion, on_data_cb cb) {
    if (cb == NULL)
    {
        return -EINVAL;
    }
    ingestion->observer = cb;

    ingestion->bytes_read = 0;
    ingestion->state = RCV_LENGTH_HIGH;

    k_work_init(&ingestion->ingest_work, ingestion_process);
    k_work_init_delayable(&ingestion->timeout_work, ingestion_timeout);

    ring_buf_init(&ingestion->rb, INGESTION_PACKET_MAX_SIZE, &ingestion->current[0]);

    return 0;
}

int ingestion_feed(struct ingestion * ingestion, uint8_t * data, uint16_t len) {
    ring_buf_put(&ingestion->rb, data, len);

    k_work_submit(&ingestion->ingest_work);

    return 0;
}


/******************************************************************************/
/* Local Function Definitions                                                 */
/******************************************************************************/
static void ingestion_timeout(struct k_work * work) {
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct ingestion * ingestion = CONTAINER_OF(dwork, struct ingestion, timeout_work);

    ring_buf_reset(&ingestion->rb);
    ingestion->state = RCV_LENGTH_HIGH;
}

static void ingestion_process(struct k_work * work) {
    struct ingestion * ingestion = CONTAINER_OF(work, struct ingestion, ingest_work);
    int rc;

    do {
        switch (ingestion->state)
        {
            case RCV_LENGTH_HIGH:
            {
                uint8_t high;
                rc = ring_buf_get(&ingestion->rb, &high, sizeof(uint8_t));
                __ASSERT_NO_MSG(rc == 1);
                ingestion->expected_size = high << 8;
                ingestion->state = RCV_LENGTH_LOW;
                k_work_schedule(&ingestion->timeout_work, K_MSEC(500));
            }

            break;
            case RCV_LENGTH_LOW:
            {
                uint8_t low;
                rc = ring_buf_get(&ingestion->rb, &low, sizeof(uint8_t));
                __ASSERT_NO_MSG(rc == 1);
                ingestion->expected_size += low;
                ingestion->state = RCV_DATA;
            }

            break;
            case RCV_DATA:
            {
                uint8_t * data;
                rc = ring_buf_get_claim(&ingestion->rb, &data, ingestion->expected_size);
                if (rc == ingestion->expected_size)
                {
                    ingestion->observer(data, ingestion->expected_size);
                    ring_buf_get_finish(&ingestion->rb, ingestion->expected_size);
                    ingestion->state = RCV_LENGTH_HIGH;
                    k_work_cancel_delayable(&ingestion->timeout_work);
                }
            }
            break;
        }
    } while(ring_buf_size_get(&ingestion->rb) != 0);
}

