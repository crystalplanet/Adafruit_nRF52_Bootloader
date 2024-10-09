/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Kuba Birecki
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "boards.h"
#include "nrfx_twi.h"
#include "uf2/configkeys.h"

__attribute__((used, section(".bootloaderConfig")))
const uint32_t bootloaderConfig[] =
{
  /* CF2 START */
  CFG_MAGIC0, CFG_MAGIC1,                       // magic
  5, 100,                                       // used entries, total entries

  204, 0x100000,                                // FLASH_BYTES = 0x100000
  205, 0x40000,                                 // RAM_BYTES = 0x40000
  208, (USB_DESC_VID << 16) | USB_DESC_UF2_PID, // BOOTLOADER_BOARD_ID = USB VID+PID, used for verification when updating bootloader via uf2
  209, 0xada52840,                              // UF2_FAMILY = 0xada52840
  210, 0x20,                                    // PINS_PORT_SIZE = PA_32

  0, 0, 0, 0, 0, 0, 0, 0
  /* CF2 END */
};

/*
 * Use the on-board ISSI chip instead of status LEDs.
 */

#define IS31FL3743A_REG_PS (0xfd)
#define IS31FL3743A_REG_PSWL (0xfe)

#define IS31FL3743A_PSWL_ENABLE (0xc5)
#define IS31FL3743A_PSWL_DISABLE (0x00)

#define IS31FL3743A_PAGE_PWM (0x00)
#define IS31FL3743A_PAGE_SCALING (0x01)
#define IS31FL3743A_PAGE_FUNCTION (0x02)

static uint8_t is31fl3743a_pixels[] = {
  0x46, 0x48, 0x47, 0x8e, 0x90, 0x8f, 0xa0, 0xa2, 0xa1, // "Ready" LED registers
  0x7c, 0x7e, 0x7d, 0x58, 0x5a, 0x59                    // Status LED registers
};

static nrfx_twi_config_t is31fl3743a_twi_config = {
  .scl = _PINNUM(0, 15),
  .sda = _PINNUM(0, 17),
  .frequency = NRF_TWI_FREQ_400K,
  .interrupt_priority = NRFX_TWI_DEFAULT_CONFIG_IRQ_PRIORITY,
  .hold_bus_uninit = false,
};

static nrfx_twi_t is31fl3743a_twi = NRFX_TWI_INSTANCE(0);

static void twi_issi_write(uint8_t reg, uint8_t val) {
  uint8_t message[2];

  message[0] = reg;
  message[1] = val;

  nrfx_twi_xfer_desc_t xfer = {
    .type = NRFX_TWI_XFER_TX | NRFX_TWI_FLAG_NO_XFER_EVT_HANDLER | NRFX_TWI_FLAG_TX_NO_STOP,
    .address = 0x20, // ISSI I2C ADDRESS
    .primary_length = 2,
    .p_primary_buf = message,
  };

  nrfx_twi_xfer(&is31fl3743a_twi, &xfer, 0);
}

static void update_color(uint8_t *rgb) {
  twi_issi_write(IS31FL3743A_REG_PSWL, IS31FL3743A_PSWL_ENABLE);
  twi_issi_write(IS31FL3743A_REG_PS, IS31FL3743A_PAGE_PWM);

  for (size_t i = 9; i < 15; i += 3) {
    twi_issi_write(is31fl3743a_pixels[i], rgb[2]);
    twi_issi_write(is31fl3743a_pixels[i + 1], rgb[1]);
    twi_issi_write(is31fl3743a_pixels[i + 2], rgb[0]);
  }
}

void board_init2(void) {
  nrfx_twi_init(&is31fl3743a_twi, &is31fl3743a_twi_config, NULL, NULL);
  nrfx_twi_enable(&is31fl3743a_twi);

  // Reset settings
  twi_issi_write(IS31FL3743A_REG_PSWL, IS31FL3743A_PSWL_ENABLE);
  twi_issi_write(IS31FL3743A_REG_PS, IS31FL3743A_PAGE_FUNCTION);
  twi_issi_write(0x2f, 0xae);

  // Set configuration & GCC registers
  twi_issi_write(IS31FL3743A_REG_PSWL, IS31FL3743A_PSWL_ENABLE);
  twi_issi_write(IS31FL3743A_REG_PS, IS31FL3743A_PAGE_FUNCTION);
  twi_issi_write(0x00, ((0x02 << 4) | (0x01 << 3) | 0x01)); // Configuration register
  twi_issi_write(0x01, 0xff); // GCC


  // Set scaling registers for the LEDs that are being used
  twi_issi_write(IS31FL3743A_REG_PSWL, IS31FL3743A_PSWL_ENABLE);
  twi_issi_write(IS31FL3743A_REG_PS, IS31FL3743A_PAGE_SCALING);
  for (size_t i = 0; i < 15; ++i) {
    // R: 255, G: 100, B: 100
    twi_issi_write(is31fl3743a_pixels[i], (i % 3 == 0) ? 0xff : 0x64 );
  }

  // Turn on the "ready" LEDs
  twi_issi_write(IS31FL3743A_REG_PSWL, IS31FL3743A_PSWL_ENABLE);
  twi_issi_write(IS31FL3743A_REG_PS, IS31FL3743A_PAGE_PWM);
  for (size_t i = 1; i < 9; i += 3) {
    twi_issi_write(is31fl3743a_pixels[i], 0xff);
  }
}

void board_teardown2(void) {
  // Turn off all LEDs
  twi_issi_write(IS31FL3743A_REG_PSWL, IS31FL3743A_PSWL_ENABLE);
  twi_issi_write(IS31FL3743A_REG_PS, IS31FL3743A_PAGE_PWM);
  for (size_t i = 0; i < 15; ++i) {
    twi_issi_write(is31fl3743a_pixels[i], 0x00);
  }

  // Uninitialize TWI
  nrfx_twi_uninit(&is31fl3743a_twi);
}

void led_state(uint32_t state) {
  uint32_t status_color;

  switch (state) {
    case STATE_USB_MOUNTED:
      status_color = 0x000000;
      break;
    case STATE_USB_UNMOUNTED:
      status_color = 0xff0000;
      break;
    case STATE_BOOTLOADER_STARTED:
    case STATE_WRITING_STARTED:
      status_color = 0xff800;
      break;
    case STATE_WRITING_FINISHED:
      status_color = 0x000000;
      break;
    case STATE_BLE_CONNECTED:
      status_color = 0x187255;
      break;
    case STATE_BLE_DISCONNECTED:
      status_color = 0x000000;
      break;
    default:
      break;

    update_color((uint8_t *) status_color);
  }
}
