/*********************************************************************************************************************
  *                                                   PROTOCOL
  * 
  * To request a file the client sends to the server the three ASCII characters “GET” followed by the ASCII space 
  * character and the ASCII characters of the file name, terminated by the ASCII carriage return (CR) 
  * and line feed (LF):
  * 
  * |G|E|T| |...filename...|CR|LF|
  * 
  * (NOTE: the command includes a total of 6 characters plus the characters of the file name).
  * The server replies by sending:
  * 
  * |+|O|K|CR|LF|B1|B2|B3|B4|T1|T2|T3|T4|File content.........
  * 
  * This message is composed by 5 characters followed by the number of bytes of the requested file (a 32-bit unsigned 
  * integer in network byte order - bytes B1 B2 B3 B4 in the figure), then by the timestamp of the last file   
  * modification (Unix time, i.e. number of seconds since the start of epoch, represented as a 32-bit unsigned integer
  * in network byte order - bytes T1 T2 T3 T4 in the figure) and then by the bytes of the requested file. The client 
  * can request more files using the same TCP connection, by sending many GET commands, one after the other. 
  * When it intends to terminate the communication it sends:
  * 
  * |Q|U|I|T|CR|LF|
  * 
  * (6 characters) and then it closes the communication channel. 
  * In case of error (e.g. illegal command, non-existing file) the server always replies with:
  * 
  * |-|E|R|R|CR|LF|
  * 
  * (6 characters) and then it closes the connection with the client.
  * 
  * 
  * [ author: Luigi Ferrettino (S254300) ]
  *********************************************************************************************************************/

#include <sys/time.h>

#include "../serve.h"

/* GLOBAR VARIABLES */
char *prog_name;

/*****************************************************************************
 * this server is a single-stack IPv6 that serves IPv4-mapped on IPv6 too on a
 * single socket. It is single process too, with a queue of clients. 
 * It handles every possible errors, avoiding unexpected disruptions.
 *****************************************************************************/
int main(int argc, char *argv[])
{
  int listenfd, connfd;         /* sockets listening and connected */
  struct sockaddr_storage ss;   /* opaque storage for socket addresses */
  socklen_t len;                /* size of the opeque storage */
  char ipstr[INET6_ADDRSTRLEN]; /* string that contains the IPv6 (or IPv4-MAPPED) conversion */
  struct timeval timeout;       /* time variable to be used with conncetion timeout */

  /* store the program name from argv */
  prog_name = argv[0];

  /* checking terminal commands */
  if (argc != 2)
    err_quit("Usage: %s <port>", prog_name);

  /**********************************************************************
   * tcp_listen by Stevens modified by Luigi Ferrettino in order to have
   * only IPv6 and IPv4-mapped IPv6, so one stack for both protocols.
   **********************************************************************/
  len = sizeof(ss);
  listenfd = tcp_listen(NULL, argv[1], &len);

  /***********************************************************************
   * ignore the SIGPIPE and handle errors directly in the code in order to 
   * increase robustness and prevent unexpected behaviours.
   ***********************************************************************/
  Signal(SIGPIPE, SIG_IGN);

  printf("ready\n\n");

  /* create the timeout */
  timeout.tv_sec = 55;
  timeout.tv_usec = 0;

  printf("PID\tMESSAGE\n");

  /* infinite loop */
  for (;;)
  {
    connfd = Accept(listenfd, (SA *)&ss, &len);

    /*********************************************************************************
   * set the SO_RCVTIMEO option on the connected socket to not wait forever during
   * operations of read/recv, specifying a timeval structure; then handle it through
   * the EINWOULDBLOCK errno (like the API says) in the Readn_timeo() function
   * implemented in recvfile.c
   *********************************************************************************/
    Setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    /* get the IP address of the client */
    Getpeername(connfd, (SA *)&ss, &len);

    /* deal with both IPv6 and IPv4-mapped IPv6 addresses */
    if (ss.ss_family == AF_INET6)
    {
      struct sockaddr_in6 *s = (struct sockaddr_in6 *)&ss;
      inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
    }
    else
    {
      err_msg("%d\t(%s) error - client socket family not valid, closing...", getpid(), prog_name);
      Close(connfd);
      continue;
    }

    /* serve client */
    serve(connfd, ipstr);
  }

  exit(0);
}
