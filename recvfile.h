/*
 
 module: recvfile.h
 
 purpose: definitions of functions in recvfile.c
 
 reference: Luigi Ferrettino (s254300)
 
 */

#ifndef _RECVFILE_H

#define _RECVFILE_H

#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "sockwrap.h"

/***************************************************************************** 
* after some tests, we can archieve ~300MB/s with a buffer of 2048 bytes 
* in order to have best performances with minimum effort; that is good enough 
* considering the maximum speed of the most commons ethernet cables (~1Gbps).
******************************************************************************/
#define MAXBUFLEN 2048

ssize_t recvfile(int s, char *filename, uint32_t dim, char *buf, uint32_t timestamp);

ssize_t Recvfile(int s, char *filename, uint32_t dim, char *buf, uint32_t timestamp);

#endif
