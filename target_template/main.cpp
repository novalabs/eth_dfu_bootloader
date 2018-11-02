/* COPYRIGHT (c) 2016-2017 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */

#include <nil.h>
#include <hal.h>
#include <dfu.h>
#include <usbcfg.h>
#include <hw_utils.hpp>

// CONFIGURATION
// Number of cycles (circa 1.125 seconds)
#define TIMEOUT 5
// What to do at timeout: 'B' bootload, 'A' application
#define AT_TIMEOUT 'A'

// ----------------------------------------------------------------------------

void
usb_disconnect_bus()
{
    palClearPort(GPIOA, (1 << GPIOA_OTG_FS_DM) | (1 << GPIOA_OTG_FS_DP));
    palSetPadMode(GPIOA, GPIOA_OTG_FS_DM, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(GPIOA, GPIOA_OTG_FS_DP, PAL_MODE_OUTPUT_PUSHPULL);
}

void
usb_connect_bus()
{
    palClearPort(GPIOA, (1 << GPIOA_OTG_FS_DM) | (1 << GPIOA_OTG_FS_DP));
    palSetPadMode(GPIOA, GPIOA_OTG_FS_DM, PAL_MODE_ALTERNATE(10));
    palSetPadMode(GPIOA, GPIOA_OTG_FS_DP, PAL_MODE_ALTERNATE(10));
}

THD_WORKING_AREA(usbThreadWorkingArea, 4096);
THD_FUNCTION(usbThread, arg) {
    (void)arg;

    if(RTC->BKP0R == 0xB0BAFE77) {
        RTC->BKP0R = 0xBAADF00D; // Boot application
        setMagic(DFU_MAGIC);
        osalSysDisable();

        __DSB();
        SCB->AIRCR = 0x5FA0004;
        __DSB();

        while(1) {
        }
    }

      sduObjectInit(&SDU1);
	  sduStart(&SDU1, &serusbcfg);

	  usbDisconnectBus(serusbcfg.usbp);
	  chThdSleepMilliseconds(500);
	  usbStart(serusbcfg.usbp, &usbcfg);
	  usbConnectBus(serusbcfg.usbp);

	  while(usbGetDriverStateI(serusbcfg.usbp) != USB_ACTIVE) {
      	chThdSleepMilliseconds(1);
      }

	  int32_t counter = TIMEOUT;

      while(1) {
		  msg_t tmp = chnGetTimeout((BaseChannel *)&SDU1, S2ST(1));
		  uint8_t c = '-';

		  if((tmp & 0xFFFFFF00) == 0) { // No errors or timeouts
			  c = (uint8_t)(tmp & 0x000000FF); // Received char
		  } else if(counter == 0) { // We have waited enough
			  c = AT_TIMEOUT;
		  }

		  if(RTC->BKP0R == 0xB0BAFE77) {
		      RTC->BKP0R = 0xBAADF00D; // Boot application
		      c = 'B';
		  }

		  if(c == 'B') { // Start the bootloader
			  setMagic(DFU_MAGIC);
			  osalSysDisable();

			  __DSB();
			  SCB->AIRCR = 0x5FA0004;
			  __DSB();

			  while(1) {
			  }
		  } else if(c == 'A') { // Start the application
		      RTC->BKP0R = 0xBAADF00D; // Boot application
              setMagic(DFU_MAGIC);
		      hw::Watchdog::enable(hw::Watchdog::Period::_6400_ms);

			  jumptoapp(PROGRAM_FLASH_FROM);
		  }

		  chnPutTimeout((BaseChannel *)&SDU1, '%', TIME_IMMEDIATE);

		  palSetPad(LED_GPIO, LED_PIN);
		  chThdSleepMilliseconds(125);
		  palClearPad(LED_GPIO, LED_PIN);

		  counter--;
	  }
}

THD_TABLE_BEGIN
THD_TABLE_ENTRY(
    usbThreadWorkingArea,
    "usb",
    usbThread,
    NULL
)
THD_TABLE_END

int
main(
    void
)
{
    halInit();
    chSysInit();

    while (true) {
        // ************************* //
        // *** DO NOT SLEEP HERE *** //
        // ************************* //
    }
} // main
