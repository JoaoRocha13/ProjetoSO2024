#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
typedef struct {
    double x;
    double y;
} Point;

//#define NUM_POINTS 10000

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
    return (val > 0)? 1: 2;
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
        q.y <= fmax(p.y, r.y) && q.y >= fmin(p.y, r.y))
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

    // p1, q1 and p2 are colinear and p2 lies on segment p1q1
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;

    // p1, q1 and q2 are colinear and q2 lies on segment p1q1
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;

    // p2, q2 and p1 are colinear and p1 lies on segment p2q2
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;

    // p2, q2 and q1 are colinear and q1 lies on segment p2q2
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
        int next = (i+1)%n;

        if (doIntersect(polygon[i], polygon[next], p, extreme)) {
            if (orientation(polygon[i], p, polygon[next]) == 0)
                return onSegment(polygon[i], p, polygon[next]);
            count++;
        }
        i = next;
    } while (i != 0);

    return count&1;
}
ssize_t writen2(int fd, const void *buffer, size_t n) {
    size_t left = n;
    ssize_t written_bytes;
    const char *ptr = (const char*) buffer;

    while (left > 0) {
        written_bytes = write(fd, ptr, left);
        if (written_bytes <= 0) {
            fprintf(stderr, "Erro ao escrever no pipe: %s\n", strerror(errno));
            return -1;  // Retornar -1 em caso de falha
        }
        left -= written_bytes;
        ptr += written_bytes;
    }
    return (n - left);  // Deve retornar 0 se tudo foi escrito
}

ssize_t readn2(int fd, void *buffer, size_t n) {
    size_t left = n;
    ssize_t read_bytes;
    char *ptr = (char*) buffer;

    while (left > 0) {
        read_bytes = read(fd, ptr, left);
        if (read_bytes < 0) {
            fprintf(stderr, "Erro ao ler do pipe: %s\n", strerror(errno));
            return -1;  // Retornar -1 em caso de falha
        } else if (read_bytes == 0) {
            break;  // EOF
        }
        left -= read_bytes;
        ptr += read_bytes;
    }
    return (n - left);  // Deve ser 0 se tudo foi lido
}


int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL) + getpid());  // Assegura semente aleatória única por processo

    if (argc != 5) {
        fprintf(stderr, "Uso: %s <arquivo_do_poligono> <num_processos_filho> <num_pontos_aleatorios> <modo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *poligono = argv[1];
    int num_processos_filho = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);
    char *modo = argv[4];  // 'normal' ou 'verboso'

    FILE *arquivo = fopen(poligono, "r");
    if (arquivo == NULL) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    Point polygon[100];
    int n = 0;
    while (fscanf(arquivo, "%lf %lf", &polygon[n].x, &polygon[n].y) == 2 && n < 100) {
        n++;
    }
    fclose(arquivo);

    if (n < 3) {
        fprintf(stderr, "Polígono inválido ou dados insuficientes no arquivo.\n");
        exit(EXIT_FAILURE);
    }

    int fd[num_processos_filho][2];
    FILE *file = fopen("resultados.txt", "w");
    if (file == NULL) {
        perror("Falha ao abrir arquivo para escrita");
        exit(EXIT_FAILURE);
    }


    for (int i = 0; i < num_processos_filho; i++) {
        if (pipe(fd[i]) == -1) {
            perror("Erro ao criar pipe");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid == 0) {  // Processo filho
            srand((unsigned int)time(NULL) + getpid());
            int pontos_por_filho = num_pontos_aleatorios / num_processos_filho;
            int pontos_extra = num_pontos_aleatorios % num_processos_filho;
            int pontos_a_processar = pontos_por_filho + (i == num_processos_filho - 1 ? pontos_extra : 0);
            int pontos_dentro = 0;

            for (int j = 0; j < pontos_a_processar; j++) {
                Point p = {(double)rand() / RAND_MAX * 2, (double)rand() / RAND_MAX * 2};
                if (isInsidePolygon(polygon, n, p)) {
                    pontos_dentro++;
                }
            }

            fprintf(file, "%d;%d;%d\n", getpid(), pontos_a_processar, pontos_dentro);
            fclose(file); // Close the file in each child to flush the output

            if (strcmp(modo, "normal") == 0) {
                char output[128] = {0}; // Inicializa o buffer com zeros
                sprintf(output, "PID %d encontrou %d pontos dentro.\n", getpid(), pontos_dentro);
                writen2(fd[i][1], output, strlen(output) + 1); // Inclui o caractere nulo na escrita
            } else if (strcmp(modo, "verboso") == 0) {
                // Verbose mode is similar but could include more detailed output
                char output[128];
                sprintf(output, "PID %d processou %d pontos, %d dentro.\n", getpid(), pontos_a_processar, pontos_dentro);
                writen2(fd[i][1], output, strlen(output));
            }
            exit(0);
        }
        close(fd[i][1]);  // Fecha o lado de escrita no pai
    }

    // Reading from pipes
    for (int i = 0; i < num_processos_filho; i++) {
        char buffer[1024] = {0}; // Limpa o buffer
        while (readn2(fd[i][0], buffer, sizeof(buffer) - 1) > 0) {
            printf("%s", buffer);
            memset(buffer, 0, sizeof(buffer)); // Limpa o buffer após o uso
        }
        close(fd[i][0]);
    }

    while (wait(NULL) > 0);  // Aguarda todos os processos filho terminarem

    return 0;
}
    /*srand((unsigned int)time(NULL) + getpid());  // Assegura semente aleatória única por processo

    if (argc != 5) {
        fprintf(stderr, "Uso: %s <arquivo_do_poligono> <num_processos_filho> <num_pontos_aleatorios> <modo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *poligono = argv[1];
    int num_processos_filho = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);

    char *modo = argv[4];  // 'normal' ou 'verboso'

    if (num_processos_filho <= 0 || num_pontos_aleatorios <= 0) {
        fprintf(stderr, "Erro: Números de processos e pontos devem ser maiores que 0.\n");
        exit(EXIT_FAILURE);
    }

    FILE *arquivo = fopen(poligono, "r");
    if (arquivo == NULL) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    Point polygon[100];
    int n = 0;
    while (fscanf(arquivo, "%lf %lf", &polygon[n].x, &polygon[n].y) == 2 && n < 100) {
        n++;
    }
    fclose(arquivo);

    if (n < 3) {
        fprintf(stderr, "Polígono inválido ou dados insuficientes no arquivo.\n");
        exit(EXIT_FAILURE);
    }

    int fd[num_processos_filho][2];
    for (int i = 0; i < num_processos_filho; i++) {
        if (pipe(fd[i]) == -1) {
            perror("Erro ao criar pipe");
            exit(EXIT_FAILURE);
        }
    }
////////////////////////////////////////////////////////////////////////REQC/////////////////////////////////////////
    for (int i = 0; i < num_processos_filho; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Processo filho
            srand((unsigned int)time(NULL) + getpid());  // Reinicia a semente aleatória
            close(fd[i][0]);  // Fecha lado de leitura
            FILE *file = fopen("resultados.txt", "a");  // Abre o arquivo em modo de anexação
            if (file == NULL) {
                perror("Falha ao abrir arquivo para escrita");
                exit(EXIT_FAILURE);
            }
            int pontos_por_filho = num_pontos_aleatorios / num_processos_filho;
            int pontos_extra = num_pontos_aleatorios % num_processos_filho;
            int pontos_a_processar = pontos_por_filho + (i == num_processos_filho - 1 ? pontos_extra : 0);

            if (strcmp(modo, "normal") == 0) {
                int pontos_dentro = 0;
                for (int j = 0; j < pontos_a_processar; j++) {
                    Point p = {(double)rand() / RAND_MAX * 2, (double)rand() / RAND_MAX * 2};
                    if (isInsidePolygon(polygon, n, p)) {
                        pontos_dentro++;
                    }
                }

                char output[128];
                sprintf(output, "%d;%d\n", getpid(), pontos_dentro);
                writen2(fd[i][1], output, strlen(output));
            } else {  // Modo verboso
                for (int j = 0; j < pontos_a_processar; j++) {
                    Point p = {(double)rand() / RAND_MAX * 2, (double)rand() / RAND_MAX * 2};
                    if (isInsidePolygon(polygon, n, p)) {
                        char output[128];
                        sprintf(output, "%d;%.2f;%.2f\n", getpid(), p.x, p.y);
                        writen2(fd[i][1], output, strlen(output));
                    }
                }
            }
            close(fd[i][1]);
            exit(0);
        } else if (pid < 0) {
            perror("Falha ao criar processo filho");
            exit(EXIT_FAILURE);
        }
        close(fd[i][1]);  // Fecha o lado de escrita no pai
    }
    int total_pontos_dentro = 0;
    for (int i = 0; i < num_processos_filho; i++) {
        char buffer[1024];
        while (readn2(fd[i][0], buffer, sizeof(buffer) - 1) > 0) {
            buffer[strlen(buffer)] = '\0';
            if (strcmp(modo, "normal") == 0) {
                int pid, pontos_dentro;
                sscanf(buffer, "%d;%d", &pid, &pontos_dentro);
                printf("PID %d encontrou %d pontos dentro.\n", pid, pontos_dentro);
                total_pontos_dentro += pontos_dentro; // Soma os pontos dentro
            } else {
                int pid;
                double x, y;
                sscanf(buffer, "%d;%lf;%lf", &pid, &x, &y);
                printf("PID %d ponto dentro: X=%.2f, Y=%.2f\n", pid, x, y);
            }
        }
        close(fd[i][0]);
    }

// Fechar os pipes
    for (int i = 0; i < num_processos_filho; i++) {
        close(fd[i][0]);
    }

// Calcular e exibir a área estimada
    if (strcmp(modo, "normal") == 0 && total_pontos_dentro > 0) {
        double area_estimada = (double)total_pontos_dentro / num_pontos_aleatorios * 4.0;  // Supõe uma área de referência de 4 unidades quadradas
        printf("Área estimada do polígono: %.2f unidades quadradas\n", area_estimada);
    }



    while (wait(NULL) > 0);  // Aguarda todos os processos filho terminarem

    return 0;
}*/