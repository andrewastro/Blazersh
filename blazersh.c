#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

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
//tokens delimited by whitespace
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

// detect IO redirection symbols < and > (which had been delimited by whitespace)
// determine file names
// truncates tokenizedCommand after arguments,  before IO redirect
void detectIORedirect(char** tokenizedCommand, char** filenames) {
    filenames[0] = NULL;
    filenames[1] = NULL;
    int i, indexIn = MAXTOK+1, indexOut = MAXTOK+1;

    //look for < or > symbols and record filenames
    for(i = 1; i < (MAXTOK-1); i++) {
        if (tokenizedCommand[i] != NULL) {
            if (strcmp(tokenizedCommand[i], "<") == 0) {
                filenames[0] = tokenizedCommand[i+1];
                indexIn = i;
                i++; //don't test next token, this is file name
            } else if (strcmp(tokenizedCommand[i], ">") == 0) {
                filenames[1] = tokenizedCommand[i+1];
                indexOut = i;
                i++;
            }
        }
    }

    //truncate tokenizedCommand if IO redirect found
    if (indexIn < MAXTOK) {                                //if input regardless of output
        for(i = indexIn; i < MAXTOK; i++) {
            tokenizedCommand[i] = NULL;
        }
    } else if (indexIn > MAXTOK && indexOut < MAXTOK) {    //if output but no input
        for(i = indexOut; i < MAXTOK; i++) {
            tokenizedCommand[i] = NULL;
        }
    }
}

//close files used for IO redirection
void closeIO(int isInputRed, int isOutputRed, int fdin, int fdout) {
    if (isInputRed == 1) {
        close(fdin);
    }
    if (isOutputRed == 1) {
        close(fdout);
    }
}

// executes command
// first looks for internal commands
void executeCommand(char** tokenizedCommand, char** filenames) {
    char* internalCommands[6];
    int internalCommand, i, pid, fdin, fdout, isInputRed = 0, isOutputRed = 0;
    char cwd[MAXPATH], arg[MAXPATH+4];

    internalCommands[0] = "environ";
    internalCommands[1] = "set";
    internalCommands[2] = "list";
    internalCommands[3] = "cd";
    internalCommands[4] = "help";
    internalCommands[5] = "quit";
    
    //look for internal commands
    for (i = 0; i < 6; i++) {
        if (strcmp(tokenizedCommand[0], internalCommands[i]) == 0) {
            internalCommand = i + 1;
	    break;
        }
    }

    //open input file
    if (filenames[0] != NULL) {
        if ((fdin = open(filenames[0], O_RDONLY, 0666)) == -1) {
            printf("error opening file %s for input\n\r", filenames[0]);
	    return;
	} else {
	    isInputRed = 1;
	}
    }
    //open output file
    if (filenames[1] != NULL) {
	if ((fdout = open(filenames[1], O_CREAT | O_WRONLY | O_TRUNC, 0666)) == -1) {
	     printf("error opening file %s for output\n\r", filenames[1]);
	     return;
	} else {
	    isOutputRed = 1;
	}
    }

    switch(internalCommand) {
        case 1 :    //environ
            pid = fork();
	    if (pid == 0) {
		if (isInputRed == 1) { dup2(fdin, 0); }
		if (isOutputRed == 1) { dup2(fdout, 1); }
	        execvp("printenv", tokenizedCommand);
		exit(0);
	    } else if (pid > 0) {
	        wait(NULL);
		closeIO(isInputRed, isOutputRed, fdin, fdout);
	    } else {
            }
	    break;
        case 2 :    //set
            if (tokenizedCommand[2] == NULL) {
	        printf("two arguments expected");
	    } else {
		if (isInputRed == 1) { dup2(fdin, 0); }
		if (isOutputRed == 1) { dup2(fdout, 1); }
	        strcpy(arg, tokenizedCommand[1]);
		strcat(arg, "=");
		strcat(arg, tokenizedCommand[2]);
                if(putenv(arg) != 0) {
		    printf("set failed");
		}
		closeIO(isInputRed, isOutputRed, fdin, fdout);
	    }
            break;
        case 3 :    //list
            pid = fork();
	    if (pid == 0) {
                if (isInputRed == 1) { dup2(fdin, 0); }
		if (isOutputRed == 1) { dup2(fdout, 1); }
		execvp("ls", tokenizedCommand);
		printf("list failed\n\r");
		exit(0);
	    } else if (pid > 0) {
                wait(NULL);
		closeIO(isInputRed, isOutputRed, fdin, fdout);
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
                    if (isInputRed == 1) { dup2(fdin, 0); }
                    if (isOutputRed == 1) { dup2(fdout, 1); }
	            strcpy(arg, "PWD=");
		    strcat(arg, getcwd(cwd, sizeof(cwd)));
	            putenv(arg);
		    closeIO(isInputRed, isOutputRed, fdin, fdout);
		}
	    }
	    break;
        case 5 :    //help
            if (isInputRed == 1) { dup2(fdin, 0); }
            if (isOutputRed == 1) { dup2(fdout, 1); }
            printf("environ: list all environment strings as name=value\n\r");
	    printf("set <NAME> <VALUE>: set the environment variable <NAME> the value specified by <VALUE> and add to environment strings if this is a new variable\n\r");
	    printf("list: list all the files in the current directory\n\r");
	    printf("cd <directory>: change the currect directory to <directory>\n\r");
	    printf("help: list the internal commands and how to use them\n\r");
	    printf("quit: quit blazersh shell\n\r");
	    closeIO(isInputRed, isOutputRed, fdin, fdout);
            break;
        case 6 :    //quit
            exit(0);
        default :   //not an internal command
            pid = fork();
	    if (pid == 0) {
                if (isInputRed == 1) { dup2(fdin, 0); }
                if (isOutputRed == 1) { dup2(fdout, 1); }
	        execvp(tokenizedCommand[0], tokenizedCommand);
		printf("invalid command\n\r");
		exit(0);
	    } else if (pid > 0) {
	        wait(NULL);
		closeIO(isInputRed, isOutputRed, fdin, fdout);
	    } else {
	    }
	    break;
    }
}

int main() {
    char input[MAXCHAR];             //raw user input
    char* tokenizedCommand[MAXTOK];  //tokenized input delimited by whitespace
    char* filenames[2];              //IO redirect filenames
    init();
    while (1 == 1) {
        readCommand(input);
        tokenizeCommand(input, tokenizedCommand);
        detectIORedirect(tokenizedCommand, filenames);
        executeCommand(tokenizedCommand, filenames);
    }
    return 0;
}
