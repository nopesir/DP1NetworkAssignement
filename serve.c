/*

module: serve.c

purpose: library for serving functions

author: Luigi Ferrettino (S254300)

*/

#include "serve.h"

/* GLOBAL VARIABLES */
extern char *prog_name;

/****************************************
 * serve the connected socket according
 * to the protocol described in 
 * server1_main.c/server2_main.c
*****************************************/
void serve(int connfd, char *host)
{
    char *c;                       /* char pointer allocated dinamically with 2 elements (+1 end of string) */
    char buf[BUFFLEN];             /* buffer used for storing temporary bytes */
    char filename[30];             /* name of the file requested */
    uint32_t dimension, timestamp; /* dimension and last modified timestamp of the filename (if it exist) */
    FILE *stream_socket_r;         /* file stream to read and send throught socket */
    char *hostipv4;                /* additional pointer to host */
    int pid = (int)getpid();       /* store the PID of this process */

    /* translates IPv4-mapped IPv6 string addresses to IPv4 string */
    if ((hostipv4 = strstr(host, "::ffff:")) != NULL)
    {
        host[0] = '\0';
        host = hostipv4 + 7;
        memset(hostipv4, 0, strlen(hostipv4));
    }

    /*********************************************** 
     * during the connection we don't know how many 
     * files are requested by the client, so we need 
     * an infinite loop that will stop correctly
     ***********************************************/
    while (1)
    {
        /* initialise the buffer */
        memset(buf, 0, BUFFLEN);

        /************************************************
         * read the first 4 bytes, not even more because 
         * we could have, has the protocol says, the name 
         * of the file that has a variable lenght 
         ************************************************/
        if (Readn_timeo(connfd, buf, 4, host) < 0)
            break;

        /* check the buffer, if is "GET ", go on to store the filename */
        if (strncmp(buf, "GET ", 4) == 0)
        {
            /**********************************************************************************
            * since we don't know the lenght of the file name, we must read byte by byte from 
            * the socket until we get '\n' or the filename is too long. We use  the unbuffered 
            * version since the buffered could have some problems on the future read/recv.
            ***********************************************************************************/

            /* reset the buffer */
            memset(buf, 0, BUFFLEN);

            ssize_t filenamelenght = readline_unbuffered(connfd, buf, BUFFLEN);

            /* check for errors */
            if (filenamelenght < 0)
            {
                err_msg("%d\t%s - (%s) error - readline_unbuffered() failed.", pid, host, prog_name);
                strncpy(buf, "-ERR\r\n", 6);
                if (writen(connfd, buf, 6) != 6)
                    err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                break;
            }

            /* make sure that the last 2 bytes respect the prtocol */
            if ((buf[filenamelenght - 2] == '\r') && (buf[filenamelenght - 1] == '\n'))
            {

                /* remove the last 2 bytes to have only the file name */
                buf[filenamelenght - 2] = '\0';

                /* save the file name in the filename variable */
                strcpy(filename, buf);

                printf("%d\t%s - file {%s} requested.\n", pid, host, filename);
                fflush(stdout);

                memset(buf, 0, BUFFLEN);

                /***************************************************************************************************** 
                * due to security reasons there's necessity to deny accesses outside the working directory by checking 
                * the "../" string in the filename; since filenames in Unix can't contains those characters, this is a 
                * simple and safe mode to do it properly. Despite the "../", it's possible to access on subdirectories 
                * included in the workspace directory, whitch is a good thing.
                ******************************************************************************************************/
                if (strstr(filename, "../") != NULL)
                {
                    err_msg("%d\t%s - (%s) error - requested a file not in the working directory, closing..", pid, host, prog_name);
                    strncpy(buf, "-ERR\r\n", 6);
                    if (writen(connfd, buf, 6) != 6)
                        err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                    break;
                }

                /* now we need to know if the file exists and if it's readable with the access() function */
                if (access(filename, R_OK) != -1)
                {
                    /* the file exists, retrieve the dimension and the modified timestamp and convert it in network byte order */
                    if ((dimension = get_file_size(filename)) < 0)
                    {
                        err_msg("%d\t%s - (%s) error - stat dimension failed, closing..", pid, host, prog_name);
                        strncpy(buf, "-ERR\r\n", 6);
                        if (writen(connfd, buf, 6) != 6)
                            err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                        break;
                    }
                    if ((timestamp = get_file_timestamp(filename)) < 0)
                    {
                        err_msg("%d\t%s - (%s) error - stat timestamp failed, closing..", pid, host, prog_name);
                        strncpy(buf, "-ERR\r\n", 6);
                        if (writen(connfd, buf, 6) != 6)
                            err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                        break;
                    }

                    dimension = htonl(dimension);
                    timestamp = htonl(timestamp);

                    /* reset the buffer */
                    memset(buf, 0, BUFFLEN);

                    /* open the file in read mode */
                    if ((stream_socket_r = fopen(filename, "r")) == NULL)
                    {
                        err_ret("%d\t%s - (%s) error - fopen() failed", pid, host, prog_name);
                        strncpy(buf, "-ERR\r\n", 6);
                        if (writen(connfd, buf, 6) != 6)
                            err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                        break;
                    }

                    /* start preparing the response according to the protocol */
                    strncpy(buf, "+OK\r\n", 5);

                    /* write the "+OK\r\n" string followed by dimension and timestamp (the last two in the network byte order) */
                    if (writen(connfd, buf, 5) != 5)
                    {
                        err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                        break;
                    }
                    if (writen(connfd, &dimension, 4) != 4)
                    {
                        err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                        break;
                    }
                    if (writen(connfd, &timestamp, 4) != 4)
                    {
                        err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                        break;
                    }

                    /****************************************************************************************************************
                     * after the timestamp, we need to send the file. sendfile() copies data between one file descriptor and another. 
                     * Because this copying is done within the kernel, sendfile() is more efficient than the combination of read() 
                     * and write(), which would require transferring data to and from user space.
                     ****************************************************************************************************************/
                    uint32_t bytesent = sendfile(connfd, fileno(stream_socket_r), NULL, ntohl(dimension));

                    /* rewind the FILE pointer and then close it */
                    rewind(stream_socket_r);
                    fclose(stream_socket_r);

                    /* check the bytesent for error handling */
                    if (bytesent == ntohl(dimension))
                    {
                        printf("%d\t%s - file {%s} sent.\n", pid, host, filename);
                        fflush(stdout);
                    }
                    else if (bytesent < ntohl(dimension))
                    {
                        /* the client is unexpectedly disconnected, the file sent is incomplete */
                        err_msg("%d\t%s - (%s) error - sendfile failed, disconnected.", pid, host, prog_name);
                        fflush(stdout);
                        Close(connfd);
                        return;
                    }
                }
                else
                {
                    /* the file does't exists, send the "-ERR\r\n" command and break the while */
                    err_msg("%d\t%s - file {%s} not found, closing..", pid, host, filename);
                    strncpy(buf, "-ERR\r\n", 6);
                    if (writen(connfd, buf, 6) != 6)
                        err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                    break;
                }
            }
            else
            {
                /* the request isn't valid, send the "-ERR\r\n" command and break the while */
                err_msg("%d\t%s - (%s) error - illegal command, closing..", pid, host, prog_name);
                strncpy(buf, "-ERR\r\n", 6);
                if (writen(connfd, buf, 6) != 6)
                    err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                break;
            }
        }
        else if (strncmp(buf, "QUIT", 4) == 0)
        {
            /* the client could have finished requesting the files, go on and check */
            memset(buf, 0, BUFFLEN);

            if (Readn_timeo(connfd, buf, 2, host) < 0)
                break;

            if (strncmp(buf, "\r\n", 2) == 0)
            {
                /* the client has finished requesting files, break the while */
                printf("%d\t%s - client served\n", pid, host);
                fflush(stdout);
                break;
            }
            else
            {
                /* bad request */
                err_msg("%d\t%s - (%s) error - illegal command, closing..", pid, host, prog_name);
                strncpy(buf, "-ERR\r\n", 6);
                if (writen(connfd, buf, 6) != 6)
                    err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
                break;
            }
        }
        else
        {
            /* the request isn't valid, send the "-ERR\r\n" command and break the while */
            err_msg("%d\t%s - (%s) error - illegal command, closing..", pid, host, prog_name);
            strncpy(buf, "-ERR\r\n", 6);
            if (writen(connfd, buf, 6) != 6)
                err_ret("%d\t%s - (%s) error - writen failed", pid, host, prog_name);
            break;
        }
    }

    /* after every break, close the socket and return to the main (accepting) */
    Close(connfd);

    return;
}

/* use the stat() function to retrieve the dimension */
unsigned get_file_size(const char *file_name)
{
    struct stat sb;
    if (stat(file_name, &sb) != 0)
        return -1;

    return sb.st_size;
}

/* use the stat() function to retrieve the "last modified" timestamp */
unsigned get_file_timestamp(const char *file_name)
{
    struct stat sb;
    if (stat(file_name, &sb) != 0)
        return -1;

    return sb.st_mtime;
}

/* modified Readn to handle socket timeout on input during reading */
ssize_t Readn_timeo(int fd, void *ptr, size_t nbytes, char *hostname)
{
    ssize_t n;

    if ((n = read(fd, ptr, nbytes)) < 0)
    {
        int error = errno;
        if (error == EWOULDBLOCK)
        {
            err_msg("%d\t%s - (%s) error - Timeout waiting for data: closing connection..", getpid(), hostname, prog_name);
        }
        else
            err_ret("%d\t%s - (%s) error - readn() failed", getpid(), hostname, prog_name);
    }

    return n;
}
