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
#include <sys/types.h>


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
            if (errno == EINTR) continue;  // Handle interrupted system call
            perror("Erro ao escrever no pipe");
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
    char *ptr = (char*) buffer;

    while (left > 0) {
        read_bytes = read(fd, ptr, left);
        if (read_bytes < 0) {
            if (errno == EINTR) continue;  // Handle interrupted system call
            perror("Erro ao ler do pipe");
            return -1;
        } else if (read_bytes == 0) {
            break; // EOF
        }
        left -= read_bytes;
        ptr += read_bytes;
    }
    return (n - left);
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL) + getpid());

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <polygon_file> <num_children> <num_random_points> <mode>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *poligono = argv[1];
    int num_processos_filho = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);
    char *modo = argv[4];

    int polygonFile = open(poligono, O_RDONLY);
    if (polygonFile < 0) {
        perror("Erro ao abrir arquivo de polígono");
        exit(EXIT_FAILURE);
    }

    Point polygon[100];
    int n = 0;
    double x, y;
    char line[256];
    while (read(polygonFile, line, sizeof(line)) > 0 && n < 100) {
        if (sscanf(line, "%lf %lf", &x, &y) == 2) {
            polygon[n].x = x;
            polygon[n].y = y;
            n++;
        }
    }
    close(polygonFile);

    if (n < 3) {
        fprintf(stderr, "Polígono inválido ou dados insuficientes no arquivo.\n");
        exit(EXIT_FAILURE);
    }

    int resultFile = open("resultados.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (resultFile < 0) {
        perror("Erro ao abrir arquivo de resultados");
        exit(EXIT_FAILURE);
    }
    close(resultFile); // Fecha imediatamente após abrir para limpar conteúdo

    int fd[num_processos_filho][2];
    int total_pontos_dentro = 0;
    int total_points_processed = 0;

    for (int i = 0; i < num_processos_filho; i++) {
        if (pipe(fd[i]) == -1) {
            perror("Erro ao criar o pipe");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid == 0) {  // Child process
            srand((unsigned int)time(NULL) + getpid());
            close(fd[i][0]);  // Close read end
            resultFile = open("resultados.txt", O_WRONLY | O_APPEND);  // Reabre para adicionar resultados

            int pontos_por_filho = num_pontos_aleatorios / num_processos_filho;
            int pontos_extra = num_pontos_aleatorios % num_processos_filho;
            int pontos_a_processar = pontos_por_filho + (i == num_processos_filho - 1 ? pontos_extra : 0);
            int pontos_dentro = 0;

            for (int j = 0; j < pontos_a_processar; j++) {
                Point p = {(double)rand() / RAND_MAX * 2, (double)rand() / RAND_MAX * 2};
                if (isInsidePolygon(polygon, n, p)) {
                    pontos_dentro++;
                    if (strcmp(modo, "verboso") == 0) {
                        char verbose_output[128];
                        sprintf(verbose_output, "%.2f;%.2f\n", p.x, p.y);
                        writen2(fd[i][1], verbose_output, strlen(verbose_output));
                    }
                }
            }
            char result[128];
            sprintf(result, "%d;%d;%d\n", getpid(), pontos_a_processar, pontos_dentro);
            write(resultFile, result, strlen(result)); // Escreve diretamente no arquivo de resultados
            close(resultFile);

            char output[128];
            sprintf(output, "%d;%d;%d\n", getpid(), pontos_a_processar, pontos_dentro);
            writen2(fd[i][1], output, strlen(output));
            close(fd[i][1]);
            exit(0);
        }
        close(fd[i][1]);  // Close write end in parent
    }

    // Parent process: Monitor progress and collect data
    int old_percent = -1;
    while (total_points_processed < num_pontos_aleatorios) {
        for (int i = 0; i < num_processos_filho; i++) {
            char buffer[1024];
            ssize_t bytes_read = readn2(fd[i][0], buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';  // Adiciona terminador de string
                int pid, processed, inside;
                if (sscanf(buffer, "%d;%d;%d", &pid, &processed, &inside) == 3) {
                    total_points_processed += processed;
                    total_pontos_dentro += inside;
                    if (strcmp(modo, "normal") == 0 && (strstr(buffer, ";") == NULL)) {
                        printf("Progresso: %s", buffer);  // Se não contiver, imprime o progresso
                    }
                }
                printf("%s", buffer); // Imprime os pontos gerados
                int percent = (int)((double)total_points_processed / num_pontos_aleatorios * 100.0);
                if (strcmp(modo, "normal") == 0 && percent != old_percent) {
                    printf("Progresso: %d%%\n", percent);
                    old_percent = percent;
                }
            }
        }
    }

    while (wait(NULL) > 0);  // Wait for all child processes to finish

    double area_of_reference = 4.0; // Area of the bounding square
    double estimated_area = ((double)total_pontos_dentro / num_pontos_aleatorios) * area_of_reference;
    printf("Área estimada do polígono: %.2f unidades quadradas\n", estimated_area);

    return 0;
}