#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>


int main(int argc, char** argv) {
	FILE *fp;
	size_t ret;
  size_t buf_len;

	openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

	if (argc != 3) {
		fprintf(stderr, "Please enter below to use\n Usage: writer [WRITEDIR] [WRITESTR)]\n");
	  syslog(LOG_ERR, "Invaliation arguments error");
	  closelog();	
		exit(EXIT_FAILURE);
  }	

 	fp = fopen(argv[1], "w+"); 
	if (fp == NULL) {
		perror("fopen");
	  syslog(LOG_ERR, "fopen()");
	  closelog();	
		exit(EXIT_FAILURE);
	}

	buf_len = strlen(argv[2]);
  ret = fwrite(argv[2], sizeof(char), buf_len, fp);
	if (ret != buf_len) {
		fprintf(stderr, "fwrite(): Error:%s \n", strerror(errno));
	  syslog(LOG_ERR, "fwrite() Error");
	  fclose(fp);
	  closelog();	
	  exit(EXIT_FAILURE);	
	}
	syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);

	closelog();	
	fclose(fp);
  

	exit(EXIT_SUCCESS);
}
