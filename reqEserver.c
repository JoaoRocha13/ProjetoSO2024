#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/polygon_socket"
#define BUFFER_SIZE 1024

typedef struct {
    double x;
    double y;
} Point;

ssize_t readn2(int fd, void *buffer, size_t n) {
    size_t left = n;
    ssize_t read_bytes;
    char *ptr = (char *) buffer;

    while (left > 0) {
        read_bytes = read(fd, ptr, left);
        if (read_bytes < 0) {
            if (errno == EINTR) continue;
            perror("Erro ao ler do socket");
            return -1;
        } else if (read_bytes == 0) {
            break;
        }
        left -= read_bytes;
        ptr += read_bytes;
    }
    return (n - left);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <arquivo_do_poligono> <num_processos_filho> <num_pontos_aleatorios>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *poligono = argv[1];
    int num_processos_filho = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);

    if (num_processos_filho <= 0 || num_pontos_aleatorios <= 0) {
        fprintf(stderr, "Erro: Números de processos e pontos devem ser maiores que 0.\n");
        return EXIT_FAILURE;
    }

    int arquivo = open(poligono, O_RDONLY);
    if (arquivo < 0) {
        perror("Erro ao abrir o arquivo do polígono");
        return EXIT_FAILURE;
    }

    Point *polygon = malloc(100 * sizeof(Point));
    if (polygon == NULL) {
        perror("Erro ao alocar memória para o polígono");
        close(arquivo);
        return EXIT_FAILURE;
    }
    int capacity = 100;
    int n = 0;
    char buffer[128];
    ssize_t bytesRead;
    char *line, *saveptr;

    while ((bytesRead = read(arquivo, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        line = strtok_r(buffer, "\n", &saveptr);
        while (line != NULL) {
            if (sscanf(line, "%lf %lf", &polygon[n].x, &polygon[n].y) == 2) {
                n++;
                if (n >= capacity) {
                    capacity *= 2;
                    polygon = realloc(polygon, capacity * sizeof(Point));
                    if (polygon == NULL) {
                        perror("Erro ao realocar memória para o polígono");
                        close(arquivo);
                        return EXIT_FAILURE;
                    }
                }
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }

    close(arquivo);

    if (n < 3) {
        fprintf(stderr, "Polígono inválido ou dados insuficientes no arquivo.\n");
        free(polygon);
        return EXIT_FAILURE;
    }

    // Remove o socket antigo, se existir
    if (unlink(SOCKET_PATH) == -1 && errno != ENOENT) {
        perror("Erro ao remover socket antigo");
        free(polygon);
        return EXIT_FAILURE;
    }

    int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Erro ao criar socket do servidor");
        free(polygon);
        return EXIT_FAILURE;
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_un)) < 0) {
        perror("Erro ao fazer bind do socket do servidor");
        close(server_sock);
        free(polygon);
        return EXIT_FAILURE;
    }

    if (listen(server_sock, num_processos_filho) < 0) {
        perror("Erro ao escutar no socket do servidor");
        close(server_sock);
        free(polygon);
        return EXIT_FAILURE;
    }

    printf("Servidor pronto e esperando conexões...\n");

    int total_pontos_dentro = 0;
    int total_pontos_processados = 0;

    for (int i = 0; i < num_processos_filho; i++) {
        printf("Aguardando conexão do cliente %d...\n", i + 1);
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) {
            perror("Erro ao aceitar conexão do cliente");
            continue;
        }

        printf("Cliente conectado: %d\n", i + 1);

        char buffer[BUFFER_SIZE];
        ssize_t bytesRead;
        while ((bytesRead = readn2(client_sock, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            int pid, processed, inside;
            double x, y;
            if (sscanf(buffer, "%d;%d;%d", &pid, &processed, &inside) == 3) {
                printf("%d;%d;%d\n", pid, processed, inside);
                total_pontos_dentro += inside;
                total_pontos_processados += processed;
            } else if (sscanf(buffer, "%d;%lf;%lf", &pid, &x, &y) == 3) {
                total_pontos_dentro++;
                total_pontos_processados++;
            }
        }
        printf("Cliente %d desconectado.\n", i + 1);
        close(client_sock);
    }

    while (wait(NULL) > 0);

    if (total_pontos_dentro > 0) {
        double area_of_reference = 4.0;
        double estimated_area = ((double) total_pontos_dentro / num_pontos_aleatorios) * area_of_reference;
        printf("Área estimada do polígono: %.6f unidades quadradas\n", estimated_area);
    }

    free(polygon);
    close(server_sock);
    unlink(SOCKET_PATH);
    return EXIT_SUCCESS;
}
