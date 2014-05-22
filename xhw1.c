/******************************************************************************
File name       :       xhw1.c
Author          :       Raghavendra Suvvari
Description     :       This is an user defined file from where we call the
			concat system call by passing the appropriate flags,
			output file, input files, flags.
Date            :       1/29/2014
Version         :       1.0
******************************************************************************/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include "header.h"

#define __NR_xconcat    349     /* our private syscall number */
#define __user

/*****************************************************************
* @main	-	Entry point of the program
* @argc:    Number of arguments entered in the command line
* @argv:    Pointer array which contains the command line arguments
******************************************************************/

int main(int argc, char *argv[])
{
	int rc = 0;
	int c = 0;
	int i = 0, j = 0;

	int oflag = O_RDWR;
	mode_t mode = 0777;

	int flag =  0;
	char *string = malloc(sizeof(char *));
	char *isMode = NULL;

	struct myargs args;

	/* getopt reads each flag from the given string*/

	while ((c = getopt(argc, argv, "acteANPm:h")) != -1)
		switch (c) {
		case 'a':
		oflag = oflag | O_APPEND;
		break;

		case 'c':
		oflag = oflag | O_CREAT;
		break;

		case 't':
		oflag = oflag | O_TRUNC;
		break;

		case 'e':
		oflag = oflag | O_EXCL;
		break;

		case 'A':
		flag = flag | 0x04;
		break;

		case 'N':
		flag = flag | 0x01;
		break;

		case 'P':
		flag = flag + 0x02;
		break;

		case 'm':
		mode = strtol(optarg, NULL, 8);
		isMode = optarg;
		break;

		case 'h':
		fprintf(stderr, "Usage: %s [- actemh] [-ANP] name\n", argv[0]);
		exit(EXIT_FAILURE);

		case '?':
		fprintf(stderr, "Usage: %s [- actemh] [-ANP] name\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (oflag == O_RDWR)
		oflag = O_RDWR | O_APPEND;

	args.oflags = oflag;
	args.mode = mode;
	args.flags = flag;
	args.outfile = malloc(sizeof(char));
	args.outfile = argv[optind];
	args.infile_count = argc - optind - 1;
	args.infiles = malloc((argc-optind-1) * sizeof(char *));

	for (i = optind+1, j = 0; i < argc ; i++, j++) {
		args.infiles[j] = malloc(sizeof(argv[i]) * sizeof(char));
		args.infiles[j] = argv[i];
	}

	if (isMode != NULL) {
		sprintf(string, "%o", mode);
		if (strcmp(string, isMode) != 0) {
			printf("\n Error: Entered mode is invalid\n");
			free(string);
			exit(0);
		}
	}
	free(string);
	void *dummy = (void *)&args;
	rc = syscall(__NR_xconcat, dummy, sizeof(struct myargs));

	if (rc >= 0)
		printf("%d\n", rc);

	/* Print the appropriate error */
	else
		printf("\nErrorno: %d\t %s\n", errno, strerror(errno));

	exit(rc);
}

