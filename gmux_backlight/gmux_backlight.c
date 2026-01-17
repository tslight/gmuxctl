#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <machine/cpufunc.h>

// I/O port addresses for gmux.
#define GMUX_PORT_MAX_BRIGHTNESS 0x770
#define GMUX_PORT_BRIGHTNESS     0x774

// Sensible fallback max brightness value for MBP 8,3 (not 0xFFFF!). The value
// ~0x24EC4~ (151236 decimal) comes from the Linux apple-gmux driver source
// code. MBP 8,3 uses the gmux v2 protocol which probably has this max value.
#define GMUX_MAX_BRIGHTNESS_FALLBACK 0x24EC4

// Sanity bounds for validating the max brightness reading. Minimum 0x10000
// (65536) because we expect more precision than 16-bit (~0xFFFF~). Anything
// below this suggests the port isn't giving us the real max brightness
// value. Maximum 0x30000 (196608) because it's roughly double the known value
// (~0x24EC4~ ≈ 151236), giving headroom for variation across models without
// accepting absurd values.
#define GMUX_MAX_BRIGHTNESS_MIN 0x10000
#define GMUX_MAX_BRIGHTNESS_MAX 0x30000

static uint32_t get_max_brightness(void) {
  uint32_t max_val;

  max_val = inl(GMUX_PORT_MAX_BRIGHTNESS);

  printf("Detected max: 0x%x (%d decimal)...", max_val, max_val);

  if (max_val >= GMUX_MAX_BRIGHTNESS_MIN &&
      max_val <= GMUX_MAX_BRIGHTNESS_MAX) {
    printf(" Seems reasonable!\n");
    return max_val;
  }

  printf(" Bonkers! Using fallback 0x%x (%d decimal)\n",
	 GMUX_MAX_BRIGHTNESS_FALLBACK, GMUX_MAX_BRIGHTNESS_FALLBACK);
  return GMUX_MAX_BRIGHTNESS_FALLBACK;
}

static int parse_percent(const char *str, int *result) {
  // It's safer to use strtol over atoi as we verify the input as a valid
  // integer whereas atoi will just return 0 if not a valid int.
  char *endptr;
  long val;

  errno = 0; // Global so clear before use.

  // Using strtol "to long" function instead of "to unsigned long" as strtoul
  // silently wraps negative numbers, such that -1 would give us
  // 18446744073709551615 on 64-bit!
  val = strtol(str, &endptr, 10); // last argument is base

  // If endptr points to same address as str then we have no digits.
  if (endptr == str) {
    fprintf(stderr, "Error: no digits found in '%s'\n", str);
    return -1;
  }

  // De-reference and check if the value after any digits is anything but the
  // null terminator.
  if (*endptr != '\0') {
    fprintf(stderr, "Error: trailing garbage after number: '%s'\n", endptr);
    return -1;
  }

  // ERANGE is if the value was outside the bounds of LONG_MIN/MAX.
  if (errno == ERANGE || val < 0 || val > 100) {
    fprintf(stderr, "Error: value must be 0-100\n");
    return -1;
  }

  *result = (int)val;
  return 0;
}

int main(int argc, char *argv[]) {
  int fd, pct;
  uint32_t val, max_brightness;

  // Grants I/O port access for the lifetime of the open fd
  fd = open("/dev/io", O_RDWR);
  if (fd < 0) {
    perror("opening /dev/io - must be run as root!");
    return 1;
  }

  max_brightness = get_max_brightness();

  if (argc == 1) {
    val = inl(GMUX_PORT_BRIGHTNESS);
    printf("Current brightness: 0x%x (%d%%)\n",
	   val, (int)((uint64_t)val * 100 / max_brightness));
  } else {
    if (parse_percent(argv[1], &pct) != 0) {
      close(fd);
      return 1;
    }
    // Inverse of the read formula. Cast to uint64_t in the multiplication
    // prevents overflow in the unlikely case that max brightness exceeds 32
    // bit space, say 0xFFFFFFFF
    val = (uint32_t)((uint64_t)pct * max_brightness / 100);

    outl(GMUX_PORT_BRIGHTNESS, val);
    printf("Set brightness to %d%% (0x%x)\n", pct, val);
  }

  close(fd);
  return 0;
}
