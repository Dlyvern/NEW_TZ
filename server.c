#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>

#define PORT 8080
#define MAX_NUMBER_OF_CLIENTS 50
#define N 6
#define DETERMINANT_HISTORY_SIZE 5

volatile int server_running = 1;

void signal_handler(int sigNum)
{
    server_running = 0;
}

typedef struct
{
    double oldDeterminants[DETERMINANT_HISTORY_SIZE];//Последние определители полученных матриц
    int amountOfMatrix; //Общее кол-во пришедших матриц от клиента
    int currentIndexOfDeterminant; //Текущий индекс определителя
} client_data_packet;

typedef struct
{
    int socket;
    int id;
    client_data_packet data;
    pthread_t threadId;
} client_data_struct;

void* handle_client(void* clientDataStruct);


double calculate_average(const double arr[DETERMINANT_HISTORY_SIZE])
{
    double sum = 0;

    for(int i = 0; i < DETERMINANT_HISTORY_SIZE; ++i)
        sum += arr[i];

    return sum / DETERMINANT_HISTORY_SIZE;
}

double determinant(double matrix[N][N])
{
    double det = 1.0;

    for(int i = 0; i < N; ++i)
    {
        double mx = fabs(matrix[i][i]);

        int idx = i;

        for(int j = i+1; j < N; ++j)
        {
            if (mx < fabs(matrix[i][j]))
                mx = fabs(matrix[i][idx = j]);
        }

        if (idx != i)
        {
            for(int j = i; j < N; ++j)
            {
                double t = matrix[j][i];
                matrix[j][i] = matrix[j][idx];
                matrix[j][idx] = t;
            }

            det = -det;
        }

        for(int k = i+1; k < N; ++k)
        {
            double t = matrix[k][i]/matrix[i][i];

            for(int j = i; j < N; ++j)
                matrix[k][j] -= matrix[i][j]*t;
        }
    }

    for(int i = 0; i < N; ++i)
        det *= matrix[i][i];

    return det;
}

void accept_clients(int serverSocket)
{
    client_data_struct clients[MAX_NUMBER_OF_CLIENTS];

    socklen_t client_length;

    struct sockaddr_in client;

    client_length = sizeof(client);

    int clients_count = 0;

    while(server_running)
    {
        if(clients_count == MAX_NUMBER_OF_CLIENTS)
        {
            sleep(1);
            continue;
        }

        int connection = accept(serverSocket, (struct sockaddr*)&client, &client_length);

        if(connection == -1)
            continue;

        pthread_t handle_client_thread;

        client_data_struct client_data;

        client_data.socket = connection;
        client_data.id = clients_count;
        client_data.data.amountOfMatrix = 0;
        client_data.data.currentIndexOfDeterminant = 0;

        int result = pthread_create(&handle_client_thread, NULL, handle_client, &client_data);

        if(result != 0)
        {
            printf("\nCould not create a thread to handle client");
            close(connection);
        }
        else
        {
            client_data.threadId = handle_client_thread;
            clients_count += 1;
            pthread_detach(handle_client_thread);
        }
    }

//Segmentation fault for some reason...
//    for(int i = 0; i < clients_count; ++i)
//        pthread_join(clients[i].threadId, NULL);

    printf("\nServer is exiting...\n");

    close(serverSocket);
}

void* handle_client(void* clientDataStruct)
{
    client_data_struct client_data = *((client_data_struct*)clientDataStruct);

    int client_socket = client_data.socket;

    double matrix[N][N];

    int bytes_received = 1;

    while(bytes_received != 0)
    {
        bytes_received = recv(client_socket, matrix, sizeof(matrix), 0);

        if((bytes_received == -1) || (bytes_received != sizeof(matrix)))
            continue;

        printf("Got a message from client %d: \n", client_data.id);

        double deter = determinant(matrix);

        client_data.data.oldDeterminants[client_data.data.currentIndexOfDeterminant] = deter;
        client_data.data.currentIndexOfDeterminant = (client_data.data.currentIndexOfDeterminant + 1) % DETERMINANT_HISTORY_SIZE;
        client_data.data.amountOfMatrix++;

        printf("Det. = %.2f\n", deter);

        if(client_data.data.amountOfMatrix >= DETERMINANT_HISTORY_SIZE)
        {
            double average = calculate_average(client_data.data.oldDeterminants);

            int index = (client_data.data.currentIndexOfDeterminant - DETERMINANT_HISTORY_SIZE + DETERMINANT_HISTORY_SIZE) % DETERMINANT_HISTORY_SIZE;

            printf("Avg.det = %.2f\n", average);
            printf("Del. det = %.2f\n", client_data.data.oldDeterminants[index]);
        }
        else
        {
            printf("Avg.det = N/A\n");
            printf("Del.det = N/A\n");
        }

        printf("End of message\n\n");
    }

    printf("Client disconnected\n");

    close(client_socket);

    return NULL;
}

int create_socket(int port, int maxNumberOfClients)
{
    struct sockaddr_in server_address;

    int server_socket;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if(server_socket == -1)
    {
        printf("Socket is failed");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if((bind(server_socket, (struct sockaddr*)&server_address, sizeof (server_address))) != 0)
    {
        printf("Bind is failed");
        return -1;
    }

    if((listen(server_socket, maxNumberOfClients)) == -1)
    {
        printf("Listen failed");
        return -1;
    }

    printf("Socket successfully created\n");

    return server_socket;
}

int main()
{
    int server_socket = create_socket(PORT, MAX_NUMBER_OF_CLIENTS);

    if(server_socket == -1)
    {
        printf("\nSocket creation is failed");
        exit(EXIT_FAILURE);
    }

    //To handle ctr + c signal
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    accept_clients(server_socket);

    return 0;
}
