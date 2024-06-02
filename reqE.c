
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

/**
 * @brief Determines the orientation of an ordered triplet (p, q, r).
 * @param p First point of the triplet.
 * @param q Second point of the triplet.
 * @param r Third point of the triplet.
 * @return 0 if p, q, and r are colinear, 1 if clockwise, 2 if counterclockwise.
 */
int orientation(Point p, Point q, Point r) {
    double val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);

    if (val == 0) return 0;
    return (val > 0) ? 1 : 2;
}

/**
 * @brief Checks if point q lies on line segment pr.
 * @param p First point of the line segment.
 * @param q Point to check.
 * @param r Second point of the line segment.
 * @return true if point q lies on line segment pr, else false.
 */
bool onSegment(Point p, Point q, Point r) {
    if (q.x <= fmax(p.x, r.x) && q.x >= fmin(p.x, r.x) &&
        q.y <= fmax(p.y, r.y) && q.y >= fmin(p.x, r.x))
        return true;

    return false;
}

/**
 * @brief Checks if line segments p1q1 and p2q2 intersect.
 * @param p1 First point of the first line segment.
 * @param q1 Second point of the first line segment.
 * @param p2 First point of the second line segment.
 * @param q2 Second point of the second line segment.
 * @return true if line segments p1q1 and p2q2 intersect, else false.
 */
bool doIntersect(Point p1, Point q1, Point p2, Point q2) {
    int o1 = orientation(p1, q1, p2);
    int o2 = orientation(p1, q1, q2);
    int o3 = orientation(p2, q2, p1);
    int o4 = orientation(p2, q2, q1);

    if (o1 != o2 && o3 != o4)
        return true;

    if (o1 == 0 && onSegment(p1, p2, q1)) return true;
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;
    if (o4 == 0 && onSegment(p2, q1, q2)) return true;

    return false;
}

/**
 * @brief Checks if a point p is inside a polygon of n points.
 * @param polygon[] Array of points forming the polygon.
 * @param n Number of points in the polygon.
 * @param p Point to check.
 * @return true if the point p is inside the polygon, else false.
 */
bool isInsidePolygon(Point polygon[], int n, Point p) {
    if (n < 3) return false;

    Point extreme = {2.5, p.y};

    int count = 0, i = 0;
    do {
        int next = (i + 1) % n;

        if (doIntersect(polygon[i], polygon[next], p, extreme)) {
            if (orientation(polygon[i], p, polygon[next]) == 0)
                return onSegment(polygon[i], p, polygon[next]);
            count++;
        }
        i = next;
    } while (i != 0);

    return count & 1;
}

ssize_t writen2(int fd, const void *buffer, size_t n) {
    size_t left = n;
    ssize_t written_bytes;
    const char *ptr = (const char *) buffer;

    while (left > 0) {
        written_bytes = write(fd, ptr, left);
        if (written_bytes <= 0) {
            if (errno == EINTR) continue;
            perror("Erro ao escrever no socket");
            return -1;
        }
        left -= written_bytes;
        ptr += written_bytes;
    }
    return (n - left);
}

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
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <arquivo_do_poligono> <num_processos_filho> <num_pontos_aleatorios> <modo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *poligono = argv[1];
    int num_processos_filho = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);
    char *modo = argv[4];

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
            if (sscanf(line, "%6lf %6lf", &polygon[n].x, &polygon[n].y) == 2) {
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

    Point *pontos = malloc(num_pontos_aleatorios * sizeof(Point));
    if (pontos == NULL) {
        perror("Erro ao alocar memória para pontos");
        free(polygon);
        return EXIT_FAILURE;
    }
    srand((unsigned int) time(NULL) + getpid());
    for (int i = 0; i < num_pontos_aleatorios; i++) {
        pontos[i].x = (double) rand() / RAND_MAX * 3.0 - 1.5;
        pontos[i].y = (double) rand() / RAND_MAX * 3.0 - 1.5;
    }



    //Configuração do Socket do Servidor


    int server_sock, client_sock; // Descritores de arquivo para os sockets do servidor e do cliente
    struct sockaddr_un server_addr, client_addr; // Estruturas de endereço para o servidor e o cliente
    socklen_t client_addr_len = sizeof(struct sockaddr_un); // Tamanho da estrutura de endereço do cliente

    unlink(SOCKET_PATH);// Remove o socket antigo, se existir

    //Criação do Socket do Servidor
    server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Erro ao criar socket do servidor");
        free(pontos);
        free(polygon);
        return EXIT_FAILURE;
    }

    //Inicialização da Estrutura de Endereço do Servidor
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    //Associar o socket do servidor a um endereço específico
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_un)) < 0) {
        perror("Erro ao fazer bind do socket do servidor");
        close(server_sock);
        free(pontos);
        free(polygon);
        return EXIT_FAILURE;
    }

    //Colocar o socket do servidor em modo de escuta, pronto para aceitar conexões.
    if (listen(server_sock, num_processos_filho) < 0) {
        perror("Erro ao escutar no socket do servidor");
        close(server_sock);
        free(pontos);
        free(polygon);
        return EXIT_FAILURE;
    }

    pid_t pids[num_processos_filho];

    for (int i = 0; i < num_processos_filho; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Processo filho
            int pontos_por_filho = num_pontos_aleatorios / num_processos_filho;
            int pontos_extra = num_pontos_aleatorios % num_processos_filho;
            int pontos_a_processar = pontos_por_filho + (i < pontos_extra ? 1 : 0);
            int pontos_dentro = 0;

            for (int j = i * pontos_por_filho + (i < pontos_extra ? i : pontos_extra); j < i * pontos_por_filho + (i < pontos_extra ? i : pontos_extra) + pontos_a_processar; j++) {
                if (isInsidePolygon(polygon, n, pontos[j])) {
                    pontos_dentro++;
                    if (strcmp(modo, "verboso") == 0) {
                        char output[128];
                        snprintf(output, sizeof(output), "%d;%6lf;%6lf\n", getpid(), pontos[j].x, pontos[j].y);
                        write(STDOUT_FILENO, output, strlen(output));  // Escreve diretamente no terminal
                    }
                }
            }
            //Criação e Conexão do Socket do Cliente
            int client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
            if (client_sock < 0) {
                perror("Erro ao criar socket do cliente");
                exit(EXIT_FAILURE);
            }
            //O socket cria um socket do cliente e connect estabelece a conexão com o servidor.
            if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_un)) < 0) {
                perror("Erro ao conectar ao socket do servidor");
                close(client_sock);
                exit(EXIT_FAILURE);
            }

            if (strcmp(modo, "normal") == 0) {
                char output[128];
                snprintf(output, sizeof(output), "%d;%d;%d\n", getpid(), pontos_a_processar, pontos_dentro);
                if (writen2(client_sock, output, strlen(output)) < 0) { // Escreve no socket
                    perror("Erro ao escrever no socket");
                    close(client_sock);
                    exit(EXIT_FAILURE);
                }
            }

            close(client_sock);
            free(pontos);
            free(polygon);
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            perror("Erro ao fazer fork");
            free(pontos);
            free(polygon);
            close(server_sock);
            return EXIT_FAILURE;
        } else {
            pids[i] = pid;
        }
    }
    //Aceitação de Conexões e Leitura dos Dados no Processo Pai
    int total_pontos_dentro = 0;
    int total_pontos_processados = 0;

    for (int i = 0; i < num_processos_filho; i++) {
        client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_addr_len); // Aceita conexão do cliente
        if (client_sock < 0) {
            perror("Erro ao aceitar conexão do cliente");
            continue;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytesRead;
        while ((bytesRead = readn2(client_sock, buffer, sizeof(buffer) - 1)) > 0) { // Lê do socket
            buffer[bytesRead] = '\0';
            int pid, processed, inside;
            double x, y;
            if (strcmp(modo, "verboso") == 0) {
                printf("%s", buffer);  // Imprime diretamente as linhas lidas no modo verboso
            }
            if (sscanf(buffer, "%d;%d;%d", &pid, &processed, &inside) == 3) {
                printf("%d;%d;%d\n", pid, processed, inside); // Imprime as linhas lidas no modo normal
                total_pontos_dentro += inside;
                total_pontos_processados += processed;
            } else if (sscanf(buffer, "%d;%6lf;%6lf", &pid, &x, &y) == 3) {
                total_pontos_dentro++;
                total_pontos_processados++;
            }
        }
        close(client_sock);
    }

    while (wait(NULL) > 0);

    if (total_pontos_dentro > 0) {
        double area_of_reference = 4.0;
        double estimated_area = ((double) total_pontos_dentro / num_pontos_aleatorios) * area_of_reference;
        printf("Área estimada do polígono: %.6f unidades quadradas\n", estimated_area);
    }

    free(pontos);
    free(polygon);
    close(server_sock);
    unlink(SOCKET_PATH);
    return EXIT_SUCCESS;
}
