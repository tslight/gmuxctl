#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <machine/cpufunc.h>

#define GMUX_PORT_DISCRETE_POWER   0x724
#define GMUX_PORT_INTEGRATED_POWER 0x740
#define GMUX_PORT_SWITCH_DISPLAY   0x728
#define GMUX_PORT_SWITCH_DDC       0x710

/* Some models use 0x02 or 0x03 for different power states, see README */
#define GMUX_DGPU_POWER_OFF  0x00
#define GMUX_DGPU_POWER_ON   0x01

static void print_status(void) {
  uint8_t dgpu_power, igpu_power;
  uint32_t display_mux;
  uint8_t ddc_mux;

  dgpu_power = inb(GMUX_PORT_DISCRETE_POWER);
  igpu_power = inb(GMUX_PORT_INTEGRATED_POWER);
  display_mux = inl(GMUX_PORT_SWITCH_DISPLAY);
  ddc_mux = inb(GMUX_PORT_SWITCH_DDC);

  printf("=== Gmux Status ===\n");
  printf("Discrete GPU power:   0x%02x (%s)\n",
	 dgpu_power, dgpu_power ? "ON" : "OFF");
  printf("Integrated GPU power: 0x%02x (%s)\n",
	 igpu_power, igpu_power ? "ON" : "OFF");
  printf("Display mux:          0x%08x (%s)\n",
	 display_mux, (display_mux & 0x1) ? "discrete" : "integrated");
  printf("DDC mux:              0x%02x (%s)\n",
	 ddc_mux, (ddc_mux & 0x1) ? "discrete" : "integrated");
}

int main(int argc, char *argv[]) {
  int fd;

  fd = open("/dev/io", O_RDWR);
  if (fd < 0) {
    perror("open /dev/io");
    fprintf(stderr, "Must run as root!\n");
    return 1;
  }

  if (argc == 1) print_status();
  else if (argc == 2) {
    if (argv[1][0] == '0') {
      printf("Powering OFF discrete GPU...\n");

      /* First, ensure display is on integrated */
      outb(GMUX_PORT_SWITCH_DDC, 0x00);
      usleep(100000);  /* 100ms delay */
      outl(GMUX_PORT_SWITCH_DISPLAY, 0x02);  /* Switch to integrated */
      usleep(100000);

      /* Double check if display is currently on discrete GPU */
      uint32_t display_mux = inl(GMUX_PORT_SWITCH_DISPLAY);
      if (display_mux & 0x1) {
	fprintf(stderr, "ERROR: Display is still on discrete GPU!\n");
	close(fd);
	return 1;
      }

      /* Power off discrete */
      outb(GMUX_PORT_DISCRETE_POWER, GMUX_DGPU_POWER_OFF);
      usleep(100000);

      printf("Discrete GPU powered off.\n");
      print_status();
    }
    else if (argv[1][0] == '1') {
      printf("Powering ON discrete GPU...\n");
      outb(GMUX_PORT_DISCRETE_POWER, GMUX_DGPU_POWER_ON);
      usleep(500000);  /* 500ms for GPU to initialize */
      printf("Discrete GPU powered on.\n");
      print_status();
    }
    else {
      fprintf(stderr, "Usage: %s [0|1]\n", argv[0]);
      fprintf(stderr, "  0 = power off discrete GPU\n");
      fprintf(stderr, "  1 = power on discrete GPU\n");
      close(fd);
      return 1;
    }
  }
  else {
    fprintf(stderr, "Usage: %s [0|1]\n", argv[0]);
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}
