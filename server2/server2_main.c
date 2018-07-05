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
#include <sys/wait.h>
#include <sys/prctl.h>

#include "../serve.h"

/* GLOBAL VARIABLES */
char *prog_name;

/* PROTOTYPES */
void sig_chld(int signo);

/****************************************************************************
 * this server is a single-stack IPv6 that serves IPv4 too on a single socket
 * with IPv4-mapped addresses. It supports concurrency with on-demand process
 * creation at handles zombie processes in order to be as robust as possibile.
 * It handles every errors, avoiding unexpected disruptions by the server
 ****************************************************************************/
int main(int argc, char *argv[])
{
  int listenfd, s;              /* sockets */
  struct sockaddr_storage ss;   /* struct for sockaddr opaque storage */
  socklen_t len;                /* sizeof the sockaddr */
  char ipstr[INET6_ADDRSTRLEN]; /* used to store the client network address */
  pid_t childpid;               /* pid of child process */
  struct timeval timeout;

  /* for errlib to know the program name */
  prog_name = argv[0];

  /* check arguments */
  if (argc != 2)
    err_quit("Usage: %s <port>", prog_name);

  len = sizeof(ss);

  /**********************************************************************
   * tcp_listen by Stevens modified by Luigi Ferrettino in order to have
   * only IPv6 and IPv4-mapped IPv6, so one stack for both protocols.
   **********************************************************************/
  s = tcp_listen(NULL, argv[1], &len);

  /* signal handler to avoid zombie processes */
  Signal(SIGCHLD, sig_chld);

  /***********************************************************************
   * ignore the SIGPIPE and handle errors directly in the code in order to 
   * increase robustness and prevent unexpected behaviours.
   ***********************************************************************/
  Signal(SIGPIPE, SIG_IGN);

  listenfd = s;

  printf("ready\n\n");

  /* create the timeout */
  timeout.tv_sec = 55;
  timeout.tv_usec = 0;

  printf("PID\tMESSAGE\n");
  fflush(stdout);

  /* infinite loop */
  for (;;)
  {
    s = Accept(listenfd, (SA *)&ss, &len);

    /* set the timeout on the input socket to not wait forever */
    Setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    
    /* get the IP address of the client */
    Getpeername(s, (SA *)&ss, &len);

    /* deal with both IPv6 and IPv4-mapped IPv6 addresses */
    if (ss.ss_family == AF_INET6)
    {
      struct sockaddr_in6 *s = (struct sockaddr_in6 *)&ss;
      inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
    }
    else
    {
      err_msg("PARENT\t(%s) error - client socket family not valid, closing...\n", prog_name);
      Close(s);
      continue; 
    }

    /* fork a new process to serve the client on the new connection */
    if ((childpid = fork()) < 0)
    {
      err_msg("(%s) error - fork() failed", prog_name);
      Close(s);
    }
    else if (childpid > 0)
    {
      /* parent process */
      Close(s);  /* close connected socket from the parent side (wrapped) */
    }
    else
    {
      /* child process */
      Close(listenfd);   /* close passive socket (wrapped) */

      /*********************************************************
      * child can ask kernel to deliver SIGHUP (or other signal) 
      * when parent dies by specifying option PR_SET_PDEATHSIG 
      * in prctl() syscall. This is used to properly kill childs
      * when the parent process dies. (linux only)
      **********************************************************/
      #ifdef __linux__
      prctl(PR_SET_PDEATHSIG, SIGHUP);
      #endif

      serve(s, ipstr); /* serve client */
      exit(0);         /* kill process */
    }
  }

  exit(0);
}

/* call waitpid */
void sig_chld(int signo)
{
  pid_t pid;
  int stat;

  while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
  {
    /* child terminated; it is not secure to use printf(s) here. */
  }
  return;
}
