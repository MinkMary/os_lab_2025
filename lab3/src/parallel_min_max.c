#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

static bool parse_int(const char *str, int min_value, int *result) {
  char *end;
  errno = 0;

  long value = strtol(str, &end, 10);

  if (end == str || *end != '\0' || errno == ERANGE ||
      value < min_value || value > INT_MAX) {
    return false;
  }

  *result = (int)value;
  return true;
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            if (!parse_int(optarg, 0, &seed)) {
              fprintf(stderr, "Ошибка: seed должен быть целым числом от 0 до %d\n",
                      INT_MAX);
              return 1;
            }
            break;

          case 1:
            if (!parse_int(optarg, 1, &array_size)) {
              fprintf(stderr, "Ошибка: array_size должен быть целым числом от 1 до %d\n",
                      INT_MAX);
              return 1;
            }
            break;

          case 2:
            if (!parse_int(optarg, 1, &pnum)) {
              fprintf(stderr, "Ошибка: pnum должен быть целым числом от 1 до %d\n",
                      INT_MAX);
              return 1;
            }
            break;

          case 3:
            with_files = true;
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;

      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files]\n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  if (array == NULL) {
    perror("malloc");
    return 1;
  }
  GenerateArray(array, array_size, seed);

  int active_child_processes = 0;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  /* ---- Подготовка средств межпроцессного взаимодействия ---- */

  int pipefds[pnum][2];
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipefds[i]) == -1) {
        perror("pipe");
        free(array);
        return 1;
      }
    }
  }

  int fd = -1;
  if (with_files) {
    fd = open("min_max.txt", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
      perror("open");
      free(array);
      return 1;
    }
  }

  int slice = array_size / pnum;

  /* ---- Запуск дочерних процессов ---- */

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();

    if (child_pid == -1) {
      perror("fork");
      free(array);
      return 1;
    }

    if (child_pid == 0) {
      /* ===== Дочерний процесс ===== */
      int start = i * slice;
      int end = (i == pnum - 1) ? array_size : (i + 1) * slice;

      struct MinMax local = GetMinMax(array, start, end);
      int buf[2] = {local.min, local.max};

      if (with_files) {
        if (write(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
          perror("write to file");
          exit(1);
        }
      } else {
        /* Закрываем все чужие концы pipe'ов, унаследованные от родителя */
        for (int j = 0; j < pnum; j++) {
          if (j != i) {
            close(pipefds[j][0]);
            close(pipefds[j][1]);
          }
        }
        close(pipefds[i][0]);
        if (write(pipefds[i][1], buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
          perror("write to pipe");
          exit(1);
        }
        close(pipefds[i][1]);
      }

      exit(0);   /* ВАЖНО: exit, а не return */
    }

    /* ===== Родитель ===== */
    active_child_processes += 1;

    if (!with_files) {
      /* Родителю запись в этот pipe не нужна */
      close(pipefds[i][1]);
    }
  }

  /* ---- Ожидание завершения всех детей ---- */

  while (active_child_processes > 0) {
    wait(NULL);
    active_child_processes -= 1;
  }

  /* ---- Сбор результатов ---- */

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  if (with_files) {
    if (lseek(fd, 0, SEEK_SET) == -1) {
      perror("lseek");
      free(array);
      return 1;
    }
  }

  for (int i = 0; i < pnum; i++) {
    int buf[2];

    if (with_files) {
      if (read(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        perror("read from file");
        free(array);
        return 1;
      }
    } else {
      if (read(pipefds[i][0], buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        perror("read from pipe");
        free(array);
        return 1;
      }
      close(pipefds[i][0]);
    }

    if (buf[0] < min_max.min) min_max.min = buf[0];
    if (buf[1] > min_max.max) min_max.max = buf[1];
  }

  /* ---- Очистка ---- */

  if (with_files) {
    close(fd);
    remove("min_max.txt");
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}