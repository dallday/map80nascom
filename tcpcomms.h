#ifndef TCPCOMMS_H
#define TCPCOMMS_H

/*
 * Tiny non-blocking RA/WA command-protocol server, exposed as three calls
 * meant to be driven from someone else's main loop:
 *
 *   int lfd = server_setup(port);   // once, at startup. Returns -1 on error.
 *   while (running) {
 *       server_poll(lfd);           // once per loop iteration - never blocks
 *       ... do your own work here ...
 *   }
 *   server_close(lfd);              // once, at shutdown
 *
 * server_poll() checks the listening socket and every connected client for
 * pending data, services whatever is ready, and returns immediately - it
 * never waits for a connection or for a full message to arrive. That means
 * your loop stays free to do other work every iteration; just call
 * server_poll() again next time round to keep the protocol moving.
 */

int  tcpcomms_setup(int port);
void tcpcomms_poll(int lfd);
void tcpcomms_close(int lfd);

void send_data_to_all_clients( const char *reason);


#endif
