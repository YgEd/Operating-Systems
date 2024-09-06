#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <pthread.h>
#include "scull.h"

#define CDEV_NAME "/dev/scull"

/* Quantum command line option */
static int g_quantum;

static void usage(const char *cmd)
{
	printf("Usage: %s <command>\n"
	       "Commands:\n"
	       "  R          Reset quantum\n"
	       "  S <int>    Set quantum\n"
	       "  T <int>    Tell quantum\n"
	       "  G          Get quantum\n"
	       "  Q          Query quantum\n"
	       "  X <int>    Exchange quantum\n"
	       "  H <int>    Shift quantum\n"
	       "  h          Print this message\n"
		   "  I          Prints information about the IOCTL\n",
	       cmd);
}

typedef int cmd_t;

static cmd_t parse_arguments(int argc, const char **argv)
{
	cmd_t cmd;

	if (argc < 2) {
		fprintf(stderr, "%s: Invalid number of arguments\n", argv[0]);
		cmd = -1;
		goto ret;
	}

	/* Parse command and optional int argument */
	cmd = argv[1][0];
	switch (cmd) {
	case 'S':
	case 'T':
	case 'H':
	case 'X':
		if (argc < 3) {
			fprintf(stderr, "%s: Missing quantum\n", argv[0]);
			cmd = -1;
			break;
		}
		g_quantum = atoi(argv[2]);
		break;
	case 'R':
	case 'G':
	case 'Q':
	case 'h':
	case 'i':
	case 'p':
	case 't':
		break;
	default:
		fprintf(stderr, "%s: Invalid command\n", argv[0]);
		cmd = -1;
	}

ret:
	if (cmd < 0 || cmd == 'h') {
		usage(argv[0]);
		exit((cmd == 'h')? EXIT_SUCCESS : EXIT_FAILURE);
	}
	return cmd;
}

void* threadWork(void* fd){
	struct task_info* t = (struct task_info*)malloc(sizeof(struct task_info));
	struct task_info* s = (struct task_info*)malloc(sizeof(struct task_info));
	ioctl(*((int*)fd), SCULL_IOCIQUANTUM, t);
	ioctl(*((int*)fd), SCULL_IOCIQUANTUM, s);
	printf("state %ld, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", t->state, t->cpu, t->prio, t->pid, t->tgid, t->nvcsw, t->nivcsw);
	printf("state %ld, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", s->state, s->cpu, s->prio, s->pid, s->tgid, s->nvcsw, s->nivcsw);
	

	free(s);
	free(t);
	return NULL;
}

static int do_op(int fd, cmd_t cmd)
{
	int ret, q;

	switch (cmd) {
	case 'R':
		ret = ioctl(fd, SCULL_IOCRESET);
		if (ret == 0)
			printf("Quantum reset\n");
		break;
	case 'Q':
		q = ioctl(fd, SCULL_IOCQQUANTUM);
		printf("Quantum: %d\n", q);
		ret = 0;
		break;
	case 'G':
		ret = ioctl(fd, SCULL_IOCGQUANTUM, &q);
		if (ret == 0)
			printf("Quantum: %d\n", q);
		break;
	case 'T':
		ret = ioctl(fd, SCULL_IOCTQUANTUM, g_quantum);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'S':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCSQUANTUM, &q);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'X':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCXQUANTUM, &q);
		if (ret == 0)
			printf("Quantum exchanged, old quantum: %d\n", q);
		break;
	case 'H':
		q = ioctl(fd, SCULL_IOCHQUANTUM, g_quantum);
		printf("Quantum shifted, old quantum: %d\n", q);
		ret = 0;
		break;
	case 'i': ;
		struct task_info* t = (struct task_info*)malloc(sizeof(struct task_info));
		ret = ioctl(fd, SCULL_IOCIQUANTUM, t);
		//making sure the information was properly passed from kernel to user
		if (ret == -1){
			printf("error copying from kernel to user space\n");
			return ret;
		}
		//printing all the fields
		printf("state %ld, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", t->state, t->cpu, t->prio, t->pid, t->tgid, t->nvcsw, t->nivcsw);
		free(t);
		break;
	case 'p':;
		for (int i=0; i<4; i++){
			//create four child processes that call SCULL_IOCIQUANTUM twice each
			if (fork()==0){
				struct task_info* t = (struct task_info*)malloc(sizeof(struct task_info));
				struct task_info* s = (struct task_info*)malloc(sizeof(struct task_info));
				ret = ioctl(fd, SCULL_IOCIQUANTUM, t);
				ret += ioctl(fd, SCULL_IOCIQUANTUM, s);
				printf("state %ld, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", t->state, t->cpu, t->prio, t->pid, t->tgid, t->nvcsw, t->nivcsw);
				printf("state %ld, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", s->state, s->cpu, s->prio, s->pid, s->tgid, s->nvcsw, s->nivcsw);
				if (ret != 0){
					printf("error copying from kernel to user space\n");
					return ret;
				}

				free(s);
				free(t);
				exit(0);
			
			}

		}

		//wait till all the children are finished
		while(wait(NULL) > 0);
		ret = 0;
		break;
	case 't': ;
		//have a array of threadIDs to create the 4 threads
		pthread_t tid[4];
		int* fdhold = &fd;
		for (int i=0; i<4; i++){
			pthread_create(&tid[i], NULL, threadWork, (void*)fdhold);
		}
		//join back the threads to the parent thread
		for (int i=0; i<4; i++){
			pthread_join(tid[i], NULL);
		}
		break;

	default:
		/* Should never occur */
		abort();
		ret = -1; /* Keep the compiler happy */
	}

	if (ret != 0)
		perror("ioctl");
	return ret;
}

int main(int argc, const char **argv)
{
	int fd, ret;
	cmd_t cmd;

	cmd = parse_arguments(argc, argv);

	fd = open(CDEV_NAME, O_RDONLY);
	if (fd < 0) {
		perror("cdev open");
		return EXIT_FAILURE;
	}

	printf("Device (%s) opened\n", CDEV_NAME);

	ret = do_op(fd, cmd);

	if (close(fd) != 0) {
		perror("cdev close");
		return EXIT_FAILURE;
	}

	printf("Device (%s) closed\n", CDEV_NAME);

	return (ret != 0)? EXIT_FAILURE : EXIT_SUCCESS;
}
