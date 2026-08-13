# Makefile

# (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

ifeq ($(BOARD),Heltec2)
  FQBN=esp32:esp32:heltec_wifi_lora_32_V2
  BOARD_MACRO=HELTEC_V2
  CHIP=esp32
  SERIAL=$(shell ls /dev/tty.usbserial-*)
else ifeq ($(BOARD),Heltec3)
  FQBN=esp32:esp32:heltec_wifi_lora_32_V3
  BOARD_MACRO=HELTEC_V3
  CHIP=esp32s3
  SERIAL=$(shell ls /dev/tty.usbserial-*)
else ifeq ($(BOARD),Heltec4)
  FQBN=esp32:esp32:esp32s3:FlashSize=16M,CDCOnBoot=cdc
  BOARD_MACRO=HELTEC_V4
  CHIP=esp32s3
  SERIAL=$(shell ls /dev/tty.usbmodem*)
else ifeq ($(BOARD),C3mini)
  FQBN=esp32:esp32:esp32c3:FlashSize=4M,CDCOnBoot=cdc
  BOARD_MACRO=C3MINI
  CHIP=esp32c3
  SERIAL=$(shell ls /dev/tty.usbmodem*)
else ifeq ($(BOARD),TBeamS) # SX1262, TBeam Supreme
  FQBN=esp32:esp32:esp32s3:FlashSize=8M,CDCOnBoot=cdc
  BOARD_MACRO=LILYGO_TBEAMS3
  CHIP=esp32s3
  SERIAL=$(shell ls /dev/tty.usbmodem*)
else ifeq ($(BOARD),T3S3v1.2) # LR1121 configured for 2.44GHz
  FQBN=esp32:esp32:esp32s3:FlashSize=4M,CDCOnBoot=cdc
  BOARD_MACRO=LILYGO_T3S3V12
  CHIP=esp32s3
  SERIAL=$(shell ls /dev/tty.usbmodem*)
else ifeq ($(BOARD),T3S3v1.3) # SX1280 configured for 2.44GHz
  FQBN=esp32:esp32:esp32s3:FlashSize=4M,CDCOnBoot=cdc
  BOARD_MACRO=LILYGO_T3S3V13
  CHIP=esp32s3
  SERIAL=$(shell ls /dev/tty.usbmodem*)
else
  $(error ">> please specify BOARD={Heltec2|Heltec3|Heltec4|C3mini|TBeamS|T3S3v1.2|T3S3v1.3}")
endif

$(info SERIAL ports are/is: $(SERIAL))

COMPILE_DATE=$(shell date -u +'%Y-%m-%d %H:%M') # UTC
$(info COMPILE_DATE=$(strip $(COMPILE_DATE)) UTC)
$(shell echo \#define COMPILE_DATE \"$(strip $(COMPILE_DATE))\" > compile_date.h)

all:	compile upload

compile:
	arduino-cli compile -b $(FQBN) --libraries libs \
	  --build-property compiler.cpp.extra_flags="-D$(BOARD_MACRO)"

upload:
	$(foreach var, $(SERIAL), arduino-cli upload -b $(FQBN) -p $(var);)

clean:
	rm -rf compile_date.h *~

# eof
