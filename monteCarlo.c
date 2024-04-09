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

int main(int argc, char* argv[]) {

////////////////////////////////////////////////////////////////////////////////////////REQA/////////////////////////////////////////////////////////////////////////
    srand((unsigned int) time(NULL));
//Verificar o Número de Argumentos
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <arquivo_do_poligono> <num_processos_filho> <num_pontos_aleatorios>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    //Extrair os Argumentos: Converta os argumentos do número de processos filho e do número de pontos aleatórios de strings para inteiro
    char *poligono = argv[1];
    int num_processos_filho = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);


    if (num_processos_filho <= 0 || num_pontos_aleatorios <= 0) {
        fprintf(stderr, "Erro: Números de processos e pontos devem ser maiores que 0.\n");
        exit(EXIT_FAILURE);
    }
    // Ler o Arquivo do Polígono
    FILE *arquivo = fopen(poligono, "r");
    if (arquivo == NULL) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    Point polygon[100]; // Use alocação dinâmica para flexibilidade
    int n = 0; // Contador de pontos no polígono

    while (fscanf(arquivo, "%lf %lf", &polygon[n].x, &polygon[n].y) == 2) { // Verifique se a leitura foi bem-sucedida
        n++; // Incrementa o contador de pontos
        if (n >= 100) break; // Ajuste conforme necessário
    }

    fclose(arquivo); // Mova fclose para aqui, após a leitura

    if (n < 3) {
        fprintf(stderr, "Polígono inválido ou dados insuficientes no arquivo.\n");
        exit(EXIT_FAILURE);
    }

/////////////////////////////////////////////////////////////////////////////////////////REQB////////////////////////////////////////////////////////////////////////

    int fd = open("resultados.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Erro ao abrir/criar o arquivo de resultados");
        exit(EXIT_FAILURE);
    }

    int pontos_por_filho = num_pontos_aleatorios / num_processos_filho;
    int pontos_extra = num_pontos_aleatorios % num_processos_filho; // Pontos extras caso a divisão não seja exata

    for (int i = 0; i < num_processos_filho; i++) {
        pid_t pid = fork();
        if (pid == 0) { // Código do processo filho
            int pontos_a_processar = pontos_por_filho + (i == num_processos_filho - 1 ? pontos_extra
                                                                                      : 0); // Último filho pega os pontos extras
            int pontos_dentro = 0;
            for (int j = 0; j < pontos_a_processar; j++) {
                Point p = {(double) rand() / RAND_MAX * 2, (double) rand() / RAND_MAX * 2};
                if (isInsidePolygon(polygon, n, p)) {
                    pontos_dentro++;
                }
            }

            // Escreve os resultados no arquivo de resultados
            dprintf(fd, "%d;%d;%d\n", getpid(), pontos_a_processar, pontos_dentro);
            close(fd);
            exit(0); // Termina o processo filho
        } else if (pid < 0) {
            perror("Erro no fork");
            exit(EXIT_FAILURE);
        }
    }

    // Aguarda todos os processos filho terminarem
    while (wait(NULL) > 0);

    close(fd);

    fd = open("resultados.txt", O_RDONLY);
    if (fd < 0) {
        perror("Erro ao abrir o arquivo de resultados para leitura");
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    int totalPontosDentro = 0, totalPontosProcessados = 0;
    ssize_t bytesLidos;

// Lê os resultados do arquivo e os agrega
    while ((bytesLidos = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesLidos] = '\0'; // Garante que o buffer seja uma string válida
        char *linha = strtok(buffer, "\n");
        while (linha != NULL) {
            int pidFilho, pontosProcessados, pontosDentro;
            sscanf(linha, "%d;%d;%d", &pidFilho, &pontosProcessados, &pontosDentro);
            totalPontosDentro += pontosDentro;
            totalPontosProcessados += pontosProcessados;
            linha = strtok(NULL, "\n");
        }
    }
    close(fd);

// Agora, calcula e exibe a área estimada com base nos resultados agregados
    if (totalPontosProcessados > 0) { // Evita divisão por zero
        double areaEstimada = 4.0 * (double) totalPontosDentro / totalPontosProcessados;
        printf("Área estimada do polígono: %f\n", areaEstimada);
    } else {
        printf("Nenhum ponto processado.\n");
    }

    return 0;
}