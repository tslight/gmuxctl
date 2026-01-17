#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
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

// Parsing modes
#define MODE_ABSOLUTE 0
#define MODE_RELATIVE 1

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

static int parse_brightness_arg(const char *str, int *value, int *mode) {
  char *endptr;
  long val;
  const char *num_start = str;

  // Check for explicit +/- prefix
  if (str[0] == '+') {
    *mode = MODE_RELATIVE;
    num_start = str + 1;
  } else if (str[0] == '-') {
    *mode = MODE_RELATIVE;
    num_start = str;
  } else {
    *mode = MODE_ABSOLUTE;
  }

  errno = 0; // Global so clear before use.

  // It's safer to use strtol over atoi as we verify the input as a valid
  // integer whereas atoi will just return 0 if not a valid int.
  // Using strtol "to long" function instead of "to unsigned long" as strtoul
  // silently wraps negative numbers, such that -1 would give us
  // 18446744073709551615 on 64-bit!
  val = strtol(num_start, &endptr, 10);

  // If endptr points to same address as num_start then we have no digits.
  if (endptr == num_start) {
    fprintf(stderr, "ERROR: no digits found in '%s'\n", num_start);
    return -1;
  }

  // De-reference and check if the value after any digits is anything but the
  // null terminator.
  if (*endptr != '\0') {
    fprintf(stderr, "ERROR: trailing garbage after number: '%s'\n", endptr);
    return -1;
  }

  // ERANGE is if the value was outside the bounds of LONG_MIN/MAX.
  if (errno == ERANGE) {
    fprintf(stderr, "ERROR: number out of range\n");
    return -1;
  }

  if (*mode == MODE_ABSOLUTE) {
    if (val < 0 || val > 100) {
      fprintf(stderr, "ERROR: absolute value must be 0-100\n");
      return -1;
    }
  } else {
    if (val < -100 || val > 100) {
      fprintf(stderr, "ERROR: relative value must be -100 to +100\n");
      return -1;
    }
  }

  *value = (int)val;
  return 0;
}

int main(int argc, char *argv[]) {
  int fd, value, mode, pct;
  uint32_t raw_val, max_brightness;

  // Grants I/O port access for the lifetime of the open fd
  fd = open("/dev/io", O_RDWR);
  if (fd < 0) {
    perror("ERROR opening /dev/io");
    fprintf(stderr, "Are you running as root (with sudo)?\n");
    return 1;
  }

  max_brightness = get_max_brightness();

  if (argc == 1) {
    raw_val = inl(GMUX_PORT_BRIGHTNESS);
    // max_brightness/2 before division rounds to nearest integer, otherwise
    // rounding errors in the integer division truncates the result.
    pct = (int)(((uint64_t)raw_val * 100 + max_brightness / 2) / max_brightness);
    printf("Current brightness: 0x%x (%d%%)\n", raw_val, pct);
  } else {
    if (parse_brightness_arg(argv[1], &value, &mode) != 0) {
      close(fd);
      return 1;
    }

    if (mode == MODE_RELATIVE) {
      // Work in raw values to avoid cumulative rounding errors
      uint32_t current_raw = inl(GMUX_PORT_BRIGHTNESS);
      // Cast to signed because adjustment can be -ve, and so can new_raw
      // before clamping & temporarily cast to 64 bit to avoid overflow
      int32_t adjustment = (int32_t)(((int64_t)value * max_brightness) / 100);
      int32_t new_raw = (int32_t)current_raw + adjustment;
      if (new_raw < 0) new_raw = 0; // Clamp to valid range
      if (new_raw > (int32_t)max_brightness) new_raw = (int32_t)max_brightness;
      raw_val = (uint32_t)new_raw; // cast back to unsigned
    } else {
      // Absolute mode: convert percent to raw. Inverse of read logic.
      // Cast to uint64_t in the multiplication prevents overflow in the unlikely
      // case that max brightness exceeds 32 bit space, say 0xFFFFFFFF
      raw_val = (uint32_t)(((uint64_t)value * max_brightness) / 100);
    }

    outl(GMUX_PORT_BRIGHTNESS, raw_val);
    // Only convert to percent for display purposes. max_brightness/2 before
    // division rounds to nearest integer, otherwise rounding errors in the
    // integer division truncates and may result in inaccurate percent display
    pct = (int)(((uint64_t)raw_val * 100 + max_brightness / 2) / max_brightness);
    printf("Set brightness to %d%% (0x%x)\n", pct, raw_val);
  }

  close(fd);
  return 0;
}
