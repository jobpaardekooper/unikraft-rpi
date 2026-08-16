/* SPI smoke-test for Unikraft on Raspberry Pi 3
 *
 * Tests:
 *   1. Init   - configure GPIO and SPI0 controller
 *   2. TX/RX  - send 4 bytes, print what comes back
 *   3. Loopback check - wire GPIO 10 (MOSI, pin 19) to GPIO 9 (MISO, pin 21)
 *              for RX == TX to pass
 *   4. Clock change - repeat transfer at 1 MHz
 */

#include <uk/print.h>
#include <uk/spi.h>
#include <stdio.h>

static const unsigned char tx[] = { 0xA5, 0x5A, 0xDE, 0xAD };
static unsigned char rx[4];

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);
	int ret, i, ok;
	printf("=== SPI smoke-test start ===\n");

	/* 1. Init */
	ret = spi_init();
	if (ret) {
		uk_pr_err("FAIL: bcm2835_spi_init() = %d\n", ret);
		return 1;
	}
	printf("PASS: SPI0 init OK (500 kHz, CS0)\n");

	/* 2. Transfer */
	struct spi_transfer xfer = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len    = 4,
		.cs     = SPI_CS0,
	};

	ret = spi_transfer(&xfer);
	if (ret) {
		uk_pr_err("FAIL: transfer timed out\n");
		return 1;
	}

	printf("TX:");
	for (i = 0; i < 4; i++) printf(" %02X", tx[i]);
	printf("\n");
	printf("RX:");
	for (i = 0; i < 4; i++) printf(" %02X", rx[i]);
	printf("\n");

	/* 3. Loopback check (only passes if MOSI wired to MISO) */
	ok = 1;
	for (i = 0; i < 4; i++)
		if (rx[i] != tx[i]) { ok = 0; break; }
	printf(ok
		? "PASS: loopback RX == TX\n"
		: "NOTE: RX != TX (connect pin 19 to pin 21 for loopback)\n");

	/* 4. 1 MHz clock change + repeat */
	spi_set_clock(1000000);
	printf("Clock changed to 1 MHz\n");

	ret = spi_transfer(&xfer);
	if (ret) {
		uk_pr_err("FAIL: 1 MHz transfer timed out\n");
		return 1;
	}
	printf("PASS: 1 MHz transfer OK\n");

	printf("=== SPI smoke-test done ===\n");
	return 0;
}
