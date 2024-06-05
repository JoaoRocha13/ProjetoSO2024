#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/wait.h>
#include <string.h>

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

    if (o1 != o2 && o3 != o4) return true;
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

int main(int argc, char* argv[]) {
    srand((unsigned int) time(NULL));

    if (argc != 4) {
        char usage[] = "Uso: <arquivo_do_poligono> <num_processos_filho> <num_pontos_aleatorios>\n";
        write(STDERR_FILENO, usage, strlen(usage));
        exit(EXIT_FAILURE);
    }

    char *poligono = argv[1];
    int num_processos_filho = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);

    if (num_processos_filho <= 0 || num_pontos_aleatorios <= 0) {
        char error[] = "Erro: Números de processos e pontos devem ser maiores que 0.\n";
        write(STDERR_FILENO, error, strlen(error));
        exit(EXIT_FAILURE);
    }


    //Leitura do Arquivo do Polígono
    int arquivo = open(poligono, O_RDONLY);
    if (arquivo < 0) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }
    // Alocação de memória para armazenar os pontos do polígono.
    Point* polygon = malloc(100 * sizeof(Point));
    int capacity = 100;
    int n = 0;
    char buffer[128];
    ssize_t bytesRead;
    char *line, *saveptr;

    //lê os pontos do polígono a partir de um arquivo e armazena esses pontos em um array dinâmico.
    while ((bytesRead = read(arquivo, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        line = strtok_r(buffer, "\n", &saveptr);
        while (line != NULL) {
            if (sscanf(line, "%lf %lf", &polygon[n].x, &polygon[n].y) == 2) {
                n++;
                if (n >= capacity) {
                    capacity *= 2;
                    polygon = realloc(polygon, capacity * sizeof(Point));
                }
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }

    close(arquivo);


    if (n < 3) {
        char error[] = "Polígono inválido ou dados insuficientes no arquivo.\n";
        write(STDERR_FILENO, error, strlen(error));
        free(polygon);
        exit(EXIT_FAILURE);
    }
    Point* pontos = malloc(num_pontos_aleatorios * sizeof(Point));
    for (int i = 0; i < num_pontos_aleatorios; i++) {
        pontos[i].x = (double) rand() / RAND_MAX * 2.0 - 1.0;
        pontos[i].y = (double) rand() / RAND_MAX * 2.0 - 1.0;
    }

    int fd = open("resultados.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Erro ao abrir/criar o arquivo de resultados");
        free(polygon);
        free(pontos);
        exit(EXIT_FAILURE);
    }
    close(fd);



    //Requisito B: Criação e Comunicação com Processos Filhos


    //Criação de Processos Filhos e Distribuição de Pontos

    int pontos_por_filho = num_pontos_aleatorios / num_processos_filho;
    int pontos_extra = num_pontos_aleatorios % num_processos_filho;  // Pontos extras caso a divisão não seja exata

    for (int i = 0; i < num_processos_filho; i++) {
        pid_t pid = fork();//Cria processos filhos
        if (pid == 0) {
            int pontos_a_processar = pontos_por_filho + (i == num_processos_filho - 1 ? pontos_extra : 0);
            int pontos_dentro = 0;
            //Verifica quais pontos estão dentro do polígono
            for (int j = i * pontos_por_filho; j < (i * pontos_por_filho + pontos_a_processar); j++) {
                if (isInsidePolygon(polygon, n, pontos[j])) {
                    snprintf(buffer, sizeof(buffer), "Ponto (%.6lf, %.6lf) está dentro do polígono.\n", pontos[j].x, pontos[j].y);
                    write(STDOUT_FILENO, buffer, strlen(buffer));
                    pontos_dentro++;
                } else {
                    snprintf(buffer, sizeof(buffer), "Ponto (%.6lf, %.6lf) está fora do polígono.\n", pontos[j].x, pontos[j].y);
                    write(STDOUT_FILENO, buffer, strlen(buffer));
                }
            }

            int fd_filho = open("resultados.txt", O_WRONLY | O_APPEND);
            if (fd_filho < 0) {
                perror("Erro ao abrir o arquivo de resultados");
                free(polygon);
                free(pontos);
                exit(EXIT_FAILURE);
            }
            char result[128];
            snprintf(result, sizeof(result), "%d;%d;%d\n", getpid(), pontos_a_processar, pontos_dentro);
            if (write(fd_filho, result, strlen(result)) < 0) {
                perror("Erro ao escrever no arquivo de resultados");
                close(fd_filho);
                free(polygon);
                free(pontos);
                exit(EXIT_FAILURE);
            }
            close(fd_filho);
            free(polygon);
            free(pontos);
            exit(0);
        } else if (pid < 0) {
            perror("Erro no fork");
            free(polygon);
            free(pontos);
            exit(EXIT_FAILURE);
        }
    }

    while (wait(NULL) > 0);

    //Leitura e Agregação dos Resultados no ficheiro
    fd = open("resultados.txt", O_RDONLY);
    if (fd < 0) {
        perror("Erro ao abrir o arquivo de resultados para leitura");
        free(polygon);
        free(pontos);
        exit(EXIT_FAILURE);
    }
    //Leitura dos resultados dos processos filhos
    char resultBuffer[128];
    ssize_t resultBytesRead;
    while ((resultBytesRead = read(fd, resultBuffer, sizeof(resultBuffer) - 1)) > 0) {
        resultBuffer[resultBytesRead] = '\0';
        char *linha = strtok(resultBuffer, "\n");
        while (linha != NULL) {
            int pidFilho, pontosProcessados, pontosDentro;
            sscanf(linha, "%d;%d;%d", &pidFilho, &pontosProcessados, &pontosDentro);
            printf("%d;%d;%d\n", pidFilho, pontosProcessados, pontosDentro);
            linha = strtok(NULL, "\n");
        }
    }
    close(fd);
    free(polygon);
    free(pontos);

    return 0;
}
