//
// Created by so on 25-03-2024.
//

#ifndef PROJETOSO2024_MONTECARLO_H
#define PROJETOSO2024_MONTECARLO_H


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


typedef struct {
    double x;
    double y;
} Point;

#define SOCKET_PATH "/tmp/polygon_socket"

#endif //PROJETOSO2024_MONTECARLO_H
