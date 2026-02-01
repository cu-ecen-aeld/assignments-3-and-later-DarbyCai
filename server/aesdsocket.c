#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

#include "queue.h"

#define MYPORT "9000"
#define BACKLOG 10

static int sockfd;
static int new_fd;
static pthread_mutex_t mutex;
static FILE *fp;
static SLIST_HEAD(slisthead, slist_data_s) head;

struct thread_data {
	int fd;
  pthread_t thread;
	bool thread_complete_success;	
};

typedef struct slist_data_s slist_data_t;
struct slist_data_s {
	struct thread_data *data;
	SLIST_ENTRY(slist_data_s) entries;
};

void *threadfunc(void *thread_param) {
	struct thread_data *thread_func_args = (struct thread_data *)thread_param;
	char buff[102400] = {0};
	char text[102400];
	
	int rc = pthread_mutex_lock(&mutex);
	if (rc != 0) {
		fprintf(stderr, "pthread_mutex_lock failed with %d\n", rc);		
	} else {
		size_t rv  = recv(thread_func_args->fd, text, sizeof(text), 0);
		if (rv > 0) {
			printf("%s", text);
			fseek(fp, 0, SEEK_END);
			fwrite(text, sizeof(char), rv, fp);
			fflush(fp);

			fseek(fp, 0, SEEK_SET);
			size_t res; 
			res = fread(buff, sizeof(char), sizeof(buff), fp);
			if (res > 0) {
				send(thread_func_args->fd, buff, res, 0);
			}
		}
		rc = pthread_mutex_unlock(&mutex);
		thread_func_args->thread_complete_success = true;
		close(thread_func_args->fd);
		//free(thread_func_args);
	}
	return thread_param;
}

void sigchld_handler(int signal_number) {
		if (signal_number == SIGINT || signal_number == SIGTERM) {
			syslog(LOG_INFO, "Caught signal, exiting");
		
			slist_data_t *datap;
			while (!SLIST_EMPTY(&head)) {
				datap = SLIST_FIRST(&head);
				pthread_join(datap->data->thread, NULL);
				SLIST_REMOVE_HEAD(&head, entries);
				free(datap->data);
				free(datap);
			}

			pthread_mutex_destroy(&mutex);

			if (sockfd >= 0) {
				close(sockfd);	
			}
			if (fp != NULL) {
				fclose(fp);
			}
			unlink("/var/tmp/aesdsocketdata");
			closelog();
			exit(EXIT_SUCCESS);
		}
}

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void callback(int signum) {
		char text[256];
		char buff[512];
		time_t t;
		struct tm *tmp;
		t = time(NULL);
		tmp = localtime(&t);
		if (tmp == NULL) {
			perror("localtime");
		}	

		if (strftime(text, sizeof(text), "%a, %d %b %Y %T %z", tmp) == 0) {
			fprintf(stderr, "strftime returned 0");
		}
		pthread_mutex_lock(&mutex);
		sprintf(buff, "timestamp:%s\n", text);
		puts(buff);
	  fseek(fp, 0, SEEK_END);
		fwrite(buff, sizeof(char), strlen(buff), fp);
		fflush(fp);
		pthread_mutex_unlock(&mutex);
}

int main(int argc, char *argv[]) {
	int rv;
	int yes = 1;
	struct sockaddr_storage their_addr;
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct addrinfo *p;
	struct sigaction sa;
	char s[INET6_ADDRSTRLEN];
	socklen_t sin_size;
	bool is_daemon_mode = false;
	
	if (argc > 1 && strcmp(argv[1], "-d") == 0) {
		is_daemon_mode = true;	
	}


	if (is_daemon_mode) {
		if (daemon(0, 0) != 0) {
			perror("daemon");
			exit(EXIT_FAILURE);
		}
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(EXIT_FAILURE);
	}	
	
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(EXIT_FAILURE);
		}
		
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}
		
	freeaddrinfo(servinfo);
		
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(EXIT_FAILURE);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction for SIGINT");
		exit(EXIT_FAILURE);
	}	
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		perror("sigaction for SIGTERM");
		exit(EXIT_FAILURE);
	}	
	
	printf("server: waiting for connections...\n");

	openlog("aesd_", LOG_PID | LOG_CONS, LOG_USER);

	fp = fopen("/var/tmp/aesdsocketdata", "a+");
	if (fp == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	struct itimerval timer;
	signal(SIGALRM, callback);
	
	timer.it_value.tv_sec = 10;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 10;
	timer.it_interval.tv_usec = 0;
	
	setitimer(ITIMER_REAL, &timer, NULL);
	pthread_mutex_init(&mutex, NULL);

	slist_data_t *datap = NULL;
	slist_data_t *datap_temp = NULL;
	SLIST_INIT(&head);

	while(true) {
		sin_size = sizeof(their_addr);
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s));
		printf("got connection from %s\n", s);
		syslog(LOG_INFO, "accept connection from %s", s);

		datap = malloc(sizeof(slist_data_t));
		struct thread_data *my_data = malloc(sizeof(struct thread_data));
		datap->data = my_data;
		my_data->thread_complete_success = false;
		my_data->fd = new_fd;

		SLIST_INSERT_HEAD(&head, datap, entries);

		int rc = pthread_create(&my_data->thread, NULL, threadfunc, (void*) my_data);
		if (rc != 0) {
			fprintf(stderr, "pthread_create failed with %d\n", rc);
			exit(EXIT_FAILURE);
		}

		SLIST_FOREACH_SAFE(datap, &head, entries, datap_temp) {
			if (datap->data->thread_complete_success) {
				pthread_join(datap->data->thread, NULL);
				SLIST_REMOVE(&head, datap, slist_data_s, entries);
				free(datap->data);
				free(datap);
			}
		}

		printf("Closed connection from %s\n", s);
		syslog(LOG_INFO, "Closed `connection from %s", s);
	}

	pthread_mutex_destroy(&mutex);
	fclose(fp);
	close(sockfd);
	closelog();
	exit(EXIT_SUCCESS);
}
