#include "zazu.h"

/**
 * Exits the program with a error.
 * @param int - Error code.
 * @param playerLetter - The player letter of the player which disconnected
 * or sent an invalid message.
 */
void exit_with_error(int error, char playerLetter) {
    switch(error) {
        case INVALID_ARG_NUM:
            fprintf(stderr, "Usage: zazu keyfile port game pname\n");
            break;
        case INVALID_KEYFILE:
            fprintf(stderr, "Bad key file\n");
            break;
        case BAD_NAME:
            fprintf(stderr, "Bad name\n");
            break;
        case CONNECT_ERR_PLAYER:
            fprintf(stderr, "Failed to connect\n");
            break;
        case BAD_AUTH:
            fprintf(stderr, "Bad auth\n");
            break;
        case BAD_RID:
            fprintf(stderr, "Bad reconnect id\n");
            break;
        case COMM_ERR:
            fprintf(stderr, "Communication Error\n");
            break;
        case PLAYER_DISCONNECTED:
            fprintf(stderr, "Player %c disconnected\n", playerLetter);
            break;
        case INVALID_MESSAGE:
            fprintf(stderr, "Player %c sent invalid message\n", playerLetter);
            break;
    }
    exit(error);
}

/**
 * Check if a character is an newline character or a comma character.
 * @param character - The character to check.
 * @return - returns 1 if the char it is an newline or comma.
 */
int is_newline_or_comma(char character) {
    return character == '\n' || character == ',';
}

/**
 * Checks initial arguments for zazu.
 * @param argc - Argument count.
 * @param argv - Argument vector.
 */
void check_args(int argc, char **argv) {
    if (argc != EXPECTED_ARGC) {
        exit_with_error(INVALID_ARG_NUM, ' ');
    }
    if (!is_string_digit(argv[PORT])) {
        exit_with_error(CONNECT_ERR_PLAYER, ' ');
    }
    if (atoi(argv[PORT]) < 0 || atoi(argv[PORT]) > 65535) {
        exit_with_error(CONNECT_ERR_PLAYER, ' ');
    }
    if (strcmp(argv[GAME_NAME], "reconnect") == 0) {
        if (!verify_rid(argv[RECONNECT_ID])) {
            exit_with_error(BAD_RID, ' ');
        }
    } else {
        for (int i = 0; i < strlen(argv[GAME_NAME]); i++) {
            if (is_newline_or_comma(argv[GAME_NAME][i])) {
                exit_with_error(BAD_NAME, ' ');
            }
        }
        for (int i = 0; i < strlen(argv[PLAYER_NAME]); i++) {
            if (is_newline_or_comma(argv[PLAYER_NAME][i])) {
                exit_with_error(BAD_NAME, ' ');
            }
        }
    }

}

/**
 * Generates a socket from a provided port.
 * @param output - The output socket.
 * @param port - Port value to generate the socket from.
 */
enum Error get_socket(int *output, char *port) {
    struct addrinfo hints, *res, *res0;
    int sock;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int error = getaddrinfo(LOCALHOST, port, &hints, &res0);
    if (error) {
        freeaddrinfo(res0);
        return CONNECT_ERR_PLAYER;
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
            close(sock);
            sock = -1;
            continue;
        }

        break;  /* okay we got one */
    }
    if (sock == -1) {
        freeaddrinfo(res0);
        return CONNECT_ERR_PLAYER;
    }
    *output = sock;
    freeaddrinfo(res0);
    return NOTHING_WRONG;
}

/**
 * Listens on the server for any bytes.
 * @param out - The file descriptor to listen on.
 * @param output - The stream of data received.
 */
void listen_server(FILE *out, char **output) {
    read_line(out, output, 0);
    if (*output == NULL) {
        return;
    }
    if (strcmp(*output, "eog") == 0) {
        exit(NOTHING_WRONG);
    }
}

/**
 * Verifies the reconnect id.
 * @param line - The line to verify.
 */
int verify_rid(char *line) {
    if (!match_seperators(line, 0, 2)) {
        return 0;
    }
    char *rid = malloc(strlen(line) + 1);
    strcpy(rid, line);
    rid[strlen(rid)] = '\0';
    char **commaSplit = split(rid, ",");
    for (int i = 0; i < 3; i++) {
        if (strlen(commaSplit[i]) == 0) {
            free(rid);
            free(commaSplit);
            return 0;
        }
    }
    if (!is_string_digit(commaSplit[GAME_COUNTER])
            || !is_string_digit(commaSplit[PID])) {
        free(rid);
        free(commaSplit);
        return 0;
    }
    free(rid);
    free(commaSplit);
    return 1;
}

/**
 * Parses the playinfo message from the hub.
 * @param server - The server instance.
 * @param line - The line to parse.
 */
void parse_playinfo_message(Server *server, char *buffer) {
    char **splitString = split(buffer, "o");
    char **playInfo = split(splitString[RIGHT], "/");
    server->game.selfId = playInfo[LEFT][0] - 'A';
    server->game.playerCount = atoi(playInfo[RIGHT]);
    setup_players(server, server->game.playerCount);
    display_turn_info(&server->game);
    free(playInfo);
    free(splitString);
    free(buffer);
}

/**
 * Parses the tokens message from the hub.
 * @param server - The server instance.
 * @param line - The line to parse.
 */
int handle_tokens_message(Server *server, char *buffer) {
    int output;
    parse_tokens_message(&output, buffer);
    if (output == -1) {
        return 0;
    }
    for (int i = 0; i < (TOKEN_MAX - 1); i++) {
        server->game.tokenCount[i] = output;
    }
    display_turn_info(&server->game);
    free(buffer);
    return 1;
}

/**
 * Gets the initial game information to setup a game state.
 * @param server - The server instance.
 */
enum Error get_game_info(Server *server) {
    char *buffer;
    listen_server(server->out, &buffer);
    if (buffer == NULL) {
        free(buffer);
        return COMM_ERR;
    }
    if (strstr(buffer, "rid") != NULL) {
        if (!verify_rid(buffer)) {
            free(buffer);
            return COMM_ERR;
        }
        char **splitString = split(buffer, "d");
        printf("%s\n", splitString[RIGHT]);
        free(splitString);
        free(buffer);
    } else {
        free(buffer);
        return COMM_ERR;
    }
    listen_server(server->out, &buffer);
    if (strstr(buffer, "playinfo") != NULL) {
        parse_playinfo_message(server, buffer);
    } else {
        free(buffer);
        return COMM_ERR;
    }
    listen_server(server->out, &buffer);
    if (strstr(buffer, "tokens") != NULL) {
        if (!handle_tokens_message(server, buffer)) {
            free(buffer);
            return INVALID_MESSAGE;
        };
    } else {
        free(buffer);
        return COMM_ERR;
    }
    return NOTHING_WRONG;
}

/**
 * Sets up server sockets and fd's and sends the required messages
 * to setup a game.
 * @param server - The server instance.
 * @param gamename - The game to connect to.
 * @param playername - The name of the current player.
 * @return Error based on the server's response.
 */
enum Error connect_server(Server *server, char *gamename, char *playername) {
    enum Error err = get_socket(&server->socket, server->port);
    if (err) {
        return err;
    }
    server->in = fdopen(server->socket, "w");
    server->out = fdopen(server->socket, "r");
    send_message(server->in, "play%s\n", server->key);
    char *buffer;
    read_line(server->out, &buffer, 0);
    if (strcmp(buffer, "yes") != 0) {
        free(buffer);
        return BAD_AUTH;
    }
    send_message(server->in, "%s\n", gamename);
    send_message(server->in, "%s\n", playername);
    server->gameName = gamename;
    free(buffer);
    return NOTHING_WRONG;
}

/**
 * Parses the player message from the hub.
 * @param server - The server instance.
 * @param line - The line to parse.
 */
void parse_player_message(Server *server, char *line) {

}

/**
 * Attempt to reconnect to a game.
 * @param server - The server instance.
 * @param rid - The reconnect id.
 */
enum Error reconnect_server(Server *server, char *rid) {
    enum Error err = get_socket(&server->socket, server->port);
    if (err) {
        return err;
    }
    server->in = fdopen(server->socket, "w");
    server->out = fdopen(server->socket, "r");
    send_message(server->in, "reconnect%s\n", server->key);
    char *buffer;
    read_line(server->out, &buffer, 0);
    if (strcmp(buffer, "yes") != 0) {
        free(buffer);
        return BAD_AUTH;
    }
    send_message(server->in, "rid%s\n", rid);
    read_line(server->out, &buffer, 0);
    if (strcmp(buffer, "player") != 0) {
        free(buffer);
        return COMM_ERR;
    }
    while (1) {
        read_line(server->out, &buffer, 0);
        if (strstr(buffer, "newcard") != NULL) {
            handle_new_card_message(&server->game, buffer);
            free(buffer);
        } else {
            free(buffer);
            break;
        }
    }
    // parse_player_message(server, buffer);
    free(buffer);
    return NOTHING_WRONG;
}

/**
 * Frees memory allocated to the server.
 */
void free_server(Server server) {
    free(server.game.players);
    free(server.key);
    fclose(server.in);
    fclose(server.out);
}

/**
 * Prompts the user for arguments to the purchase message.
 * @param server - The server instance.
 * @param state - The current game state.
 */
void prompt_purchase(Server *server, struct GameState *state) {
    int validCardNumber = 0;
    struct PurchaseMessage message;
    while(!validCardNumber) {
        printf("Card> ");
        char *number;
        read_line(stdin, &number, 0);
        if (is_string_digit(number) && atoi(number) >= 0 &&
                atoi(number) <= 7) {
            message.cardNumber = atoi(number);
            validCardNumber = 1;
        }
        free(number);
    }
    for (int i = 0; i < TOKEN_MAX; i++) {
        if (state->players[state->selfId].tokens[i] == 0) {
            message.costSpent[i] = 0;
            continue;
        }
        int validToken = 0;
        while(!validToken) {
            printf("Token-%c> ", print_token(i));
            char *tokenTaken;
            read_line(stdin, &tokenTaken, 0);
            if (is_string_digit(tokenTaken) && atoi(tokenTaken) >= 0 &&
                    atoi(tokenTaken) <=
                    state->players[state->selfId].tokens[i]) {
                message.costSpent[i] = atoi(tokenTaken);
                validToken = 1;
            }
            free(tokenTaken);
        }
    }
    char *purchaseMessage = print_purchase_message(message);
    send_message(server->in, purchaseMessage);
    free(purchaseMessage);
}

/**
 * Prompts the user for arguments to the take message.
 * @param server - The server instance.
 * @param state - The current game state.
 */
void prompt_take(Server *server, struct GameState *state) {
    struct TakeMessage message;
    for (int i = 0; i < TOKEN_MAX - 1; i++) {
        int validAction = 0;
        while(!validAction) {
            printf("Token-%c> ", print_token(i));
            char *tokenTaken;
            read_line(stdin, &tokenTaken, 0);
            if (strcmp(tokenTaken, "") != 0 && is_string_digit(tokenTaken) &&
                    atoi(tokenTaken) >= 0 &&
                    atoi(tokenTaken) <= state->tokenCount[i]) {
                message.tokens[i] = atoi(tokenTaken);
                validAction = 1;
            }
            free(tokenTaken);
        }
    }
    char *takeMessage = print_take_message(message);
    send_message(server->in, takeMessage);
    free(takeMessage);
}

/**
 * Prompt the user to make a move.
 * @param server - The server instance.
 * @param state - The current game state.
 */
void make_move(Server *server, struct GameState *state) {
    int validInput = 0;
    while(!validInput) {
        printf("Action> ");
        char *buffer;
        read_line(stdin, &buffer, 0);
        if (strcmp(buffer, "purchase") == 0) {
            prompt_purchase(server, state);
            validInput = 1;
        } else if (strcmp(buffer, "take") == 0) {
            prompt_take(server, state);
            validInput = 1;
        } else if (strcmp(buffer, "wild") == 0) {
            send_message(server->in, "wild\n");
            validInput = 1;
        }
        free(buffer);
    }
}

/**
 * Handles messages received from the server.
 * @param server - The server instance.
 * @param type - The type of message received.
 * @param line - The message received.
 * @return error depending on whether if the message was valid.
 */
enum Error handle_messages(Server *server, enum MessageFromHub type,
        char *line) {
    enum ErrorCode err = 0;
    int id;
    switch (type) {
        case END_OF_GAME:
            display_eog_info(&server->game);
            exit_with_error(err, ' ');
        case DO_WHAT:
            printf("Received dowhat\n");
            make_move(server, &server->game);
            break;
        case PURCHASED:
            err = handle_purchased_message(&server->game, line);
            break;
        case TOOK:
            err = handle_took_message(&server->game, line);
            break;
        case TOOK_WILD:
            err = handle_took_wild_message(&server->game, line);
            break;
        case NEW_CARD:
            err = handle_new_card_message(&server->game, line);
            break;
        case DISCO:
            err = parse_disco_message(&id, line);
            if (err) {
                err = COMM_ERR;
            } else {
                free(line);
                exit_with_error(PLAYER_DISCONNECTED, id + 'A');
            }
        case INVALID:
            err = parse_invalid_message(&id, line);
            if (err) {
                err = COMM_ERR;
            } else {
                free(line);
                exit_with_error(INVALID_MESSAGE, id + 'A');
            }
        default:
            err = COMM_ERR;
    }
    return err;
}

/* Play the game, starting at the first round. Will return at the end of the
 * game, either with 0 or with the relevant exit code.
 * @param server - The server instance.
 */
enum Error play_game(Server *server) {
    enum ErrorCode err = 0;
    while (1) {
        char *line;
        int readBytes = read_line(server->out, &line, 0);
        // printf("recieved from server: %s\n", line);
        if (readBytes <= 0) {
            free(line);
            return COMM_ERR;
        }
        enum MessageFromHub type = classify_from_hub(line);
        err = handle_messages(server, type, line);
        free(line);
        if (err) {
            return err;
        } else if (type != DO_WHAT) {
            display_turn_info(&server->game);
        }
    }
}

/**
 * Sets up initial players.
 * @param server - The server instance.
 * @param amount - The amount of players.
 */
void setup_players(Server *server, int amount) {
    server->game.players = malloc(sizeof(struct GamePlayer) * amount);
    for (int i = 0; i < amount; i++) {
        initialize_player(&server->game.players[i], i);
    }
}

/**
 * Main
 */
int main(int argc, char **argv) {
    check_args(argc, argv);
    Server server;
    server.port = argv[PORT];
    server.game.boardSize = 0;
    enum Error err;
    err = load_keyfile(&server.key, argv[KEYFILE]);
    if (err) {
        exit_with_error(err, ' ');
    }
    if (strcmp(argv[GAME_NAME], "reconnect") == 0) {
        err = reconnect_server(&server, argv[RECONNECT_ID]);
        if (err) {
            exit_with_error(err, ' ');
        }
    } else {
        err = connect_server(&server, argv[GAME_NAME], argv[PLAYER_NAME]);
        if (err) {
            exit_with_error(err, ' ');
        }
        err = get_game_info(&server);
        if (err) {
            exit_with_error(err, ' ');
        }
    }
    play_game(&server);
    free_server(server);
}