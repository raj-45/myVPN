#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

// Buffer size for reading packets (MTU is usually 1500)
#define BUFFER_SIZE 2000

int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("Opening /dev/net/tun");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; // IFF_NO_PI = No Packet Info header

    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        perror("ioctl(TUNSETIFF)");
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);
    return fd;
}

int main() {
    char tun_name[IFNAMSIZ];
    char buffer[BUFFER_SIZE];
    strcpy(tun_name, "tun0");

    int tun_fd = tun_alloc(tun_name);

    if (tun_fd < 0) {
        exit(1);
    }

    printf("Interface %s created. Reading packets...\n", tun_name);

    // Continuous loop to read packets
    while(1) {
        // This read blocks until a packet is available
        ssize_t nread = read(tun_fd, buffer, sizeof(buffer));
        
        if (nread < 0) {
            perror("Reading from interface");
            close(tun_fd);
            exit(1);
        }

        // Print packet info
        printf("Read %zd bytes from %s\n", nread, tun_name);
        
        // Let's print the first few bytes to see the IP header
        // First 4 bits of the first byte is the Version (4 for IPv4, 6 for IPv6)
        unsigned char version = (buffer[0] >> 4);
        printf("IP Version: %d\n", version);
        
        // Print raw hex of the first 20 bytes (typical IPv4 header size)
        for(int i = 0; i < (nread < 20 ? nread : 20); i++) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n\n");
    }

    return 0;
}
