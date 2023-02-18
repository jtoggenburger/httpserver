//-----------------------------------------------
//      Jack Toggenburger
//
//      http_helper.c
//
//      A collection of functions to assist
//      in creating an http server
//
//-----------------------------------------------

#include "http_helper.h"

#define LINE_REGEX "^([a-zA-Z]{0,8}) /([a-zA-Z0-9._]{1,63}) (HTTP/[0-9].[0-9])\r\n"

#define HEADER_REGEX "^([a-zA-Z0-9.-]{1,128}): ([ -~]{0,128})\r\n"

#define MESSAGE_REGEX "^([ -~]+)?"

#define BUFF_SIZE    8192
#define HTTP_VERSION "HTTP/1.1 "
#define MAX_MATCHES  50

int jwrite(int socket, char *str);

int get_line(Request *req, Response *re);

int get_header(Request *req, Response *re);

int get_message(Request *req, Response *re);

// read the request from socket, return 0 if successful and -1 if bad request
int get_request(int socket, Request *req, Response *re) {

    // ends with \r\n
    req->bufsize = read_until(socket, req->buf, BUFF_SIZE, "\r\n\r\n");

    if (req->bufsize > 0) {

        // add EOF
        req->buf[req->bufsize] = '\0';

        // get the initial line
        if (get_line(req, re) < 0) {
            return -1;
        }

        if (strncmp(req->method, "PUT", 4) == 0) {
            // get our content_length
            if (get_header(req, re) < 0) {
                return -1;
            }

            // get our message
            if (get_message(req, re) < 0) {
                return -1;
            }
        }

        return 0;
    }

    // otherwise this was a bad request
    re->status_code = 400;
    return -1;
}

// reads the request line and stores it in Request
int get_line(Request *req, Response *re) {
    int rc = 0;
    regex_t reg;
    regmatch_t matches[4];

    rc = regcomp(&reg, LINE_REGEX, REG_EXTENDED);
    assert(!rc);

    rc = regexec(&reg, req->buf, 4, matches, 0);

    if (rc == 0) {
        // we have a put match
        // set our req to the correct positions
        req->method = req->buf;
        req->uri = req->buf + matches[2].rm_so;
        req->version = req->buf + matches[3].rm_so;

        // add EOF
        req->method[matches[1].rm_eo] = '\0';
        req->uri[matches[2].rm_eo - matches[2].rm_so] = '\0';
        req->version[matches[3].rm_eo - matches[3].rm_so] = '\0';

        // calculate the length of this line
        req->request_line_length = matches[3].rm_eo + 2; // account for unmatched characters

        regfree(&reg);
        return 0;
    }

    regfree(&reg);
    re->status_code = 400;
    return -1;
}

int get_header(Request *req, Response *re) {
    int rc = 0;
    regex_t reg;
    regmatch_t matches[3];

    rc = regcomp(&reg, HEADER_REGEX, REG_EXTENDED);
    assert(!rc);

    char *header_buf = req->buf + req->request_line_length;
    char *start = header_buf;

    rc = regexec(&reg, header_buf, 3, matches, 0);

    if (rc == -1) {
        re->status_code = 400;
        regfree(&reg);
        return -1;
    }

    while (rc == 0) {

        // screen the fields to find our Content-Length
        char *key = header_buf;
        char *value = header_buf + matches[2].rm_so;

        // add EOF
        key[matches[1].rm_eo] = '\0';
        value[matches[2].rm_eo - matches[2].rm_so] = '\0';

        // check for content length
        if (strcmp(key, "Content-Length") == 0) {
            req->content_length = (int) atoi(value);
        }

        header_buf = header_buf + matches[2].rm_eo + 2;

        // increment our search region by the last search length
        rc = regexec(&reg, header_buf, 3, matches, 0);
    }

    req->header_line_length = (header_buf + 2) - start;

    regfree(&reg);
    return 0;
}

int get_message(Request *req, Response *re) {
    int rc = 0;
    regex_t reg;
    regmatch_t matches[2];

    char *message_buf = req->buf + req->request_line_length + req->header_line_length;

    rc = regcomp(&reg, MESSAGE_REGEX, REG_EXTENDED);
    assert(!rc);

    rc = regexec(&reg, message_buf, 2, matches, 0);

    if (rc == 0) {

        req->message_body = message_buf + matches[1].rm_so;
        req->message_body[matches[1].rm_eo - matches[1].rm_so] = '\0';
        req->message_body_length = matches[1].rm_eo - matches[1].rm_so;

        regfree(&reg);
        return 0;
    }

    re->status_code = 400;
    regfree(&reg);
    return -1;
}

int screen_request(Request *req, Response *re) {

    // check for version
    if (strncmp(req->version, "HTTP/1.1", 9) != 0) {
        re->status_code = 505;
        return -1;
    }

    // ensure a PUT command is valid
    if (strncmp(req->method, "PUT", 4) == 0) {

        // ensure there is a content length > 0
        if (req->content_length <= 0) {
            re->status_code = 400;
            return -1;
        }
    }

    return 0;
}

// Response Functions ---------------------------------------------------------

// 0 if success, -1 if failed
int send_status(int socket, Response *re) {
    // write out the HTTP version
    if (jwrite(socket, HTTP_VERSION) < 0) {
        return -1;
    }

    // write out status code / phrase

    int status;
    int status_code = re->status_code;

    switch (status_code) {
    case 200: status = jwrite(socket, "200 OK"); break;
    case 201: status = jwrite(socket, "201 Created"); break;
    case 400: status = jwrite(socket, "400 Bad Request"); break;
    case 403: status = jwrite(socket, "403 Forbidden"); break;
    case 404: status = jwrite(socket, "404 Not Found"); break;
    case 500: status = jwrite(socket, "500 Internal Server Error"); break;
    case 501: status = jwrite(socket, "501 Not Implemented"); break;
    case 505: status = jwrite(socket, "505 Version Not Supported"); break;
    default:
        fprintf(stderr, "ERROR: send_response: %d not a valid status code\n", re->status_code);
        return -1;
    }

    // check to ensure it wrote
    if (status < 0) {
        return -1;
    }

    // write the last \r\n
    if (jwrite(socket, "\r\n") < 0) {
        return -1;
    }

    return 0;
}

int send_content_length(int socket, int length) {

    if (jwrite(socket, "Content-Length: ") < 0) {
        return -1;
    }

    // now write the number
    int num = length;
    int num_digits = (int) (log10(num) + 2);
    char *num_string = malloc(num_digits);

    if (snprintf(num_string, num_digits, "%d", num) < 0) {
        fprintf(stderr, "ERROR: send_content_length: snprintf did not write correctly \n");
        return -1;
    }

    if (jwrite(socket, num_string) < 0) {
        return -1;
    }

    free(num_string);

    // write the end of the request
    if (jwrite(socket, "\r\n\r\n") < 0) {
        return -1;
    }

    return 0;
}

// returns -1 on failure
int send_error_response(int socket, Response *re) {

    // print the status
    if (send_status(socket, re) < 0) {
        return -1;
    }

    // print the content length and error message
    int status_code = re->status_code;
    char *message_body;

    switch (status_code) {
    case 400: message_body = "Bad Request\n"; break;
    case 403: message_body = "Forbidden\n"; break;
    case 404: message_body = "Not Found\n"; break;
    case 500: message_body = "Internal Server Error\n"; break;
    case 501: message_body = "Not Implemented\n"; break;
    case 505: message_body = "Version Not Supported\n"; break;
    default:
        fprintf(
            stderr, "ERROR: send_error_response: %d not a valid status code\n", re->status_code);
        return -1;
    }

    // send the message length
    if (send_content_length(socket, strlen(message_body)) < 0) {
        return -1;
    }

    // send the message body

    if (jwrite(socket, message_body) < 0) {
        return -1;
    }

    return 0;
}

// Method Functions -----------------------------------------------------------

// will return 0 success, -1 if there is an error message
// status code will be saved in the Response
// if success then will send the required file
int get(int socket, Request *req, Response *re) {

    // check to see if the file exists
    int file;
    file = open(req->uri, O_RDONLY);

    // go through the errors, set response accordingly, exit with failure
    if (file < 0) {
        switch (errno) {
        case EACCES:
        case EISDIR:
        case EOPNOTSUPP:
        case EPERM: re->status_code = 403; return -1;
        case ENFILE:
        case ENOMEM: re->status_code = 500; return -1;
        case EFAULT:
        default: re->status_code = 404; return -1;
        }
    }

    // make sure to outlaw any forbidden files
    if (strncmp(req->uri, "..", 3) == 0) {
        re->status_code = 403;
        return -1;
    }
    if (strncmp(req->uri, ".", 2) == 0) {
        re->status_code = 403;
        return -1;
    }

    // save the length of the file
    struct stat st;

    if (fstat(file, &st) < 0) {
        re->status_code = 500;
        fprintf(stderr, "ERROR: get: fstat() failed\n");
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        re->status_code = 403;
        return -1;
    }

    int length = st.st_size;

    // send status, content length

    re->status_code = 200;

    if (send_status(socket, re) < 0) {
        fprintf(stderr, "ERROR: get: send_status() failure\n");
        re->status_code = 500;
        return -1;
    }

    if (send_content_length(socket, length) < 0) {
        fprintf(stderr, "ERROR: get: send_content_length() failure\n");
        //re->status_code = 500;
        return -1;
    }

    // send the response
    char buf[BUFF_SIZE];
    int bytes_read = 0;

    while (bytes_read < length) {
        int bytes = read_until(file, buf, BUFF_SIZE, NULL);

        if (bytes < 0) {
            //re->status_code = 500;
            return -1;
        }

        if (write_all(socket, buf, bytes) < 0) {
            //re->status_code = 500;
            return -1;
        }

        bytes_read += bytes;
    }

    close(file);
    return 0;
}

int put(int socket, Request *req, Response *re) {

    // check to see if the file exists or not

    // if it exists, overwrite respond with code 200

    // otherwise create it with perms 0644 and return 201

    // if all fails, return -1 and set the error in response, on success return 0

    // does the file exist?
    if (access(req->uri, F_OK) == 0) {
        // file exists
        re->status_code = 200;
    } else {
        // file is created
        re->status_code = 201;
    }

    int file;
    file = open(req->uri, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (file < 0) {
        switch (errno) {
        case EACCES:
        case EISDIR:
        case EROFS: re->status_code = 403; return -1;
        case EDQUOT:
        case ENFILE:
        case ENOMEM:
        case ENOSPC: re->status_code = 500; return -1;
        case EINVAL: re->status_code = 400; return -1;
        case EFAULT:
        default: re->status_code = 404; return -1;
        }
    }

    // make sure to outlaw any forbidden files
    if (strncmp(req->uri, "..", 3) == 0) {
        re->status_code = 403;
        return -1;
    }
    if (strncmp(req->uri, ".", 2) == 0) {
        re->status_code = 403;
        return -1;
    }

    struct stat st;

    if (fstat(file, &st) < 0) {
        re->status_code = 500;
        fprintf(stderr, "ERROR: get: fstat() failed\n");
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        re->status_code = 403;
        return -1;
    }

    // now write into the file from the socket!

    // first write any leftover items
    int bytes_to_write = req->content_length;
    int bytes = 0;
    char buf[BUFF_SIZE];

    if (req->message_body_length > 0) {
        bytes = write_all(file, req->message_body, req->message_body_length);
        if (bytes < 0) {
            re->status_code = 500;
            return -1;
        }

        bytes_to_write -= bytes;
    }

    // continue reading from socket until cannot anymore or bytes_to_write is 0
    // we need to make sure to only read content_length bytes!!!!!

    while (bytes_to_write > 0) {
        bytes = read_until(socket, buf, fmin(BUFF_SIZE, bytes_to_write), NULL);

        //printf("%s", buf);

        if (bytes < 0) {
            fprintf(stderr, "PUT: read_until: bytes < 0: %d\n", errno);
            re->status_code = 500;
            return -1;
        }

        if (write_all(file, buf, bytes) < 0) {
            fprintf(stderr, "PUT: write_alll: < 0\n");
            re->status_code = 500;
            return -1;
        }

        bytes_to_write -= bytes;
    }

    // now write our response

    send_status(socket, re);

    char *message_bod;

    if (re->status_code == 201) {
        message_bod = "Created\n";
    } else {
        message_bod = "OK\n";
    }

    send_content_length(socket, strlen(message_bod));
    jwrite(socket, message_bod);

    close(file);

    return 0;
}

// custom print style function which takes a null terminated string and sends it to the socket
// return -1 on failure
int jwrite(int socket, char *str) {
    return write_all(socket, str, strlen(str));
}

// prints an error message to stderr and returns 1
int port_error(void) {
    fprintf(stderr, "Invalid Port\n");
    return 1;
}
