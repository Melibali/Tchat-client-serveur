#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#define PORT 'mettre le num du port'
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    int identified;
    char name[16];
} Client;

Client clients[MAX_CLIENTS];

/* Envoie une chaîne complète au client */
int send_message(int socket, const char *message)
{
    size_t length = strlen(message);
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(socket, message + sent, length - sent, 0);

        if (n <= 0)
            return -1;

        sent += n;
    }

    return 0;
}

/* Initialise le tableau des clients */
void init_clients(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].identified = 0;
        clients[i].name[0] = '\0';
    }
}

/* Cherche un emplacement libre */
int find_free_client(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == -1)
            return i;
    }

    return -1;
}

/* Cherche un client par pseudo */
int find_client_by_name(const char *name)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 &&
            clients[i].identified &&
            strcmp(clients[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

/* Supprime un client */
void remove_client(int index)
{
    if (clients[index].socket != -1)
        close(clients[index].socket);

    clients[index].socket = -1;
    clients[index].identified = 0;
    clients[index].name[0] = '\0';
}

/* Vérifie un pseudo */
int valid_name(const char *name)
{
    size_t length = strlen(name);

    if (length < 1 || length > 15)
        return 0;

    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)name[i];

        /*
         * ASCII printable = 32 à 126.
         * L'espace est interdit.
         */
        if (c < 32 || c > 126 || c == ' ')
            return 0;
    }

    return 1;
}

/* Traite NAME */
void handle_name(int index, char *buffer)
{
    char name[16];

    if (sscanf(buffer, "NAME %15s", name) != 1) {
        send_message(clients[index].socket, "ERR! 10\n");
        return;
    }

    if (!valid_name(name)) {
        send_message(clients[index].socket, "ERR! 10\n");
        return;
    }

    if (find_client_by_name(name) != -1) {
        send_message(clients[index].socket, "ERR! 12\n");
        return;
    }

    strcpy(clients[index].name, name);
    clients[index].identified = 1;

    send_message(clients[index].socket, "OKAY\n");

    printf("Client %d identifié : %s\n",
           clients[index].socket,
           clients[index].name);
}

/* Traite une commande reçue */
void handle_command(int index, char *buffer)
{
    int socket = clients[index].socket;

    /* NAME */
    if (strncmp(buffer, "NAME ", 5) == 0) {
        handle_name(index, buffer);
        return;
    }

    /* PING */
    if (strcmp(buffer, "PING") == 0) {
        send_message(socket, "PONG\n");
        return;
    }

    /* QUIT */
    if (strcmp(buffer, "QUIT") == 0) {
        printf("Client déconnecté : %s\n",
               clients[index].identified
                   ? clients[index].name
                   : "(non identifié)");

        remove_client(index);
        return;
    }

    /*
     * Toutes les autres commandes nécessitent
     * une identification.
     */
    if (!clients[index].identified) {
        send_message(socket, "ERR! 02\n");
        return;
    }

    if (strncmp(buffer, "PRIV ", 5) == 0) {
        send_message(socket, "ERR! 11\n");
        return;
    }

    if (strncmp(buffer, "JOIN ", 5) == 0) {
        send_message(socket, "ERR! 01\n");
        return;
    }

    if (strncmp(buffer, "EXIT ", 5) == 0) {
        send_message(socket, "ERR! 03\n");
        return;
    }

    if (strcmp(buffer, "LIST") == 0) {
        send_message(socket, "LIST 0\n");
        return;
    }

    if (strncmp(buffer, "TALK ", 5) == 0) {
        send_message(socket, "ERR! 03\n");
        return;
    }

    send_message(socket, "ERR! 01\n");
}

int main(void)
{
    int server_socket;
    struct sockaddr_in address;
    int opt = 1;

    init_clients();

    /* Création de la socket */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /*
     * Permet de réutiliser rapidement le port
     * après l'arrêt du serveur.
     */
    if (setsockopt(server_socket,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_socket);
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    /* bind */
    if (bind(server_socket,
             (struct sockaddr *)&address,
             sizeof(address)) < 0) {
        perror("bind");
        close(server_socket);
        return EXIT_FAILURE;
    }

    /* listen */
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Serveur lancé sur le port %d...\n", PORT);

    while (1) {
        fd_set readfds;

        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);

        int max_fd = server_socket;

        /* Ajout des clients dans select() */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1) {
                FD_SET(clients[i].socket, &readfds);

                if (clients[i].socket > max_fd)
                    max_fd = clients[i].socket;
            }
        }

        int result = select(max_fd + 1,
                            &readfds,
                            NULL,
                            NULL,
                            NULL);

        if (result < 0) {
            if (errno == EINTR)
                continue;

            perror("select");
            break;
        }

        /* Nouvelle connexion */
        if (FD_ISSET(server_socket, &readfds)) {
            struct sockaddr_in client_address;
            socklen_t client_length = sizeof(client_address);

            int client_socket = accept(
                server_socket,
                (struct sockaddr *)&client_address,
                &client_length
            );

            if (client_socket < 0) {
                perror("accept");
            } else {
                int index = find_free_client();

                if (index == -1) {
                    send_message(client_socket, "ERR! 00\n");
                    close(client_socket);
                    printf("Connexion refusée : trop de clients.\n");
                } else {
                    clients[index].socket = client_socket;
                    clients[index].identified = 0;
                    clients[index].name[0] = '\0';

                    printf("Nouveau client connecté.\n");
                }
            }
        }

        /* Messages des clients */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int socket = clients[i].socket;

            if (socket == -1)
                continue;

            if (!FD_ISSET(socket, &readfds))
                continue;

            char buffer[BUFFER_SIZE];

            ssize_t received = recv(
                socket,
                buffer,
                sizeof(buffer) - 1,
                0
            );

            if (received <= 0) {
                printf("Connexion fermée.\n");
                remove_client(i);
                continue;
            }

            buffer[received] = '\0';

            /*
             * Pour cette première version, on retire
             * les retours à la ligne envoyés par le client.
             */
            buffer[strcspn(buffer, "\r\n")] = '\0';

            printf("Reçu : %s\n", buffer);

            handle_command(i, buffer);
        }
    }

    close(server_socket);

    return EXIT_SUCCESS;
}
