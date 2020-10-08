#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "pthread.h"

// принимаем указатель на данные, возвращаем указатель на результат

struct data {
	int count;
	int score1;
	int score2;
};

void* experiment(void *arg) {
	struct data *data = (struct data *)arg;
	int sc1 = data->score1;
	int sc2 = data->score2;
	for (int i = 0; i < data->count; ++i) {
		sc1 += rand() % 6 + 1 + rand() % 6 + 1;
		sc2 += rand() % 6 + 1 + rand() % 6 + 1;
	}
	int *win = (int *)malloc(sizeof(int));
	if (sc1 > sc2) {
		*win = 1;
	} else if (sc2 > sc1) {
		*win = 2;
	}
	pthread_exit((void *)win);
	return NULL;
}

int main(int argc, char const *argv[]) {
	// srand(time(NULL));
	srand(3456);

	struct data data; 
	int k, cur, count;
	scanf("%d %d %d %d %d", &k, &cur, &data.score1, &data.score2, &count);
	data.count = k - cur + 1;

	pthread_t *id;
	id = (pthread_t *)malloc(sizeof(pthread_t) * count);

	for (int i = 0; i < count; ++i) {
		if (pthread_create(&id[i], NULL, experiment, (void *)&data) != 0) {
			perror("pthread create error\n");
			return 1;
		}
	}

	int first = 0, second = 0;
	int *win = NULL;
	for (int i = 0; i < count; ++i) {
		pthread_join(id[i], (void *)&win);
		if (*win == 1) {
			++first;
		} else if (*win == 2) {
			++second;
		}
		free(win);
	}
	printf("%.2lf %.2lf\n", (double)first / count, (double)second / count);
	// printf("%d %d\n", first, second);

	free(id);
	return 0;
}