#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int serial_open_nonblock(const char *devpath)
{
    int bits;
    struct termios settings_orig;
    struct termios settings;
    int fd;

    fd = open(devpath, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }

    if (ioctl(fd, TIOCEXCL) == -1) {
        close(fd);
        printf("unable to get exclussive access to port %s\n", devpath);
        return -1;
    }

    if (ioctl(fd, TIOCMGET, &bits) < 0) {
        close(fd);
        printf("unable to query serial port signals on %s\n", devpath);
        return -1;
    }

    bits &= ~(TIOCM_DTR | TIOCM_RTS);
    if (ioctl(fd, TIOCMSET, &bits) < 0) {
        close(fd);
        printf("unable to control serial port signals on %s\n", devpath);
        return -1;
    }

    if (tcgetattr(fd, &settings_orig) < 0) {
        close(fd);
        printf("unable to access baud rate on port %s\n", devpath);
        return -1;
    }

    memset(&settings, 0, sizeof(settings));
    settings.c_cflag = CS8 | CLOCAL | CREAD | HUPCL;
    settings.c_iflag = IGNBRK | IGNPAR;

    speed_t baud = B115200;
    cfsetospeed(&settings, (speed_t)baud);
    cfsetispeed(&settings, (speed_t)baud);
    if (tcsetattr(fd, TCSANOW, &settings) < 0)
        return -1;

    tcflush(fd, TCIFLUSH);

    return fd;
}
