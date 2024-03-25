#include <main.h>

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

int main(int argc, char *argv[]) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s <polygon file> <number of child processes> <total number of random points>\n", argv[0]);
            return EXIT_FAILURE;
        }

        const char* polygonFile = argv[1];
        int numChildProcesses = atoi(argv[2]);
        int totalNumPoints = atoi(argv[3]);

        // Opening the polygon file with POSIX system call
        int fd = open(polygonFile, O_RDONLY);
        if (fd == -1) {
            perror("Error opening polygon file");
            return EXIT_FAILURE;
        }

        // Placeholder: Implement the logic for reading polygon points from the file here
        // For the purpose of this example, we're directly closing the file
        // You would typically read the file content here before closing
        close(fd);

        srand(time(NULL));

        // This is a placeholder for the actual polygon points array which should be populated based on the file's content
        Point polygon[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        int n = sizeof(polygon) / sizeof(polygon[0]);

        int pointsInside = 0;
        for (int i = 0; i < totalNumPoints; i++) {
            Point p = {((double)rand() / RAND_MAX) * 2 - 1, ((double)rand() / RAND_MAX) * 2 - 1};
            if (isInsidePolygon(polygon, n, p)) {
                pointsInside++;
            }
        }

        double squareArea = 4.0; // Assuming a bounding box from -1 to 1 for both x and y for simplicity
        double polygonArea = squareArea * ((double)pointsInside / totalNumPoints);

        printf("Estimated area of the polygon: %f\n", polygonArea);

        return 0;
    }