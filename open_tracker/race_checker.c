#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define N 100

void opener_loop()
{
	for (int i = 0; i < N; i++)
		open("/dev/open_tracker", O_RDWR);
	sleep(2);
}

int main()
{
	int processes = 2, status = 0;
	pid_t fork_pid, wpid;

	for (int i = 0; i < processes; i++) {
		fork_pid = fork();
		if (fork_pid == 0) {
			opener_loop();
			exit(0);
		}
	}

	while ((wpid = wait(&status)) > 0)
		; // this way, the father waits for all the child processes

	return 0;
}
