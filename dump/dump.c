#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <machine/cpufunc.h>

/* Known gmux ports - MBP 8,x series */
#define GMUX_PORT_VERSION_MAJOR    0x704
#define GMUX_PORT_VERSION_MINOR    0x705
#define GMUX_PORT_VERSION_RELEASE  0x706
#define GMUX_PORT_SWITCH_DISPLAY   0x710
#define GMUX_PORT_SWITCH_DDC       0x728
#define GMUX_PORT_DISCRETE_POWER   0x750
#define GMUX_PORT_MAX_BRIGHTNESS   0x770
#define GMUX_PORT_BRIGHTNESS       0x774

/* Alternative port addresses used on some models */
#define ALT_GMUX_PORT_SWITCH_DISPLAY   0x728
#define ALT_GMUX_PORT_SWITCH_DDC       0x710
#define ALT_GMUX_PORT_DISCRETE_POWER   0x724

int main(int argc, char *argv[]) {
    int fd;

    fd = open("/dev/io", O_RDWR);
    if (fd < 0) {
	perror("open /dev/io");
	return 1;
    }

    printf("=== Gmux Register Dump ===\n\n");

    printf("Version registers:\n");
    printf("  0x704 (major):   0x%02x\n", inb(0x704));
    printf("  0x705 (minor):   0x%02x\n", inb(0x705));
    printf("  0x706 (release): 0x%02x\n", inb(0x706));

    printf("\nSwitch registers (standard addresses):\n");
    printf("  0x710: 0x%02x (byte) / 0x%08x (dword)\n", inb(0x710), inl(0x710));
    printf("  0x724: 0x%02x\n", inb(0x724));
    printf("  0x728: 0x%02x (byte) / 0x%08x (dword)\n", inb(0x728), inl(0x728));
    printf("  0x740: 0x%02x\n", inb(0x740));
    printf("  0x750: 0x%02x\n", inb(0x750));

    printf("\nBrightness registers:\n");
    printf("  0x770 (max): 0x%08x\n", inl(0x770));
    printf("  0x774 (cur): 0x%08x\n", inl(0x774));

    printf("\nScanning for interesting values (0x700-0x7ff):\n");
    for (int port = 0x700; port < 0x800; port += 4) {
	uint32_t val = inl(port);
	uint8_t valb = inb(port);
	if (val != 0 && val != 0xffffffff) {
	    printf("  0x%03x: byte=0x%02x dword=0x%08x\n", port, valb, val);
	}
    }

    close(fd);
    return 0;
}
