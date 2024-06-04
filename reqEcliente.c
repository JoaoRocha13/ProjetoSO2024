#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

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

// Função para escrever dados em um socket, garantindo a escrita completa
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

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <arquivo_do_poligono> <num_processos_filho> <num_pontos_aleatorios> <modo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Extrai argumentos da linha de comando
    char *poligono = argv[1];
    int num_processos_filho = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);
    char *modo = argv[4];

    // Verifica se os argumentos são válidos
    if (num_processos_filho <= 0 || num_pontos_aleatorios <= 0) {
        fprintf(stderr, "Erro: Números de processos e pontos devem ser maiores que 0.\n");
        return EXIT_FAILURE;
    }

    // Abre o arquivo do polígono para leitura
    int arquivo = open(poligono, O_RDONLY);
    if (arquivo < 0) {
        perror("Erro ao abrir o arquivo do polígono");
        return EXIT_FAILURE;
    }

    // Aloca memória para armazenar os pontos do polígono
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

    // Lê os pontos do polígono do arquivo
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

    // Verifica se o polígono é válido
    if (n < 3) {
        fprintf(stderr, "Polígono inválido ou dados insuficientes no arquivo.\n");
        free(polygon);
        return EXIT_FAILURE;
    }

    // Aloca memória para armazenar pontos aleatórios
    Point *pontos = malloc(num_pontos_aleatorios * sizeof(Point));
    if (pontos == NULL) {
        perror("Erro ao alocar memória para pontos");
        free(polygon);
        return EXIT_FAILURE;
    }

    // Gera pontos aleatórios
    srand((unsigned int) time(NULL) + getpid());
    for (int i = 0; i < num_pontos_aleatorios; i++) {
        pontos[i].x = (double) rand() / RAND_MAX * 3.0 - 1.5;
        pontos[i].y = (double) rand() / RAND_MAX * 3.0 - 1.5;
    }

    // Cria processos filhos para processar pontos e se comunicar com o servidor
    for (int i = 0; i < num_processos_filho; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Processo filho
            int pontos_por_filho = num_pontos_aleatorios / num_processos_filho;
            int pontos_extra = num_pontos_aleatorios % num_processos_filho;
            int pontos_a_processar = pontos_por_filho + (i < pontos_extra ? 1 : 0);
            int pontos_dentro = 0;

            // Processa pontos e verifica se estão dentro do polígono
            for (int j = i * pontos_por_filho + (i < pontos_extra ? i : pontos_extra); j < i * pontos_por_filho + (i < pontos_extra ? i : pontos_extra) + pontos_a_processar; j++) {
                if (isInsidePolygon(polygon, n, pontos[j])) {
                    pontos_dentro++;
                    // Se o modo for 'verboso', imprime os pontos dentro do polígono
                    if (strcmp(modo, "verboso") == 0) {
                        char output[128];
                        snprintf(output, sizeof(output), "%d;%6lf;%6lf\n", getpid(), pontos[j].x, pontos[j].y);
                        write(STDOUT_FILENO, output, strlen(output));  // Escreve diretamente no terminal
                    }
                }
            }

            // Conecta-se ao servidor e envia dados
            int client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
            if (client_sock < 0) {
                perror("Erro ao criar socket do cliente");
                exit(EXIT_FAILURE);
            }

            // Cria uma estrutura sockaddr_un para representar o endereço do servidor
            struct sockaddr_un server_addr;
            memset(&server_addr, 0, sizeof(struct sockaddr_un)); // Limpa a estrutura de endereço
            server_addr.sun_family = AF_UNIX; // Define o tipo de endereço como AF_UNIX (Unix Domain)
            strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1); // Copia o caminho do socket para a estrutura de endereço
            printf("Tentando conectar ao servidor...\n");

            // Tenta estabelecer a conexão com o servidor
            if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_un)) < 0) {
                perror("Erro ao conectar ao socket do servidor");
                close(client_sock);
                exit(EXIT_FAILURE);
            }
            printf("Conectado ao servidor.\n");
            if (strcmp(modo, "normal") == 0) {
                // Cria uma string formatada contendo os dados a serem enviados para o servidor
                char output[128];
                snprintf(output, sizeof(output), "%d;%d;%d\n", getpid(), pontos_a_processar, pontos_dentro);
                // Envia os dados para o servidor através do socket do cliente
                if (writen2(client_sock, output, strlen(output)) < 0) {
                    perror("Erro ao escrever no socket");
                    close(client_sock);
                    exit(EXIT_FAILURE);
                }
            }
            printf("Conectado ao servidor.\n");

            // Envia dados para o servidor
            if (strcmp(modo, "normal") == 0) {
                char output[128];
                snprintf(output, sizeof(output), "%d;%d;%d\n", getpid(), pontos_a_processar, pontos_dentro);
                if (writen2(client_sock, output, strlen(output)) < 0) {
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
            return EXIT_FAILURE;
        }
    }

    // Libera a memória alocada
    free(pontos);
    free(polygon);

    // Aguarda todos os processos filhos terminarem
    while (wait(NULL) > 0);

    return EXIT_SUCCESS;
}