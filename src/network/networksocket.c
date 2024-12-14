#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libubox/blobmsg_json.h>

#include "utils.h"
#include "memory_utils.h"
#include "multicastsocket.h"
#include "broadcastsocket.h"
#include "msghandler.h"
#include "datastorage.h"
#include "networksocket.h"


/* Network Defines */
#define MAX_RECV_STRING 2048

/* Network Attributes */
static int sock;
static struct sockaddr_in addr;
static const char *ip;
static unsigned short port;
static char recv_string[MAX_RECV_STRING + 1];
static int recv_string_len;
static int multicast_socket;

static pthread_mutex_t send_mutex;

void *receive_msg(void *args);

int init_socket_runopts(const char *_ip, int _port, int _multicast_socket) {

    port = _port;
    ip = _ip;
    multicast_socket = _multicast_socket;

    if (multicast_socket) {
        dawnlog_info("Settingup multicastsocket!\n");
        sock = setup_multicast_socket(ip, port, &addr);
    } else {
        sock = setup_broadcast_socket(ip, port, &addr);
    }

    pthread_t sniffer_thread;
    if (pthread_create(&sniffer_thread, NULL, receive_msg, NULL)) {
        dawnlog_error("Could not create receiving thread!\n");
        return -1;
    }

    dawnlog_info("Connected to %s:%d\n", ip, port);

    return 0;
}

// TODO: Will all messages arrive in a single read? Borrow looping partial read from tcpsocket.c?
static void* receive_msg_inner(void* args, bool is_enc) {

    while (1) {
        recv_string_len = recvfrom(sock, recv_string, MAX_RECV_STRING, 0, NULL, 0);

        //FIXME: Next few lines look a bit mangled, with odd strlen() tests, etc...
        if (recv_string_len < 0) {
            dawnlog_error("Could not receive message!");
            continue;
        }

        if (recv_string_len == 0) {
            return 0;
        }

        char* final_msg = NULL;
        final_msg = recv_string;

        dawnlog_debug("Received network message: %s\n", final_msg);
        handle_network_msg(final_msg);

    }
}

void* receive_msg(void* args) {
    return receive_msg_inner(args, false);
}

int send_string(char *msg, bool is_enc) {
    dawn_mutex_lock(&send_mutex);

    char* final_msg = NULL;
    size_t msglen = 0;

    // Include NUL terminator in message
    final_msg = msg;
    msglen = strlen(msg) + 1;

    if (sendto(sock,
        final_msg,
        msglen,
        0,
        (struct sockaddr*)&addr,
        sizeof(addr)) < 0) {
        dawnlog_perror("sendto()");

        dawn_mutex_unlock(&send_mutex);

        exit(EXIT_FAILURE);
    }

    dawn_mutex_unlock(&send_mutex);
    return 0;
}

void close_socket() {
    if (multicast_socket) {
        remove_multicast_socket(sock);
    }
    close(sock);
}
