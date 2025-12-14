#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#define BUFFER_SIZE 2000
#define PORT 5555

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
    strcpy(tun_name, "tun1"); 
    // NOTE: If testing on ONE machine, Client and Server need different TUN names!
    // We will handle this in the run instructions.

    // 1. Create TUN Interface
    int tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0) exit(1);
    printf("TUN interface %s created.\n", tun_name);

    // 2. Create UDP Socket
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // 3. Bind the Socket (Server Specific)
    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
    server_addr.sin_port = htons(PORT);

    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    printf("Server listening on port %d...\n", PORT);

    socklen_t client_len = sizeof(client_addr);
    int client_known = 0; // Flag to check if we have heard from a client yet

    // 4. Main Loop
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(tun_fd, &read_fds);
        FD_SET(sock_fd, &read_fds);

        int max_fd = (tun_fd > sock_fd) ? tun_fd : sock_fd;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            break;
        }

        // Case A: Packet from the Real Network (UDP) -> Going to TUN
        if (FD_ISSET(sock_fd, &read_fds)) {
            // We receive the packet AND the sender's address
            int nread = recvfrom(sock_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &client_len);
            if (nread < 0) break;

            printf("Socket -> TUN: %d bytes from client\n", nread);
            
            // Mark that we know who the client is, so we can reply later
            client_known = 1;

            // Write to TUN interface (Kernel will route it)
            write(tun_fd, buffer, nread);
        }

        // Case B: Packet from TUN -> Going to Client (UDP)
        if (FD_ISSET(tun_fd, &read_fds)) {
            int nread = read(tun_fd, buffer, sizeof(buffer));
            if (nread < 0) break;

            if (client_known) {
                printf("TUN -> Socket: %d bytes to client\n", nread);
                sendto(sock_fd, buffer, nread, 0, (struct sockaddr*)&client_addr, client_len);
            } else {
                printf("TUN -> Socket: Dropped packet (No client connected yet)\n");
            }
        }
    }

    close(tun_fd);
    close(sock_fd);
    return 0;
}
