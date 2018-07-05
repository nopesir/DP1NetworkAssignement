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

#include "../errlib.h"
#include "../sockwrap.h"
#include "../recvfile.h"

/* GLOBAL VARIABLES */
char *prog_name;

/* MAIN */
int main(int argc, char *argv[])
{
  int s;                              /* socket */
  char buf[MAXBUFLEN];                /* byte buffer */
  uint32_t len, dimension, timestamp; /* unsigned 32bit varibles */
  struct timeval tval;                /* uset to set ti TIMEOUT with setsockopt() */

  /* store the program name from argv */
  prog_name = argv[0];

  /* checking terminal commands */
  if (argc < 4)
  {
    err_quit("Usage: %s <IPv4/IPv6 address> <port number> <filename> [<filename>...]\n", prog_name);
  }

  printf("NOTE: for IPv6 addresses, specify the interface with (%%) at the end of it.\n");

  /*****************************************************
   * modified tcp_connect() implementation in sockwrap.c
   * with a non-blocking connect() and a timeout
   *****************************************************/
  s = tcp_connect(argv[1], argv[2]);

  printf("\nconnected.\n===========================================================\n");

  /***********************************************************************
   * ignore the SIGPIPE and handle errors of broken pipes directly in the
   * code in order to prevent unexpected behaviours.
   ***********************************************************************/
  Signal(SIGPIPE, SIG_IGN);

  /* create the timeout */
  tval.tv_sec = 6;
  tval.tv_usec = 0;

  /*********************************************************************************
   * set the SO_RCVTIMEO option on the connected socket to not wait forever during
   * operations of read/recv, specifying a timeval structure; then handle it through
   * the EINWOULDBLOCK errno (like the API says) in the Readn_timeo() function
   * implemented in recvfile.c
   *********************************************************************************/
  Setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tval, sizeof(tval));

  int k;

  /* loop statement for every file requested by the terminal */
  for (k = 3; k < argc; k++)
  {

    /* reset the buffer */
    memset(buf, 0, MAXBUFLEN);

    /* create the "GET filename\r\n" string command */
    strcpy(buf, "GET ");
    strncat(buf, argv[k], strlen(argv[k]));
    strncat(buf, "\r\n", 2);

    /* send the "GET filename\r\n" command */
    Writen(s, buf, strlen(buf));

    printf("\nfile {%s} requested, waiting for response.\n", argv[k]);

    /********************************************************************** 
     * read the first 5 bytes; this is the maximum number of bytes possible 
     * to read according to the protocol because, if we read 6 bytes,
     * the 6th can be the first byte of the dimension variable.
     **********************************************************************/
    Readn(s, buf, 5);

    /* check if the server response is positive */
    if (strncmp(buf, "+OK\r\n", 5) == 0)
    {
      /* yeah, the command is good, go ahead */

      /* read the first 4 bytes of unsigned int to store te file dimension */
      Readn(s, &dimension, 4);
      /* read the second 4 bytes of unsigned int to store te file timestamp */
      Readn(s, &timestamp, 4);

      /* convert from network byte order to local byte order */
      dimension = ntohl(dimension);
      timestamp = ntohl(timestamp);

      /* receive the file byte by byte and store it; implemented in recvfile.c */
      len = Recvfile(s, argv[k], dimension, buf, timestamp);
    }
    /* check if the server response is negative */
    else if (strncmp(buf, "-ERR\r", 5) == 0)
    {
      /**************************************************************
       * due to the max of 5 bytes at the first readn, we have to make
       * sure that the 6byte string is correct (check the 6th byte) 
       **************************************************************/
      char c;
      Readn(s, &c, 1);
      if (c == '\n')
      {
        err_msg("(%s) server error - closing", prog_name);
        printf("\n===========================================================\n");
        Close(s);
        printf("closed.\n");
        exit(-1);
      }
    }
    else if ((strncmp(buf, "-ERR\r", 5) != 0) && (strncmp(buf, "+OK\r\n", 5) != 0))
    {
      /* message not valid from the server, close all */
      err_msg("(%s) server error - invalid response", prog_name);
      printf("\n===========================================================\n");
      Close(s);
      printf("closed.\n");
      exit(-1);
    }
  }

  /* reset buffer */
  memset(buf, 0, MAXBUFLEN);

  /* prepare and send the closing message */
  strncpy(buf, "QUIT\r\n", 6);
  Writen(s, buf, 6);

  printf("\n===========================================================\n");

  /* quit */
  Close(s);
  printf("closed.\n");

  exit(0);
}
