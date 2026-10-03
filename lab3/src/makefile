CC=gcc
CFLAGS=-I.

all: sequential_min_max parallel_min_max run_sequential
sequential_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o sequential_min_max find_min_max.o utils.o sequential_min_max.c $(CFLAGS) 
#собирает последовательную программу из объектных файлов

parallel_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o parallel_min_max utils.o find_min_max.o parallel_min_max.c $(CFLAGS)
#собирает параллельную программу

run_sequential: utils.o find_min_max.o run_sequential.c
	$(CC) -o run_sequential utils.o find_min_max.o run_sequential.c $(CFLAGS)


utils.o : utils.h
	$(CC) -o utils.o -c utils.c $(CFLAGS)
#компилирует utils.c в объектный файл
find_min_max.o : utils.h find_min_max.h
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)
#компилирует find_min_max.c
clean :
	rm utils.o find_min_max.o sequential_min_max parallel_min_max
#удаляет объектные файлы и бинарники