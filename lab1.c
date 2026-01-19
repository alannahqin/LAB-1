
#define _POSIX_C_SOURCE 200809L //Needed for getline() and strtok_r()
 
#include <stdio.h> //input/output
#include <stdlib.h> //free(), exit()
#include <string.h> //strtok_r()
#include <errno.h> //perror()


int main(void) {

	//buffer for user input
	char *buffer = NULL;

	//current size of the buffer
	size_t bufsize = 0;
	
	// # of characters read
	ssize_t chars_read;

	//loop to keep reading until EOF
	while (1) {
		printf("Please enter some text: ");
		chars_read = getline(&buffer, &bufsize, stdin);

		//check if getline failed or reached EOF
		if (chars_read == -1) {
			if (feof(stdin)) {
				break; 
			}
			perror("getline");
			free(buffer);
			exit(EXIT_FAILURE);
		}
		printf("Tokens:\n");

		//keeps track of position
		char *saveptr = NULL;

		//first token
		char *token = strtok_r(buffer, " ", &saveptr);

		//any remaining tokens
		while (token != NULL) {
			printf(" %s\n", token);
			token = strtok_r(NULL, " ", &saveptr);
		}
	}

	//clean up memory
	free(buffer);

	return 0;
}
