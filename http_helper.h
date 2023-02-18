//-----------------------------------------------
//	Jack Toggenburger
//
//	http_helper.h
//
//	A collection of functions to assist
//	in creating an http server
//
//-----------------------------------------------

#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

#include "asgn2_helper_funcs.h"

#define BUFF_SIZE 8192

// structures for the request information
typedef struct {

    // internal buffer for storing the request line
    char buf[BUFF_SIZE + 1];
    int bufsize;

    // request line
    char *method; 	// 8 characters
    char *uri; 		// 2 - 64 character
    char *version; 	// 8 characters (should be HTTP/1.1)
    int request_line_length;

    int content_length; // length of content
    int header_line_length;

    char *message_body;
    int message_body_length;
} Request;

// structure for response information
typedef struct {
    int status_code;

    // for use with valid get commands:
    int content_length;

} Response;

// Request helper functions ---------------------------------------------------

// read the request from socket, return 0 if successful and -1 if bad request
// will save remainder of the message body in message_body
int get_request(int socket, Request *req, Response *re);

// will screen a request struct, return a 0 if valid request and -1 if invalid.
int screen_request(Request *req, Response *re);

// Response helper functions ---------------------------------------------------

// will send the status line
// format "HTTP/1.1 status_code status_line\r\n"
int send_status(int socket, Response *re);

// send the content length 
// format "Content-Length: content_length\r\n\r\n"
int send_content_length(int socket, int length);

// sends a full response for any error status (e.g. no 200 or 201)
int send_error_response(int socket, Response *re);

// Method Helper Functions ----------------------------------------------------

// will return 0 write a response if successful, otherwise returns -1 and list error in re
int get(int socket, Request *req, Response *re);

// will return 0 write a response if successful, otherwise returns -1 and list error in re
int put(int socket, Request *req, Response *re);

// error helper functions -----------------------------------------------------

// returns the corrent message for a port error
int port_error(void);
