#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s seed arraysize\n", argv[0]);
    return 1;
  }

  pid_t pid = fork();

  if (pid == -1) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    /* ===== Дочерний процесс ===== */
    char *args[] = {"./sequential_min_max", argv[1], argv[2], NULL};

    execv(args[0], args);

    /* Если execv вернулся — значит, запуск провалился */
    perror("execv");
    exit(1);
  }

  /* ===== Родитель ===== */
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
    return 1;
  }

  if (WIFEXITED(status)) {
    printf("Child exited with code %d\n", WEXITSTATUS(status));
  }

  return 0;
}