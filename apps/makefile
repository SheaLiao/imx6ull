CROSS_COMPILE=arm-linux-gnueabihf-
CC=${CROSS_COMPILE}gcc
AR=${CROSS_COMPILE}ar

# libgpiod compile install path
LIBGPIOD_PATH=libgpiod/install/

# compile flags and link flags
CFLAGS+=-I${LIBGPIOD_PATH}/include
LDFLAGS+=-L${LIBGPIOD_PATH}/lib -lgpiod

all:
	${CC} hello.c -o hello
	${CC} ${CFLAGS} leds.c -o leds ${LDFLAGS}
	${CC} ${CFLAGS} keypad.c -o keypad ${LDFLAGS}
	${CC} ${CFLAGS} pwm_test.c -o pwm_test ${LDFLAGS}
	${CC} ${CFLAGS} pwm_play.c -o pwm_play ${LDFLAGS}
	${CC} ${CFLAGS} sht20_fops.c -o sht20_fops ${LDFLAGS}
	${CC} ${CFLAGS} sht20_ioctl.c -o sht20_ioctl ${LDFLAGS}
	${CC} ${CFLAGS} spi_test.c -o spi_test ${LDFLAGS}
	${CC} ${CFLAGS} ttyS_test.c -o ttyS_test ${LDFLAGS}
	${CC} ${CFLAGS} lcd_test.c -o lcd_test ${LDFLAGS}
	${CC} ${CFLAGS} video2lcd.c -o video2lcd ${LDFLAGS}

clean:
	@rm -f hello
	@rm -f leds
	@rm -f keypad
	@rm -f pwm_test
	@rm -f pwm_play
	@rm -f sht20_fops
	@rm -f sht20_ioctl
	@rm -f spi_test
	@rm -f ttyS_test
	@rm -f lcd_test
	@rm -f video2lcd
