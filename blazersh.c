#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXCHAR 1000    //max number of supported characters in command
#define MAXTOK 100      //max number of tokens in command
#define MAXPATH 256     //max number of characters in path

//display welcome message
void init() {
    printf("\n*********************************\n\n\r");
    printf("welcome to blazersh simple shell!\n\n\r");
    printf("*********************************\n\n\r");
}

//read command from command line
void readCommand(char* input) {
    printf("blazersh>");
    char buffer[MAXCHAR];
    fgets(buffer, sizeof(buffer), stdin);
    // get rid of newline character
    buffer[strlen(buffer) - 1] = '\0';
    if (strlen(buffer) < 1) {
        // if no input, prompt user again
        readCommand(input);
    } else {
        strcpy(input, buffer);
    }
}

//tokenize command into individual tokens
void tokenizeCommand(char* input, char** tokenizedCommand) {
    char *token;
    int i;
    for(i = 0; i < MAXTOK; i++) {
        tokenizedCommand[i] = NULL;
    }
    i = 0;
    token = strtok(input, " ");
    while (token != NULL && i < MAXTOK) {
        tokenizedCommand[i] = token;
        token = strtok(NULL, " ");
        i++;
    }
}

// executes command
// first looks for internal commands
void executeCommand(char** tokenizedCommand) {
    char* internalCommands[6];
    int internalCommand = 0;
    int i = 0;
    int pid = 0;
    char cwd[MAXPATH];
    char arg[MAXPATH+4];

    internalCommands[0] = "environ";
    internalCommands[1] = "set";
    internalCommands[2] = "list";
    internalCommands[3] = "cd";
    internalCommands[4] = "help";
    internalCommands[5] = "quit";
    
    for (i = 0; i < 6; i++) {
        if (strcmp(tokenizedCommand[0], internalCommands[i]) == 0) {
            internalCommand = i + 1;
	    break;
        }
    }

    switch(internalCommand) {
        case 1 :    //environ
            pid = fork();
	    if (pid == 0) {
	        execvp("printenv", tokenizedCommand);
		exit(0);
	    } else if (pid > 0) {
	        wait(NULL);
	    } else {
            }
	    break;
        case 2 :    //set
            if (tokenizedCommand[2] == NULL) {
	        printf("two arguments expected");
	    } else {
	        strcpy(arg, tokenizedCommand[1]);
		strcat(arg, "=");
		strcat(arg, tokenizedCommand[2]);
                if(putenv(arg) != 0) {
		    printf("set failed");
		}
	    }
            break;
        case 3 :    //list
            pid = fork();
	    if (pid == 0) {
	        execvp("ls", tokenizedCommand);
		printf("list failed\n\r");
		exit(0);
	    } else if (pid > 0) {
                wait(NULL);
            } else {
	    }
	    break;
        case 4 :    //cd
	    if (tokenizedCommand[1] == NULL) {
                printf("argument expected\n\r");
	    } else {
                if(chdir(tokenizedCommand[1]) != 0) {
	            printf("cd failed: check path\n\r");
		} else {
	            strcpy(arg, "PWD=");
		    strcat(arg, getcwd(cwd, sizeof(cwd)));
	            putenv(arg);
		}
	    }
	    break;
        case 5 :    //help
            printf("environ: list all environment strings as name=value\n\r");
	    printf("set <NAME> <VALUE>: set the environment variable <NAME> the value specified by <VALUE> and add to environment strings if this is a new variable\n\r");
	    printf("list: list all the files in the current directory\n\r");
	    printf("cd <directory>: change the currect directory to <directory>\n\r");
	    printf("help: list the internal commands and how to use them\n\r");
	    printf("quit: quit blazersh shell\n\r");
            break;
        case 6 :    //quit
            exit(0);
        default :   //not an internal command
            pid = fork();
	    if (pid == 0) {
	        execvp(tokenizedCommand[0], tokenizedCommand);
		printf("invalid command\n\r");
		exit(0);
	    } else if (pid > 0) {
	        wait(NULL);
	    } else {
	    }
	    break;
    }
}

int main() {
    char input[MAXCHAR];
    char* tokenizedCommand[MAXTOK];
    init();
    while (1 == 1) {
        readCommand(input);
        tokenizeCommand(input, tokenizedCommand);
        executeCommand(tokenizedCommand);
    }
    return 0;
}
