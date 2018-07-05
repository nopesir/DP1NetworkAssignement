/*
 
 module: serve.h
 
 purpose: definitions of functions in serve.c
 
 reference: Luigi Ferrettino (S254300)
 
 */

#ifndef _SERVE_H

#define _SERVE_H

#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include "errlib.h"
#include "sockwrap.h"

#define BUFFLEN 64

void serve(int connfd, char *host);

unsigned get_file_size(const char *file_name);

unsigned get_file_timestamp(const char *file_name);

ssize_t Readn_timeo(int fd, void *ptr, size_t nbytes, char *hostname);

#endif
