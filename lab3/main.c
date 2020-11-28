#include "stdio.h"
#include "stdlib.h"
// #include "time.h"
#include "pthread.h"

int score1, score2, rounds;

void* experiment(void *arg) {
	int *ex_count = (int *)arg;
	int sc1;
	int sc2;
	int *win = (int *)malloc(sizeof(int) * 2);
	win[0] = 0; win[1] = 0;
	for (int j = 0; j < *ex_count; j++) {
		sc1 = score1;
		sc2 = score2;
		for (int i = 0; i < rounds; ++i) {
			sc1 += rand() % 6 + 1 + rand() % 6 + 1;
			sc2 += rand() % 6 + 1 + rand() % 6 + 1;
		}
		if (sc1 > sc2) {
			win[0]++;
		} else if (sc2 > sc1) {
			win[1]++;
		}
	}
	free(ex_count);
	pthread_exit((void *)win);
	return NULL;
}

int main(int argc, char const *argv[]) {
	int th_count = atoi(argv[1]);
	if (th_count <= 0) {
		perror("invalid argument");
		return 1;
	}

	srand(time(NULL));
	// srand(3456);

	int k, cur, count;
	scanf("%d %d %d %d %d", &k, &cur, &score1, &score2, &count);
	rounds = k - cur + 1;

	if (count < th_count) {
		th_count = count;
	}

	pthread_t *id = (pthread_t *)malloc(sizeof(pthread_t) * th_count);

	int first = 0, second = 0;
	int *win = NULL;
	int *ex_count = NULL;

	// clock_t start = clock();

	for (int i = 0; i < th_count-1; ++i) {
		ex_count = (int *)malloc(sizeof(int));
		*ex_count = count / th_count;
		if (pthread_create(&id[i], NULL, experiment, (void *)ex_count) != 0) {
			perror("pthread create error\n");
			return 1;
		}
	}
	ex_count = (int *)malloc(sizeof(int));
	*ex_count = count / th_count + count % th_count;
	if (pthread_create(&id[th_count - 1], NULL, experiment, (void *)ex_count) != 0) {
			perror("pthread create error\n");
			return 1;
	}

	
	for (int i = 0; i < th_count; ++i) {
		pthread_join(id[i], (void *)&win);
		first += win[0];
		second += win[1];
		free(win);
	}

	// clock_t end = clock();

	printf("%.2lf %.2lf\n", (double)first / count, (double)second / count);
	// printf("%d threads: %li\n",th_count, end - start);
	// printf("%d %d\n", first, second);

	free(id);
	return 0;
}