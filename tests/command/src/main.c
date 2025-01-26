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
#include <zephyr/drivers/gpio.h>
#include "zephyr/drivers/gpio/gpio_emul.h"
#include "ampoule/command.h"
#include "command.pb.h"

/******************************************************************************/
/* Local Constant, Macro and Type Definitions                                 */
/******************************************************************************/

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
ZTEST(command_tests, test_command_ping_shall_pong)
{
	ampoule_Command command;
	command.opcode = ampoule_Opcode_PING;
	ampoule_Response response;
	zassert_ok(command_process(&command, &response));

	zassert_true(response.opcode == ampoule_Opcode_PONG);
	zassert_true(response.success == true);
}

#if DT_NODE_EXISTS(LED0_NODE)
static const struct gpio_dt_spec dev = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

ZTEST(command_tests, test_command_white_led_should_set_led_on)
{
	ampoule_Command command;
	command.opcode = ampoule_Opcode_SET_LED;
	command.which_operation = ampoule_Command_led_tag;
	command.operation.led.color = ampoule_Led_Color_WHITE;

	ampoule_Response response;
	zassert_ok(command_process(&command, &response));

	zassert_true(response.opcode == ampoule_Opcode_SET_LED);
	zassert_true(response.success == true);

	/* Make sure pin is output */
	gpio_flags_t flags0;
	gpio_emul_flags_get(dev.port, dev.pin, &flags0);
	zassert_equal(flags0 & GPIO_DIR_MASK, GPIO_OUTPUT);

	/* Make sure gpio is high  */
	zassert_equal(gpio_emul_output_get(dev.port, dev.pin), 1);
}

ZTEST(command_tests, test_command_off_led_should_set_led_off)
{
	ampoule_Command command;
	command.opcode = ampoule_Opcode_SET_LED;
	command.which_operation = ampoule_Command_led_tag;
	command.operation.led.color = ampoule_Led_Color_OFF;

	ampoule_Response response;
	zassert_ok(command_process(&command, &response));

	zassert_true(response.opcode == ampoule_Opcode_SET_LED);
	zassert_true(response.success == true);

	/* Make sure pin is output */
	gpio_flags_t flags0;
	gpio_emul_flags_get(dev.port, dev.pin, &flags0);
	zassert_equal(flags0 & GPIO_DIR_MASK, GPIO_OUTPUT);

	/* Make sure gpio is high  */
	zassert_equal(gpio_emul_output_get(dev.port, dev.pin), 0);
}
#else
ZTEST(command_tests, test_command_led_should_fail)
{
	ampoule_Command command;
	command.opcode = ampoule_Opcode_SET_LED;
	ampoule_Response response;
	zassert_not_ok(command_process(&command, &response));

	zassert_true(response.opcode == ampoule_Opcode_SET_LED);
	zassert_true(response.success == false);
}

#endif

/******************************************************************************/
/* Local Function Definitions                                                 */
/******************************************************************************/

ZTEST_SUITE(command_tests, NULL, NULL, NULL, NULL, NULL);
