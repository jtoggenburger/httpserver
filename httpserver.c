//-----------------------------------------------
//      Jack Toggenburger
//      
//      httpserver.c
//
//      A functioning subset of the HTTP/1.1
//      protocol implemented in C
//
//-----------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <stdbool.h>
#include "asgn2_helper_funcs.h"
#include "http_helper.h"

#define BUFF_SIZE    8192
#define HTTP_VERSION "HTTP/1.1"

int main(int argc, char **argv) {

    // initializing the server --------------------------------------------
    // socket stuff

    // if wrong number of command line args
    if (argc != 2) {
        return port_error();
    }

    int port = atoi(argv[1]);

    // if port is not a valid number
    if (port < 1 || port > 65535) {
        return port_error();
    }

    // attempt to open a socket
    Listener_Socket socket;

    if (listener_init(&socket, port) == -1) {
        return port_error();
    }

    // start listening for connections forever ----------------------------
    while (true) {

        // accept the connection
        int client_socket = listener_accept(&socket);

        // check client socket validity
        if (client_socket == -1) {
            err(1, NULL);
        }

        // read the intial request
        Request client_req;
        Response response;

        if (get_request(client_socket, &client_req, &response) == -1) {
            send_error_response(client_socket, &response);
            close(client_socket);
            continue;
        }

        // screen the request
        if (screen_request(&client_req, &response) == -1) {
            send_error_response(client_socket, &response);
            close(client_socket);
            continue;
        }

        // execute the request ------------------------------------------------
        if (strncmp(client_req.method, "GET", 4) == 0) {
            if (get(client_socket, &client_req, &response) == -1) {
                send_error_response(client_socket, &response);
                close(client_socket);
                continue;
            }
        }

        else if (strncmp(client_req.method, "PUT", 4) == 0) {
            if (put(client_socket, &client_req, &response) == -1) {
                send_error_response(client_socket, &response);
                close(client_socket);
                continue;
            }
        }

        else {
            response.status_code = 501;
            send_error_response(client_socket, &response);
            close(client_socket);
            continue;
        }

        // close the request --------------------------------------------------
        if (close(client_socket) == -1) {
            err(1, NULL);
        }
    }

    // should never ever hit this
    return 0;
}
