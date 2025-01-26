/**
 * @file serial
 * @author Lucas Denefle - ldenefle@gmail.com
 * @date 2023-12-16 12:57:17
 * @brief Implement the serial transport for ampoule library
 *
 */

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "zephyr/sys/ring_buffer.h"
#include "pb_decode.h"
#include "pb_encode.h"

#include "ampoule/command.h"
#include "ampoule/ingestion.h"

/******************************************************************************/
/* Local Constant, Macro and Type Definitions                                 */
/******************************************************************************/
#define SERIAL_DEVICE DT_CHOSEN(ampoule_transport_serial)

static const struct device *uart_dev = DEVICE_DT_GET(SERIAL_DEVICE);

/******************************************************************************/
/* Local Function Prototypes                                                  */
/******************************************************************************/
int send_tx(void *context, uint8_t *data, uint16_t len);

/******************************************************************************/
/* Local Variable Definitions                                                 */
/******************************************************************************/

/******************************************************************************/
/* Global Function Definitions                                                */
/******************************************************************************/
RING_BUF_DECLARE(tx_ring, 1024);

static struct ingestion ingestion;
static struct ingestion_transport transport = {
	.write = send_tx,
};

static struct ingestion_rpc rpc = {
	.on_command = command_process,
};

/******************************************************************************/
/* Local Function Definitions                                                 */
/******************************************************************************/
int send_tx(void *context, uint8_t *data, uint16_t len)
{
	uint8_t *output;
	uint16_t bytes_written;

	bytes_written = ring_buf_put_claim(&tx_ring, &output, len);
	if (bytes_written < 0) {
		return -ENOMEM;
	}

	memcpy(output, data, bytes_written);

	int rc = ring_buf_put_finish(&tx_ring, bytes_written);
	if (rc < 0) {
		return rc;
	}

	uart_irq_tx_enable(uart_dev);

	return bytes_written;
}

void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t buffer[64];
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			int recv_len = uart_fifo_read(dev, buffer, sizeof(buffer));
			if (recv_len < 0) {
				recv_len = 0;
			};

			ingestion_feed(&ingestion, buffer, recv_len);
		}

		if (uart_irq_tx_ready(dev)) {
			int rb_len;
			rb_len = ring_buf_get(&tx_ring, buffer, sizeof(buffer));
			if (!rb_len) {
				uart_irq_tx_disable(dev);
				continue;
			} else {
				uart_fifo_fill(dev, buffer, rb_len);
			}
		}
	}
}

int ampoule_serial_init(void)
{
	if (!device_is_ready(uart_dev)) {
		printk("Serial device not ready!");
		return -ENODEV;
	}

	ingestion_init(&ingestion, &transport, &rpc, NULL);

	uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
	uart_irq_rx_enable(uart_dev);
	return 0;
}

SYS_INIT(ampoule_serial_init, APPLICATION, 0);
