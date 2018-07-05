/*

module: recvfile.c

purpose: library of file incoming functions

author: Luigi Ferrettino (S254300)

*/

#include "errlib.h"
#include "recvfile.h"

extern char *prog_name;

/************************************************************* 
 * implemented by luigiferrettino from lab2.3 in 2018;
 * used to take care of file in-going trasmission and storing
**************************************************************/
ssize_t recvfile(int s, char *filename, uint32_t dim, char *buf, uint32_t timestamp)
{
  int len;
  struct timeval t1, t2;
  uint32_t remain_data = dim;
  FILE *stream_socket_w;
  char *temp = filename;

  /* if the filename is a path, delete all the path and replace it with only the filename */
  if (strstr(filename, "/") != NULL)
  {
    /* retrieve the string from the last occurrence of '/', +1 to skip the '\' itself */
    filename = (strrchr(temp, '/')) + 1;
  }

  /* at this point, we can remove the pointing variable (no free() required because temp points to filename and we don't want to delete it) */
  temp = NULL;

  /* open the file in write mode */
  stream_socket_w = fopen(filename, "w");
  if (stream_socket_w == NULL)
    err_sys("(%s) error - fopen() failed", prog_name);

  /* get time and store it in the struct timeval */
  gettimeofday(&t1, NULL);

  /* recv while loop from socket and fwrite on file with time manipulation funcions to retrieve te network instant speed */
  while ((len = Read(s, buf, MAXBUFLEN)) > 0)
  {
    fwrite(buf, sizeof(char), len, stream_socket_w);

    /* decrease the remain data */
    remain_data -= len;

    /* get time a subtract with t1 to know the time elapsed */
    gettimeofday(&t2, NULL);
    double elapsed_time = (t2.tv_sec - t1.tv_sec) + 0.0001;

    /* print on the same line an estimation of percentage and speed */
    printf("\r receiving.. %lu%%  %.1fMB/s            ", (unsigned long)(dim - remain_data) * 100 / dim, ((dim - remain_data) / elapsed_time) / 1000000);
    fflush(stdout);

    if (remain_data <= 0)
    {
      printf("\n");
      fflush(stdout);
      printf("\n");
      fflush(stdout);

      fclose(stream_socket_w);

      printf("{%s} received\n|- bytes: %lu\n|- timestamp: %lu\n", filename, (unsigned long)dim, (unsigned long)timestamp);
      fflush(stdout);

      break;
    }
  }

  /* return the effective stored data
  *  NOTE: to be checked in order to avoid errors
  */
  return dim - remain_data;
}

/*********************************************************************************** 
 * uppercase version of recvfile() with error handling and mass storage preservation
 * in order to avoid (after a disconnection) junk/corrupted files
 ***********************************************************************************/
ssize_t Recvfile(int s, char *filename, uint32_t dim, char *buf, uint32_t timestamp)
{

  ssize_t received;

  /* the server is unexpectedly disconnected, the file sent is incomplete, try to delete it */
  if ((received = recvfile(s, filename, dim, buf, timestamp)) < dim)
  {
    int ret = remove(filename);
    if (ret == 0)
      err_quit("\n(%s) error - recvfile() failed, corrupted file deleted.", prog_name);
    else
      err_quit("\n(%s) error - recvfile() failed, corrupted file not deleted.", prog_name);
  }

  return received;
}
