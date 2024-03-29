#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "game.h"
#include "communicate.h"
#include <ctype.h>

conn_data_type conn_data;
conn_msg_type conn_msg;
char username[50];
char guess_char;
int client_sock;

void wait()
{
    sleep(2);
}

void send_server(int connfd, conn_msg_type conn_msg);

void print_title();

void print_sub_question(sub_question_type sub_question);

void print_waiting_room(waiting_room_type waiting_room);

void print_game_state(game_state_type game_state);

void handle_game_state(game_state_type *game_state);

char receive_answer();

void handle_sub_question(sub_question_type *sub_question);

void print_notification(char *notification);

int is_valid_guesschar(game_state_type game_state);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <Server IP> <Server Port>\n", argv[0]);
        return 0;
    }
    struct sockaddr_in server_addr;
    int bytes_sent, bytes_received, sin_size;

    // Step 1: Construct a TCP socket
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        // Call socket() to create a socket
        perror("\nError: ");
        return 0;
    }

    // Step 2: Define the address of the server
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    printf("Server IP: %s - Port: %d\n", argv[1], atoi(argv[2]));

    // Step 3: Request to connect server
    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0)
    {
        printf("\nError!Can not connect to sever! Client exit imediately! ");
        return 0;
    }

    // Step 4: Communicate with server
    // Step 4.1: Send username to server

    printf("Welcome to Wheel of Fortune\n");

back:
    printf("Please enter your username: ");
    scanf("%s%*c", username);
    player_type player = init_player(username, -1);

    printf("Connecting to server...\n");
    // Init communicate message
    copy_player_type(&conn_data.player, player);
    conn_msg = make_conn_msg(JOIN, conn_data);

    // Send message to server
    bytes_sent = send(client_sock, &conn_msg, sizeof(conn_msg), 0);
    if (bytes_sent <= 0)
    {
        perror("\nError: ");
        close(client_sock);
        return 0;
    }

    // Step 4.2: Infinitely receive message from server
    while (1)
    {
        printf("Waiting for server's response...\n");
        bytes_received = recv(client_sock, &conn_msg, sizeof(conn_msg), 0);
        printf("[DEBUG] Received %d bytes\n", bytes_received);
        wait();
        if (bytes_received <= 0)
        {
            printf("Lost server's connection\n");
            close(client_sock);
            return 0;
        }

        fflush(stdout);

        // Handle message from server
        switch (conn_msg.type)
        {

        case REFUSE:
            print_notification(conn_msg.data.notification);
            goto back;

        case WAITING_ROOM:
            print_waiting_room(conn_msg.data.waiting_room);
            break;

        case GAME_STATE:
            handle_game_state(&conn_msg.data.game_state);
            break;

        case SUB_QUESTION:
            handle_sub_question(&conn_msg.data.sub_question);
            break;

        case NOTIFICATION:
            print_notification(conn_msg.data.notification);
            break;

        case END_GAME:
            print_game_state(conn_msg.data.game_state);
            wait();
            return 0;
        }
    }

    // Step 5: Close socket
    close(client_sock);
    return 0;
}

void send_server(int connfd, conn_msg_type conn_msg)
{
    int bytes_sent;
    bytes_sent = send(connfd, &conn_msg, sizeof(conn_msg), 0);
    printf("[DEBUG] Send %d bytes\n", bytes_sent);
    wait();
    if (bytes_sent < 0)
    {
        printf("Error: Cannot send message to server!\n");
        exit(1);
    }
}

void print_title()
{
    printf("Wheel of Fortune\n");
    printf("====================================\n\n");
}

void print_sub_question(sub_question_type sub_question)
{
    // Clear screen
    printf("\033[2J");

    printf("Sub question:\n");
    printf("%s\n", sub_question.question);
    printf("A. %s\n", sub_question.answer[0]);
    printf("B. %s\n", sub_question.answer[1]);
    printf("C. %s\n", sub_question.answer[2]);
}

void print_waiting_room(waiting_room_type waiting_room)
{
    // Clear screen
    printf("\033[2J");

    // Print title
    print_title();

    // Print waiting room
    printf("Waiting room:\n");
    printf("Waiting room joined: %d\n", waiting_room.joined);
    for (int i = 0; i < waiting_room.joined; i++)
    {
        printf("Player %d: %s\n", i + 1, waiting_room.player[i].username);
    }
    printf("\n");
}

void print_game_state(game_state_type game_state)
{

    // Clear screen
    printf("\033[2J");

    // Print title
    print_title();

    // Print game state
    printf("====================================\n\n");
    printf("Game state:\n\n");

    // Print player
    for (int i = 0; i < PLAYER_PER_ROOM; i++)
    {
        printf("Player %d: %s\n", i + 1, game_state.player[i].username);
        printf("Point: %d\n\n", game_state.player[i].point);
    }

    // Print server's message
    printf("Server's message: \n%s\n", game_state.game_message);

    // Print sector
    printf("Current sector: %d\n", game_state.sector);

    printf("Question: %s\n", game_state.main_question);

    // Print crossword
    printf("Crossword:\n");
    printf("%s\n", game_state.crossword);

    // Print current player
    printf("[DEBUG] Current turn: %d\n", game_state.turn);
    printf("Current player: %s\n", game_state.player[game_state.turn].username);

    return;
}

void handle_game_state(game_state_type *game_state)
{
    int i;
    int bytes_sent;

    print_game_state(*game_state);

    // If it's my turn
    if (strcmp(game_state->player[game_state->turn].username, username) == 0)
    {
        printf("Please enter your guess: ");

        // Check guess_char is alphabet
        game_state->guess_char = '0';
        while (!isalpha(game_state->guess_char))
        {
            game_state->guess_char = getchar();
            fflush(stdin);
        }

        game_state->guess_char = toupper(game_state->guess_char);

        while (is_valid_guesschar(*game_state))
        {
            printf("Cannot choose exist character\nPlease choose again: ");
            game_state->guess_char = '0';
            while (!isalpha(game_state->guess_char))
            {
                game_state->guess_char = getchar();
                fflush(stdin);
            }

            game_state->guess_char = toupper(game_state->guess_char);
        }

        // Send guess char to server
        copy_game_state_type(&conn_msg.data.game_state, *game_state);
        conn_msg = make_conn_msg(GUESS_CHAR, conn_msg.data);
        send_server(client_sock, conn_msg);
    }
}

char receive_answer()
{
    char answer = '0';
    // Check answer is A, B, C
    while (answer != 'A' && answer != 'B' && answer != 'C')
    {
        printf("Please enter your answer (A, B, C): ");
        fflush(stdin);
        answer = toupper(getchar());
        fflush(stdin);
    }
    return answer;
}

void handle_sub_question(sub_question_type *sub_question)
{
    int i;
    int bytes_sent;
    print_sub_question(*sub_question);

    // If it's my turn
    if (strcmp(sub_question->username, username) == 0)
    {
        // Send answer to server
        sub_question->guess = receive_answer();
        copy_sub_question_type(&conn_msg.data.sub_question, *sub_question);
        conn_msg = make_conn_msg(SUB_QUESTION, conn_msg.data);
        send_server(client_sock, conn_msg);
    }
}

void print_notification(char *notification)
{
    // Clear screen
    printf("\033[2J");
    printf("====================================\n\n");
    printf("%s\n", notification);
}

int is_valid_guesschar(game_state_type game_state)
{
    int i;
    for (i = 0; i < strlen(game_state.crossword); i++)
    {
        if (game_state.crossword[i] == game_state.guess_char)
        {
            return 1;
        }
    }
    return 0;
}