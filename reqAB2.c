#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

#define MAX_POINTS 1000000

typedef struct {
    double x;
    double y;
} Point;

typedef struct {
    Point *points;
    Point *polygon;
    int start;
    int end;
    int num_polygon_points;
    int *total_inside;
    pthread_mutex_t *mutex;
} ThreadData;

typedef struct {
    int *total_processed;
    int num_random_points;
    pthread_mutex_t *mutex;
} ProgressData;

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

void *worker_thread(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int local_inside = 0;

    for (int i = data->start; i < data->end; i++) {
        if (isInsidePolygon(data->polygon, data->num_polygon_points, data->points[i])) {
            local_inside++;
        }
    }

    pthread_mutex_lock(data->mutex);
    *(data->total_inside) += local_inside;
    pthread_mutex_unlock(data->mutex);

    pthread_exit(NULL);
}

void *progress_thread(void *arg) {
    ProgressData *progress_data = (ProgressData *)arg;
    while (1) {
        sleep(1);
        pthread_mutex_lock(progress_data->mutex);
        int progress = (*(progress_data->total_processed) * 100) / progress_data->num_random_points;
        pthread_mutex_unlock(progress_data->mutex);
        printf("\rProgresso: %d%%", progress);
        fflush(stdout);
        if (progress >= 100) break;
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <arquivo_do_poligono> <num_threads> <num_pontos_aleatorios>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *poligono = argv[1];
    int num_threads = atoi(argv[2]);
    int num_pontos_aleatorios = atoi(argv[3]);

    if (num_threads <= 0 || num_pontos_aleatorios <= 0) {
        fprintf(stderr, "Erro: Número de threads e pontos deve ser maior que 0.\n");
        return EXIT_FAILURE;
    }

    int arquivo = open(poligono, O_RDONLY);
    if (arquivo < 0) {
        perror("Erro ao abrir o arquivo do polígono");
        return EXIT_FAILURE;
    }

    Point *polygon = malloc(MAX_POINTS * sizeof(Point));
    if (polygon == NULL) {
        perror("Erro ao alocar memória para o polígono");
        close(arquivo);
        return EXIT_FAILURE;
    }

    int capacity = MAX_POINTS;
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

    Point *pontos = malloc(num_pontos_aleatorios * sizeof(Point));
    if (pontos == NULL) {
        perror("Erro ao alocar memória para pontos");
        free(polygon);
        return EXIT_FAILURE;
    }

    srand((unsigned int)time(NULL));
    for (int i = 0; i < num_pontos_aleatorios; i++) {
        pontos[i].x = (double)rand() / RAND_MAX * 2;
        pontos[i].y = (double)rand() / RAND_MAX * 2;
    }

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    ThreadData *thread_data = malloc(num_threads * sizeof(ThreadData));
    int total_inside = 0;
    int total_processed = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    int points_per_thread = num_pontos_aleatorios / num_threads;
    int remaining_points = num_pontos_aleatorios % num_threads;

    for (int i = 0; i < num_threads; i++) {
        thread_data[i].points = pontos;
        thread_data[i].polygon = polygon;
        thread_data[i].start = i * points_per_thread;
        thread_data[i].end = thread_data[i].start + points_per_thread;
        if (i == num_threads - 1) {
            thread_data[i].end += remaining_points;
        }
        thread_data[i].num_polygon_points = n;
        thread_data[i].total_inside = &total_inside;
        thread_data[i].mutex = &mutex;

        pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
    }

    ProgressData progress_data = {
            .total_processed = &total_processed,
            .num_random_points = num_pontos_aleatorios,
            .mutex = &mutex
    };

    pthread_t progress_tid;
    pthread_create(&progress_tid, NULL, progress_thread, &progress_data);

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        pthread_mutex_lock(&mutex);
        total_processed += thread_data[i].end - thread_data[i].start;
        pthread_mutex_unlock(&mutex);
    }

    pthread_join(progress_tid, NULL);

    double area_of_reference = 4.0;
    double estimated_area = ((double)total_inside / num_pontos_aleatorios) * area_of_reference;
    printf("\nÁrea estimada do polígono: %.2f unidades quadradas\n", estimated_area);

    free(pontos);
    free(polygon);
    free(threads);
    free(thread_data);
    return EXIT_SUCCESS;
}
