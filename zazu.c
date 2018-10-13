#include "shared.h"

enum Error {
    INVALID_ARG_NUM = 1,
    INVALID_KEYFILE = 2,
    CONNECT_ERR = 5,
    FAILED_LISTEN = 6,
    BAD_RID = 7,
    COMM_ERR = 8,
    PLAYER_DISCONNECTED = 9,
    INVALID_MESSAGE = 10
};

enum Argument {
    KEYFILE = 1,
    PORT = 2,
    GAME_NAME = 3,
    PLAYER_NAME = 4
};

typedef struct {
    int socket;
    char *port;
    char *name;
    char *key;
} Server;

void exit_with_error(int error, char *playerName) {
    switch(error) {
        case INVALID_ARG_NUM:
            fprintf(stderr, "Usage: zazu keyfile port game pname\n");
            break;
        case INVALID_KEYFILE:
            fprintf(stderr, "Bad keyfile\n");
            break;
        case CONNECT_ERR:
            fprintf(stderr, "Bad timeout\n");
            break;
        case FAILED_LISTEN:
            fprintf(stderr, "Failed listen\n");
            break;
        case BAD_RID:
            fprintf(stderr, "Bad reconnect id\n");
            break;
        case COMM_ERR:
            fprintf(stderr, "Communication Error\n");
            break;
        case PLAYER_DISCONNECTED:
            fprintf(stderr, "Player %s disconnected\n", playerName);
            break;
        case INVALID_MESSAGE:
            fprintf(stderr, "Player %s sent invalid message\n", playerName);
            break;
    }
    exit(error);
}

void check_args(int argc, char **argv) {
    
}

void load_statfile(char *path) {
    
}

void get_socket(Server *server) {
    struct addrinfo hints, *res, *res0;
    int sock;
    const char *cause = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int error = getaddrinfo(LOCALHOST, server->port, &hints, &res0);
    if (error) {
        exit_with_error(CONNECT_ERR, server->name);
    }
    sock = -1;
    for (res = res0; res != NULL; res = res->ai_next) {
        sock = socket(res->ai_family, res->ai_socktype,
           res->ai_protocol);
        if (sock == -1) {
            sock = -1;
            continue;
        }

        if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
            sock = -1;
            close(sock);
            continue;
        }

        break;  /* okay we got one */
    }
    if (sock == -1) {
        exit_with_error(CONNECT_ERR, server->name);
    }
    printf("cause: %s, socket: %i\n", cause, sock);
    server->socket = sock;
    freeaddrinfo(res0);
}

void append_char(char **str, char c) {
    char *newStr = malloc(strlen(*str) + 2);
    strcpy(newStr, *str);
    newStr[strlen(*str)] = c;
    newStr[strlen(*str) + 1] = '\0';
    printf("newstr: %s\n", newStr);
    *str = newStr;
}

void connect_server(Server *server, char *gamename, char *playername) {
    get_socket(server);
    printf("client socket: %i\n", server->socket);
    Connection connection;
    connection.in = fdopen(server->socket, "w");
    connection.out = fdopen(server->socket, "r");
    send_message(&connection, "12345\n"); 
    char *buffer;
    read_line(connection.out, &buffer, 0);
    printf("recieved from server: %s\n", buffer);
    if (strcmp(buffer, "yes") != 0) {
        return;
    }
    // append_char(&gamename, '\n');
    // append_char(&playername. '\n');
    send_message(&connection, "%s\n", gamename); 
    send_message(&connection, "%s\n", playername); 
}

void setup_server(Server *server, char *port, char *keyfile) {
    server->port = port;
    server->key = "12345";
}

int main(int argc, char **argv) {
    check_args(argc, argv);
    load_keyfile(argv[1]);
    Server server;
    setup_server(&server, argv[PORT], argv[KEYFILE]);
    connect_server(&server, argv[GAME_NAME], argv[PLAYER_NAME]);
}