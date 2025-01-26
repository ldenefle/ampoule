/**
 * @file command
 * @author Lucas Denefle - ldenefle@gmail.com
 * @date 2023-12-16 12:57:17
 * @brief Process commands and return response
 *
 */

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include "command.pb.h"
#include "errno.h"
#include "ampoule/command.h"
#include "zephyr/logging/log.h"
#include "zephyr/drivers/gpio.h"

/******************************************************************************/
/* Local Constant, Macro and Type Definitions                                 */
/******************************************************************************/
LOG_MODULE_REGISTER(command, CONFIG_AMPOULE_LOG_LEVEL);

/******************************************************************************/
/* Local Function Prototypes                                                  */
/******************************************************************************/
#define LED0_NODE DT_ALIAS(led0)

/******************************************************************************/
/* Local Variable Definitions                                                 */
/******************************************************************************/

/******************************************************************************/
/* Global Function Definitions                                                */
/******************************************************************************/

/******************************************************************************/
/* Local Function Definitions                                                 */
/******************************************************************************/
int command_handle_led(ampoule_Command *command, ampoule_Response *response)
{
#if !DT_NODE_EXISTS(LED0_NODE)
	return -ENOSYS;
#else

	int rc = 0;
	ampoule_Led *led = &command->operation.led;

	const struct gpio_dt_spec dev = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

	if (!gpio_is_ready_dt(&dev)) {
		return -EIO;
	}

	rc = gpio_pin_configure_dt(&dev, GPIO_OUTPUT_ACTIVE);
	if (rc < 0) {
		return rc;
	}

	switch (led->color) {
	case ampoule_Led_Color_WHITE:
		gpio_pin_set_dt(&dev, 1);
		break;
	case ampoule_Led_Color_OFF:
		gpio_pin_set_dt(&dev, 0);
		break;
	}

	return 0;
#endif
}

int command_process(ampoule_Command *command, ampoule_Response *response)
{
	int rc = 0;

	switch (command->opcode) {
	case ampoule_Opcode_PING:
		response->opcode = ampoule_Opcode_PONG;
		break;
	case ampoule_Opcode_SET_LED:
		response->opcode = ampoule_Opcode_SET_LED;
		rc = command_handle_led(command, response);
		break;
	default:
		rc = -ENOSYS;
		break;
	}

	response->success = rc == 0;

	return rc;
}
