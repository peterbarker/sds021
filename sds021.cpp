#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

const speed_t baud = B9600;

int serial_fd;

void open_and_configure_serial(const char *serial_path)
{
    // open serial port
    serial_fd = open(serial_path, O_RDWR| O_NONBLOCK | O_NDELAY);
    if (serial_fd == -1) {
        fprintf(stderr, "test-bootloader: Failed to open (%s): %s\n", serial_path, strerror(errno));
        abort();
    }

    // get current serial port configuration:
    struct termios options;
    if (tcgetattr(serial_fd, &options) == -1) {
        fprintf(stderr, "Failed to get serial options: %m\n");
        exit(1);
    }

    // set baud rate in options
    cfsetspeed(&options, B9600);

    // apply setting to serial port:
    if (tcsetattr(serial_fd, TCSANOW, &options) == -1) {
        fprintf(stderr, "Failed to set serial options: %m\n");
        exit(1);
    }
}

void usage()
{
    fprintf(stderr, "sds021 serial-port-path\n");
    exit(1);
}

class SDS021Reading
{
public:
    uint16_t pm25; //10ug/m^3
    uint16_t pm10; //10ug/m^3
};

class SDS021Parser
{

public:

    SDS021Parser(int _serial_fd) :
        serial_fd(_serial_fd),
        state(sds021_state_want_header)
    {}

    void update();
    void yield_message();

private:

    int serial_fd;
    typedef enum {
        sds021_state_want_header,
        sds021_state_want_command,
        sds021_state_want_data1,
        sds021_state_want_data2,
        sds021_state_want_data3,
        sds021_state_want_data4,
        sds021_state_want_data5,
        sds021_state_want_data6,
        sds021_state_want_checksum,
        sds021_state_want_tail,
    } parser_state_t;
    parser_state_t state;

    uint32_t bad_chars;
    uint32_t checksum_failures;

    SDS021Reading reading;
    uint8_t checksum;
};

void SDS021Parser::yield_message()
{
    char timestamp[30];
    struct tm *tmp;
    time_t t;
    t = time(NULL);
    tmp = localtime(&t);

    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tmp);
    ::fprintf(stderr, "%s: PM1.0=%0.1f PM2.5=%0.1f (bad=%u cksum fails=%u)\n", timestamp, reading.pm10/10.0, reading.pm25/10.0, bad_chars, checksum_failures);
}


void SDS021Parser::update()
{
    uint8_t byte;
    const ssize_t num_read = read(serial_fd, (char *)&byte, 1);
    if (num_read == -1) {
        if (errno == EAGAIN) {
            return;
        }
        fprintf(stderr, "Failed to read from serial port: %s", strerror(errno));
        exit(1);
    }
    if (num_read == 0) {
        return;
    }

    // fprintf(stderr, "Got byte (0x%02x) (state=%u)\n", byte, state);
    switch(state) {
    case sds021_state_want_header:
        switch(byte) {
        case 0xAA:
            state = sds021_state_want_command;
            break;
        default:
            bad_chars++;
            break;
        }
        break;
    case sds021_state_want_command:
        switch(byte) {
        case 0xC0:
            state = sds021_state_want_data1;
            break;
        default:
            bad_chars++;
            state = sds021_state_want_header;
            break;
        }
        break;
    case sds021_state_want_data1:
        reading.pm25 = byte;
        state = sds021_state_want_data2;
        checksum = byte;
        break;
    case sds021_state_want_data2:
        reading.pm25 |= byte<<8;
        state = sds021_state_want_data3;
        checksum += byte;
        break;
    case sds021_state_want_data3:
        reading.pm10 = byte;
        state = sds021_state_want_data4;
        checksum += byte;
        break;
    case sds021_state_want_data4:
        reading.pm10 |= byte<<8;
        state = sds021_state_want_data5;
        checksum += byte;
        break;
    case sds021_state_want_data5:
        state = sds021_state_want_data6;
        checksum += byte;
        break;
    case sds021_state_want_data6:
        state = sds021_state_want_checksum;
        checksum += byte;
        break;
    case sds021_state_want_checksum:
        if (byte != checksum) {
            checksum_failures++;
            fprintf(stderr, "checksum failure\n");
            state = sds021_state_want_header;
            break;
        }
        state = sds021_state_want_tail;
        yield_message();
        break;
    case sds021_state_want_tail:
        if (byte != 0xAB) {
            bad_chars++;
        }
        state = sds021_state_want_header;
        break;
    }
}


int main(int argc, const char *argv[])
{
    const char *serial_path = argv[1];
    if (argc < 2) {
        usage();
    }
    open_and_configure_serial(serial_path);

    SDS021Parser parser = SDS021Parser(serial_fd);
    while (1) {
        parser.update();
    }

    close(serial_fd);
}
