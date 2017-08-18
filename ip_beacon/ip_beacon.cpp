
#if defined(_MSC_VER)
#define IS_WINDOWS 1
#define IS_LINUX 0
#define IS_MACOSX 0
#include <winsock2.h>
#include <Windows.h>
//  TODO: Add IS_MACOSX detection
//  and port to Mac OS X
#else
//  default to Linux
#define IS_WINDOWS 0
#define IS_LINUX 1
#define IS_MACOSX 0
#define closesocket close
#define SOCKET int
#define INVALID_SOCKET -1
#include <time.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEFAULT_CONFIG "~/ip_beacon.ini"

/* Name to identify as (will default 
 * to the local hostname)
 */
char myname[64] = "";

/* Config file to load.
 */
char config[256] = DEFAULT_CONFIG;

/* What port to use -- browsers and beacons 
 * must agree on this.
 */
char port[16] = "42189";

/* What address to broadcast to (you may want 
 * to restrict to a specific subnet.)
 */
char address[50] = "255.255.255.255";

/* How often to send the beacon packet when idle */
char interval[20] = "5.0";

/* verbose levels:
 * 0 - errors
 * 1 - incoming packets
 * 2 - packet dispatch handling
 * 3 - broadcast messages
 */
char verbose[20] = "0";

SOCKET udpSocket = INVALID_SOCKET;
volatile bool running = true;
int verbosity = -1;

struct Option {
    char const *name;
    char *buf;
    size_t bufsz;
};

#define OPTION(x) \
    { #x, x, sizeof(x) }

static Option options[] = {
    OPTION(myname),
    OPTION(config),
    OPTION(port),
    OPTION(address),
    OPTION(interval),
    OPTION(verbose),
};

int read_option(char const *opt) {
    if (opt[0] == '-' && opt[1] == '-') {
        opt += 2;
    }
    char const *eq = strchr(opt, '=');
    if (!eq) {
        return -1;
    }
    for (size_t i = 0; i != sizeof(options)/sizeof(options[0]); ++i) {
        if (!strncmp(opt, options[i].name, strlen(options[i].name))) {
            strncpy(options[i].buf, eq+1, options[i].bufsz);
            options[i].buf[options[i].bufsz] = 0;
            return 0;
        }
    }
    return -2;
}

void usage(char const *opt) {
    if (opt) {
        fprintf(stderr, "ip_beacon: unknown option %s\n",
                opt);
    }
    for (size_t i = 0; i != sizeof(options)/sizeof(options[0]); ++i) {
        fprintf(stderr, "--%s=%s\n",
                options[i].name, options[i].buf);
    }
    exit(1);
}

int read_config(char const *fname) {
    char path[512];
    if (fname[0] == '~' && fname[1] == '/') {
        char const *HOME = getenv("HOME");
        if (!HOME) {
            return -1;
        }
        snprintf(path, 512, "%s%s", HOME, fname+1);
        path[511] = 0;
    } else {
        strncpy(path, fname, 512);
        path[511] = 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (strcmp(fname, DEFAULT_CONFIG)) {
            return -1;
        }
        //  It's OK that the default config doesn't exist
        return 0;
    }
    while (!ferror(f) && !feof(f)) {
        path[0] = 0;
        if (!fgets(path, 512, f)) {
            break;
        }
        char *nl = strchr(path, '\n');
        if (nl) {
            *nl = 0;
        }
        if (path[0] == '#' || path[0] == 0) {
            continue;
        }
        if (read_option(path) < 0) {
            fprintf(stderr, "Unknown option in %s: %s\n",
                    fname, path);
            fclose(f);
            return -1;
        }
    }
    fclose(f);
}

#if IS_LINUX
#include <signal.h>

void doquit(int) {
    running = false;
}

void setup_signals() {
    signal(SIGINT, doquit);
    signal(SIGHUP, doquit);
}
#endif

#if IS_WINDOWS
void setup_signals() {
}
#endif


bool is_verbose(int level) {
    if (verbosity < 0) {
        verbosity = atoi(verbose);
    }
    return verbosity >= level;
}

char const *sin_str(struct sockaddr_in const *sin, char *buf) {
    buf[0] = 0;
    strcpy(buf, inet_ntoa(sin->sin_addr));
    char *port = buf + strlen(buf);
    sprintf(port, ":%d", ntohs(sin->sin_port));
    return buf;
}

void parse_incoming_packet(char const *data, int size, struct sockaddr_in const *sin) {

    char str[50];
    if (is_verbose(1)) {
        fprintf(stderr, "Received packet from %s\n",
                sin_str(sin, str));
    }

    /* the reason for living -- answer calls for my presence! */

    if (!strncmp(data, "whothere", 7)) {
        //  send a response
        char response[512];
        sprintf(response, "iam %s", myname);
        int s = ::sendto(udpSocket, response, strlen(response), 0,
                (struct sockaddr const *)sin, (socklen_t)sizeof(*sin));
        if (s < 0) {
            perror("sendto()");
        } else if (is_verbose(2)) {
            fprintf(stderr, "Answered a 'whothere' to %s\n",
                    sin_str(sin, str));
        }
        return;
    }

    /* someone else is announcing themselves */

    if (!strncmp(data, "iam", 3)) {
        if (is_verbose(2)) {
            fprintf(stderr, "Ignored an incoming 'iam' from %s (%.20s)\n",
                    sin_str(sin, str), data);
        }
        return;
    }

    /* this is some other data I don't recognize */

    if (is_verbose(1)) {
        fprintf(stderr, "Packet from %s is unknown: %.20s...\n",
                sin_str(sin, str), data);
    }
}

void send_broadcast_packet(short port_i) {
    struct sockaddr_in sin = { 0 };
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port_i);
    memset(&sin.sin_addr, 0xff, sizeof(sin.sin_addr));
    char msg[512];
    sprintf(msg, "iam %s", myname);
    int s = ::sendto(udpSocket, msg, strlen(msg), 0,
            (struct sockaddr const *)&sin, (socklen_t)sizeof(sin));
    if (s < 0) {
        perror("sendto()");
    } else if (is_verbose(3)) {
        fprintf(stderr, "Sent an 'iam' as broadcast\n");
    }
}


int run_socket_listener() {

    /* parse arguments */

    char *o = NULL;
    long port_i = strtol(port, &o, 10);
    if (port_i < 1 || port_i > 65535) {
        fprintf(stderr, "Port number %s is not legal (1-65535)\n",
                port);
        return -1;
    }
    double delay = strtod(interval, &o);
    if (delay <= 0.0) {
        fprintf(stderr, "Interval %s is not legal (> 0.0)\n",
                interval);
        return -1;
    }

    /* prepare the socket */

    udpSocket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == INVALID_SOCKET) {
        perror("Creating socket failed");
        return -1;
    }
    struct sockaddr_in sin = { 0 };
    sin.sin_family = AF_INET;
    sin.sin_port = htons((short)port_i);
    if (bind(udpSocket, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "Could not bind to port %d: is another ip_beacon already running?\n",
                (short)port_i);
        closesocket(udpSocket);
        return -1;
    }
    int one = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0) {
        fprintf(stderr, "Could not turn on broadcast for socket\n");
        closesocket(udpSocket);
        return -1;
    }

    /* run the main loop */

    fd_set fds;
    FD_ZERO(&fds);
    int num_errors = 0;
    while (running) {
        FD_SET(udpSocket, &fds);
        struct timeval timeout = { 0 };
        timeout.tv_sec = (int)floor(delay);
        timeout.tv_usec = (int)((delay - timeout.tv_sec) * 1e6);
        int sel = ::select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
        if (sel == 1) {         //  ready to read
            socklen_t slen = (socklen_t)sizeof(sin);
            char buf[256];
            int r = ::recvfrom(udpSocket, buf, 256, 0, (struct sockaddr *)&sin, (socklen_t *)&slen);
            if (r < 0) {        //  error
            } else {            //  solicitation!
                if (r > 255) {
                    r = 255;
                }
                buf[r] = 0;
                parse_incoming_packet(buf, r, &sin);
            }
        } else if (sel == 0) {  //  timeout
            send_broadcast_packet((short)port_i);
        } else {                //  error
            perror("select");
            if (++num_errors > 2) {
                break;
            }
        }
        if (num_errors > 0) {
            --num_errors;
        }
    }

    /* cleanup */

    closesocket(udpSocket);
    if (running == false) {
        fprintf(stderr, "ip_beacon: signal received; shutting down.\n");
    }
    return num_errors > 2 ? -1 : 0;
}


int main(int argc, char const *argv[]) {
    
    /* set up default values */

    if (gethostname(myname, sizeof(myname)) < 0) {
        perror("gethostname()");
        strcpy(myname, "unknown");
    }
    myname[sizeof(myname)-1] = 0;

    /* read options and configs */

    for (int i = 1; i != argc; ++i) {
        if (read_option(argv[i]) < 0) {
            usage(argv[i]);
        }
    }
    if (read_config(config) < 0) {
        fprintf(stderr, "ip_beacon: could not read config file %s\n",
                config);
        exit(1);
    }

    /* prepare for running */

    setup_signals();

    /* run */

    return run_socket_listener();
}

