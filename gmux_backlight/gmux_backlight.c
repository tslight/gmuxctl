#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <machine/cpufunc.h>

/* I/O port address where the gmux chip listens for brightness commands. */
#define GMUX_PORT_BRIGHTNESS 0x774

int main(int argc, char *argv[]) {
  int fd;
  uint32_t val;

  fd = open("/dev/io", O_RDWR);
  if (fd < 0) {
    perror("open /dev/io");
    return 1;
  }

  if (argc == 1) {
    /* Read current brightness */
    val = inl(GMUX_PORT_BRIGHTNESS);
    printf("Current brightness: 0x%x (%d%%)\n", val, val * 100 / 0xFFFF);
  } else {
    int pct = atoi(argv[1]); // should probably use strtol() or stroul()
    if (pct < 0 || pct > 100) {
      fprintf(stderr, "Value must be 0-100\n");
      close(fd);
      return 1;
    }
    /* Inverse of the read formula */
    val = pct * 0xFFFF / 100;
    /* write brightness */
    outl(GMUX_PORT_BRIGHTNESS, val);
    printf("Set brightness to %d%% (0x%x)\n", pct, val);
  }

  close(fd);
  return 0;
}
