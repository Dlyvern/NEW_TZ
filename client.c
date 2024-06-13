#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#define PORT 8080 //Server's port
#define N 6 //Matrix
#define TIMER_TIMEOUT 5 //Timer timeout to send matrix to the server

volatile int client_running = 1; //Flag to close client's side correctly

void generate_random_matrix(double matrix[N][N]); //Just generates matrix N x N with random numbers
void print_matrix(double matrix[N][N]);
int connect_to_server(); //Creates a new socket to communicate with the server

//Sends matrix to the server every TIMER_TIMEOUT seconds
void* start_timer_to_send_data_to_the_server(void* args)
{
    int socket = *((int*)args);

    time_t start_time = time(NULL);

    while(client_running)
    {
        int elapsed_time = (int)(time(NULL) - start_time);

        if(elapsed_time < 0)
            continue;

        else if(elapsed_time >= TIMER_TIMEOUT)
        {
            double matrix[N][N];

            generate_random_matrix(matrix);
            print_matrix(matrix);

            int send_result = send(socket, matrix, sizeof(matrix), MSG_NOSIGNAL);

            //Well... Actually this doesn't seem like a good idea...
            if(send_result < 0)
            {
                switch (errno)
                {
                    case EPIPE:
                    {
                        socket = connect_to_server();
                    }
                }
            }

            start_time = time(NULL);
        }

        sleep(1);
    }

    return NULL;
}

//Sets client_running flag to false to close client's side
void signal_handler(int sigNum)
{
    client_running = 0;
}

void generate_random_matrix(double matrix[N][N])
{
    //A lot of warnings in CLion...
    srand(time(NULL));

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            matrix[i][j] = (double)rand() / RAND_MAX * 10.0;
}

void print_matrix(double matrix[N][N])
{
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
            printf("%.2f\t", matrix[i][j]);

        printf("\n");
    }
}

int connect_to_server()
{
    int client_socket;

    struct sockaddr_in server_address;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if(client_socket == -1)
    {
        printf("Socket is failed\n");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); //Only local host
    server_address.sin_port = htons(PORT);

    int is_connected = -1;

    printf("Trying to connect to the server...\n");

    //Trying to connect every second
    while(is_connected == -1 && client_running)
    {
        is_connected = connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address));

        sleep(1);
    }

    return client_socket;
}

int main()
{
    int client_socket = connect_to_server();

    if(client_socket == -1)
    {
        printf("Failed to connect to the server");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server\n");

    //To handle ctr + c signal
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    pthread_t timer_thread;

    pthread_create(&timer_thread, NULL, start_timer_to_send_data_to_the_server, &client_socket);
    pthread_detach(timer_thread);

    while(client_running)
    {
        getchar();

        double matrix[N][N];

        generate_random_matrix(matrix);
        print_matrix(matrix);

        int send_result = send(client_socket, matrix, sizeof(matrix), MSG_NOSIGNAL);

        //Well... Actually this doesn't seem like a good idea...
        if(send_result < 0)
        {
            switch (errno)
            {
                case EPIPE:
                {
                    client_socket = connect_to_server();
                }
            }
        }
    }

    pthread_join(timer_thread, NULL);

    printf("\nClient is exiting...\n");

    close(client_socket);

    return 0;
}
