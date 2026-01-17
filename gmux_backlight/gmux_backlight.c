#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <machine/cpufunc.h>

/* I/O port addresses for gmux */
#define GMUX_PORT_MAX_BRIGHTNESS 0x770
#define GMUX_PORT_BRIGHTNESS     0x774

/*
  Sensible fallback max brightness value for MBP 8,3 (not 0xFFFF!). The value
  ~0x24EC4~ (151236 decimal) comes from the Linux apple-gmux driver source
  code. MBP 8,3 uses the "gmux v2" protocol which has this specific max value.
 */
#define GMUX_MAX_BRIGHTNESS_FALLBACK 0x24EC4

/*
  Sanity bounds for validating the max brightness reading. 0x10000 (65536) as
  minimum because we expect more precision than 16-bit (~0xFFFF~), so anything
  below this suggests the port isn't giving us the real max brightness
  value. 0x30000 (196608) as maximum because it's roughly double the known
  value (~0x24EC4~ ≈ 151236), giving headroom for variation across models
  without accepting absurd values
 */
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

int main(int argc, char *argv[]) {
  int fd;
  uint32_t val, max_brightness;

  fd = open("/dev/io", O_RDWR);
  if (fd < 0) {
    perror("open /dev/io");
    return 1;
  }

  max_brightness = get_max_brightness();

  if (argc == 1) {
    /* Read current brightness */
    val = inl(GMUX_PORT_BRIGHTNESS);
    printf("Current brightness: 0x%x (%d%%)\n",
	   val, (int)((uint64_t)val * 100 / max_brightness));
  } else {
    int pct = atoi(argv[1]);
    if (pct < 0 || pct > 100) {
      fprintf(stderr, "Value must be 0-100\n");
      close(fd);
      return 1;
    }
    /*
      Inverse of the read formula. Cast to ~uint64_t~ in the multiplication
      prevents overflow
     */
    val = (uint32_t)((uint64_t)pct * max_brightness / 100);
    /* write brightness */
    outl(GMUX_PORT_BRIGHTNESS, val);
    printf("Set brightness to %d%% (0x%x)\n", pct, val);
  }

  close(fd);
  return 0;
}
