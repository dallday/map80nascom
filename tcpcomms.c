/*
 * tcpcomms.c - Non-blocking text-based command-protocol server, packaged as a
 * small library: tcpcomms_setup() / tcpcomms_poll() / tcpcomms_close().
 *
 * There is no main() here and no internal loop - the caller owns the loop
 * and calls server_poll() once per iteration. See server.h for the calling
 * pattern, and main.c for a working example of a caller loop.
 *
 * Wire protocol: plain ASCII text, one command per line, each line
 * terminated by '\n' (a trailing '\r' before it is tolerated and ignored,
 * so it also works fine from tools that send CRLF). A byte value is written
 * as a 2-digit hex string, no "0x" prefix.
 *
 *   "QA\n"        -> replies "XX\n"      (current value of Port A, hex)
 *   "WA XX\n"     -> sets port  A to XX, replies "XX\n" as confirmation
 *   "QB\n"        -> replies "XX\n"      (current value of Port B, hex)
 *   "WB XX\n"     -> sets port B to XX, replies "XX\n" as confirmation
 *   "RA\n"        -> replies "XX\n"      (current value of A ready line, hex)
 *   "RB\n"        -> replies "XX\n"      (current value of B ready line, hex)
 *
 * Anything malformed (missing/invalid hex value, unknown command) gets a
 * one-line "ERR <reason>\n" reply; the connection is kept open so the
 * client can just try again, rather than being dropped.
 *
 * Everything is non-blocking: accept(), read() and write() never wait, and
 * server_poll() uses a zero-timeout select() so it always returns right
 * away, whether or not anything was ready. A command split across several
 * reads (even one byte at a time) is buffered per-client and only acted on
 * once a full line has arrived - the caller's loop is never held up
 * waiting for it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <signal.h>

#include "simz80.h"
#include "tcpcomms.h"
#include "pio.h"

#define BACKLOG        16
#define MAX_CLIENTS    64
#define LINE_BUF_SIZE  32   /* generous for "WA XX\n" (6 bytes) plus slack */

/* Registers A and B - the state our tiny protocol operates on. Only ever
 * touched from whichever single thread calls server_poll(), so no locking
 * is needed here. */
static unsigned char regA = 0;
static unsigned char regB = 0;

/* Per-client connection state. A slot is free when fd < 0. */
typedef struct {
    int fd;
    unsigned char buf[LINE_BUF_SIZE]; /* raw bytes received, not yet a complete line */
    int buf_len;
} client_t;

static client_t clients[MAX_CLIENTS];

/* ---- small helpers ------------------------------------------------------ */

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void client_close(client_t *c)
{
    if (commsdebug){
        printf("[fd %d] client disconnected\n", c->fd);
    }
    close(c->fd);
    c->fd = -1;
    c->buf_len = 0;
}

static client_t *client_add(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd < 0) {
            clients[i].fd = fd;
            clients[i].buf_len = 0;
            return &clients[i];
        }
    }
    return NULL; /* no free slot */
}

/* Parse a 2-digit (or 1-digit) hex byte, no "0x" prefix expected.
 * Returns 0 and fills *out on success, -1 on a malformed value. */
static int parse_hex_byte(const char *s, unsigned char *out)
{
    if (!s || !*s) return -1;
    char *end;
    long v = strtol(s, &end, 16);
    if (*end != '\0' || v < 0 || v > 255) return -1;
    *out = (unsigned char)v;
    return 0;
}

/* ---- protocol handling --------------------------------------------------- */

/* Write every byte of a short text reply; replies here are only a handful
 * of bytes, so a short write essentially never happens on a healthy
 * socket - if it does, we drop the connection rather than adding output
 * buffering, to keep this simple. */
static int write_all(int fd, const char *buf, int len)
{
    int off = 0;
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (int)n;
    }
    return 0;
}

static int send_hex_reply(client_t *c, unsigned char val)
{
    char line[8];
    int len = snprintf(line, sizeof(line), "%02X\n", val);
    return write_all(c->fd, line, len);
}

static void send_err(client_t *c, const char *reason)
{
    char line[64];
    int len = snprintf(line, sizeof(line), "ERR %s\n", reason);
    if (write_all(c->fd, line, len) < 0) client_close(c);
}

/* Handle one fully-received line (no trailing '\n'). */
static void handle_line(client_t *c, char *line)
{
    /* Tolerate a trailing '\r' (CRLF line endings). */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';

    char cmd[16] = {0};
    char arg[16] = {0};
    int items = sscanf(line, "%15s %15s", cmd, arg);
    if (items < 1) return; /* blank line, ignore */

    for (char *p = cmd; *p; p++) *p = (char)toupper((unsigned char)*p);

    //if (cmd[0]='R'){
        
    if (strcmp(cmd, "QA") == 0) {
        // command RA
        regA=PIODeviceReadPort(PIOPORTA);
        if (commsdebug){
            printf("[fd %d] QA -> 0x%02X\n", c->fd, regA);
        }
        
        if (send_hex_reply(c, regA) < 0) client_close(c);

    } else if (strcmp(cmd, "QB") == 0) {
        // command RB
        regB=PIODeviceReadPort(PIOPORTB);
        if (commsdebug){
            printf("[fd %d] QB -> 0x%02X\n", c->fd, regB);
        }
        if (send_hex_reply(c, regB) < 0) client_close(c);

    } else if (strcmp(cmd, "WA") == 0 || strcmp(cmd, "WB") == 0) {
        if (items < 2) {
            fprintf(stderr, "[fd %d] %s: missing hex value\n", c->fd, cmd);
            send_err(c, "missing value");
            return;
        }
        unsigned char val;
        if (parse_hex_byte(arg, &val) < 0) {
            fprintf(stderr, "[fd %d] %s: bad hex value '%s'\n", c->fd, cmd, arg);
            send_err(c, "bad hex value");
            return;
        }
        // sort our which call to make
        if (cmd[1] == 'A'){
            regA=PIODeviceWritePort(PIOPORTA,val);
        }
        else {
            regB = PIODeviceWritePort(PIOPORTB,val);
        }
        if (commsdebug){
            printf("[fd %d] %s %02X -> ack %02X\n", c->fd, cmd, val, val);
        }
        if (send_hex_reply(c, val) < 0) client_close(c);
    
    } else if (strcmp(cmd, "RA") == 0 || strcmp(cmd, "RB") == 0) {
        // check the ready lines
        // sort our which call to make
        unsigned char val;
        if (cmd[1] == 'A'){
            val=PIOReadReadyLine(PIOPORTA);
        }
        else {
            val = PIOReadReadyLine(PIOPORTB);
        }
        if (commsdebug){
            printf("[fd %d] %s -> ack %02X\n", c->fd, cmd, val);
        }
        if (send_hex_reply(c, val) < 0) client_close(c);
        

    } else {
        fprintf(stderr, "[fd %d] unknown command '%s'\n", c->fd, cmd);
        send_err(c, "unknown command");
    }
}

/* Consume as many complete lines as are currently sitting in c->buf. */
static void process_buffer(client_t *c)
{
    for (;;) {
        void *nl = memchr(c->buf, '\n', c->buf_len);
        if (!nl) {
            if (c->buf_len >= (int)sizeof(c->buf)) {
                /* Line too long for our buffer with no newline in sight -
                 * discard what we have rather than growing unbounded. */
                fprintf(stderr, "[fd %d] line too long, discarding\n", c->fd);
                c->buf_len = 0;
            }
            break;
        }

        int line_len = (int)((unsigned char *)nl - c->buf);
        char line[LINE_BUF_SIZE + 1];
        memcpy(line, c->buf, line_len);
        line[line_len] = '\0';

        handle_line(c, line);
        if (c->fd < 0) return; /* handle_line closed the connection */

        int consumed = line_len + 1; /* +1 for the '\n' itself */
        int remaining = c->buf_len - consumed;
        memmove(c->buf, c->buf + consumed, remaining);
        c->buf_len = remaining;
    }
}

/* Non-blocking read of whatever bytes happen to be available right now -
 * could be a whole line, part of one, several lines, or (spuriously)
 * nothing. */
static void client_readable(client_t *c)
{
    unsigned char tmp[64];
    ssize_t n = read(c->fd, tmp, sizeof(tmp));

    if (n == 0) { client_close(c); return; }
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return; /* nothing ready right now */
        perror("read");
        client_close(c);
        return;
    }

    for (ssize_t i = 0; i < n; i++) {
        if (c->buf_len < (int)sizeof(c->buf)) {
            c->buf[c->buf_len++] = tmp[i];
        } /* else: dropped, process_buffer() will notice and reset */
    }

    process_buffer(c);
}

/* Accept every pending connection without blocking. */
static void accept_new_connections(int lfd)
{
    for (;;) {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &clilen);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; /* none pending */
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        set_nonblocking(cfd);
        client_t *c = client_add(cfd);
        if (!c) {
            fprintf(stderr, "too many clients, rejecting fd %d\n", cfd);
            close(cfd);
            continue;
        }

        if (commsdebug){
            printf("[fd %d] client connected: %s:%d\n",
               cfd, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        }
    }
}

/* ---- public API: setup / poll / close ----------------------------------- */

int tcpcomms_setup(int port)
{
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    signal(SIGPIPE, SIG_IGN); /* don't die if a client vanishes mid-write */

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(lfd); return -1;
    }
    if (listen(lfd, BACKLOG) < 0) {
        perror("listen"); close(lfd); return -1;
    }

    set_nonblocking(lfd);

    printf("server listening on port %d (regA=0x%02X regB=0x%02X)\n",
           port, regA, regB);
    return lfd;
}

/* Called once per iteration of the caller's own loop. Checks the listening
 * socket and every connected client for pending activity, services
 * whatever is ready, and returns immediately either way - a zero-timeout
 * select() means this never blocks, even briefly. */
void tcpcomms_poll(int lfd)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(lfd, &readfds);
    int maxfd = lfd;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0) {
            FD_SET(clients[i].fd, &readfds);
            if (clients[i].fd > maxfd) maxfd = clients[i].fd;
        }
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0; /* zero timeout: poll and return immediately, never block */

    int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
    if (ready < 0) {
        if (errno == EINTR) return;
        perror("select");
        return;
    }
    if (ready == 0) return; /* nothing to do right now */

    if (FD_ISSET(lfd, &readfds)) accept_new_connections(lfd);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0 && FD_ISSET(clients[i].fd, &readfds)) {
            client_readable(&clients[i]);
        }
    }
}

/* Closes every connected client and the listening socket itself. Call once,
 * on the way out. */
void tcpcomms_close(int lfd)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0) {
            close(clients[i].fd);
            clients[i].fd = -1;
        }
    }
    close(lfd);
}

/*
 * Send a message to all clients connected to the server
 * 
 * It will add the end line so the reason should not end with \n
 * This makes the debug message on the terminal cleaner
 * 
 */

void send_data_to_all_clients( const char *reason){

    char line[64];
    int len = snprintf(line, sizeof(line), "%s\n", reason);
   
    for (int clientno=0;clientno<MAX_CLIENTS ; clientno++){
        if (clients[clientno].fd>0){
            //clients[MAX_CLIENTS];
            if (commsdebug){
                printf("sending \"%s\" to client %d\n",reason,clientno);
            }
            if (write_all(clients[clientno].fd, line, len) < 0) {
            // if (write_all(clients[clientno].fd, reason, sizeof(reason)) < 0){
                printf("Whoops error sending %s to client %d\n",reason,clientno);
            }
        
        }
    }
}


// end of code