#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h> // NEW: For socket functions
#include <netinet/in.h> // NEW: For sockaddr_in
#include <arpa/inet.h>  // NEW: For inet_addr
#include <linux/if.h>
#include <linux/if_tun.h>

#define BUFFER_SIZE 2000
#define SERVER_IP "127.0.0.1" // We will test locally first
#define SERVER_PORT 5555

int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("Opening /dev/net/tun");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

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

    // 1. Create TUN Interface
    int tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0) exit(1);
    printf("TUN interface %s created.\n", tun_name);

    // 2. Create UDP Socket (NEW)
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Define the Server Address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    printf("Connected to server %s:%d\n", SERVER_IP, SERVER_PORT);

    // 3. Main Loop with select() (NEW)
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);           // Clear the list of file descriptors
        FD_SET(tun_fd, &read_fds);    // Add TUN to the list
        FD_SET(sock_fd, &read_fds);   // Add Socket to the list

        // Max FD is needed for select()
        int max_fd = (tun_fd > sock_fd) ? tun_fd : sock_fd;

        // Wait indefinitely until data is available on EITHER interface
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Select error");
            break;
        }

        // Case A: Data available on TUN (Packet from your PC -> Going to Server)
        if (FD_ISSET(tun_fd, &read_fds)) {
            int nread = read(tun_fd, buffer, sizeof(buffer));
            if (nread < 0) break;

            printf("TUN -> Socket: %d bytes\n", nread);
            
            // Send to VPN Server
            sendto(sock_fd, buffer, nread, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        }

        // Case B: Data available on Socket (Packet from Server -> Going to your PC)
        if (FD_ISSET(sock_fd, &read_fds)) {
            int nread = recvfrom(sock_fd, buffer, sizeof(buffer), 0, NULL, NULL);
            if (nread < 0) break;

            printf("Socket -> TUN: %d bytes\n", nread);
            
            // Write into TUN so the OS handles the packet
            write(tun_fd, buffer, nread);
        }
    }

    close(tun_fd);
    close(sock_fd);
    return 0;
}
