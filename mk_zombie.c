#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(){
	pid_t f = fork();
	if (f > 0) {
		printf("Child pid: %d\n", f);
		return 0;
	} else if (f == 0) {
		printf("Child starts sleeping...\n");
		sleep(5);
		printf("Child done sleeping.\n");
		return 0;
	}
	else {
		printf("Error forking!\n");
	}
	return 0;
}
