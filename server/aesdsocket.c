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

#define MYPORT "9000"
#define BACKLOG 10

static int sockfd;
static int new_fd;
static FILE *fp;

void sigchld_handler(int signal_number) {
		if (signal_number == SIGINT) {
			syslog(LOG_INFO, "Caught signal, exiting");
			close(sockfd);	
			close(new_fd);	
			fclose(fp);
			unlink("/var/tmp/aesdsocketdata");
			closelog();
			exit(EXIT_SUCCESS);
		} else if (signal_number == SIGTERM) {
			syslog(LOG_INFO, "Caught signal, exiting");
			close(sockfd);	
			close(new_fd);	
			fclose(fp);
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

int main(int argc, char *argv[]) {
	int rv;
	int yes = 1;
	struct sockaddr_storage their_addr;
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct addrinfo *p;
	struct sigaction sa;
	char s[INET6_ADDRSTRLEN];
	char text[102400];
	socklen_t sin_size;
	bool is_daemon_mode = false;
	char buff[102400] = {0};
	
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

	if (listen(sockfd, BACKLOG) == 1) {
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
	
		rv  = recv(new_fd, text, sizeof(text), 0);
		if (rv > 0) {
			printf("%s", text);
		  fseek(fp, 0, SEEK_END);
			fwrite(text, sizeof(char), rv, fp);
			fflush(fp);

			fseek(fp, 0, SEEK_SET);
			size_t res; 
		  res = fread(buff, sizeof(char), sizeof(buff), fp);
			if (res > 0) {
				send(new_fd, buff, res, 0);
			}
		}
		
		printf("Closed connection from %s\n", s);
		syslog(LOG_INFO, "Closed `connection from %s", s);
		close(new_fd);
	}

	fclose(fp);
	close(sockfd);
	closelog();
	exit(EXIT_SUCCESS);
}
