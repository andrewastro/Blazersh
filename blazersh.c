#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAXCHAR 1000    //max number of supported characters in command
#define MAXTOK 100      //max number of tokens in command
#define MAXPIPES 50     //max number of pipes
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

//count number of pipes and record indices of pipe tokens
int detectPipes(char** tokenizedCommand, int* pipeIndices) {
    char* pipeToken = "|";
    int numPipes = 0;
    int i, j = 0;
    for(i = 0; i < (MAXTOK-1); i++) {
        if (tokenizedCommand[i] != NULL) {
            if (strcmp(tokenizedCommand[i], pipeToken) == 0) {
                numPipes++;
                pipeIndices[j] = i;
                j++;
            }
            if (tokenizedCommand[i+1] == NULL) {
                pipeIndices[j] = i+1;
            }
        }
    }
    return numPipes;
}

//parse each piped command
void findPipedCommands(int numPipes, int* pipeIndices, char* pipedCommands[MAXPIPES+1][MAXTOK], char** tokenizedCommand) {
    char* pipeToken = "|";
    int i, j, k = 0;

    //reset
    for (i = 0; i < (MAXPIPES+1); i++) {
        for (j = 0; j < MAXTOK; j++) {
            pipedCommands[i][j] = NULL;
        }
    }

    //there is always one more command than pipes
    j = 0;
    for (i = 0; i < (numPipes+1); i++) {
        for (; j < pipeIndices[i]; j++) {
            pipedCommands[i][k] = tokenizedCommand[j];
            k++;
        }
        j++;  //increment j to skip pipe token
        k = 0;
    }
}

//detect commands that need to execute in the background
//returns 1 if background process, 0 otherwise
int detectBackgroundProc(char** command) {
    char* backgroundToken = "&";
    int index = 0;

    //look at last token in command
    while ((command[index] != NULL) && (index < MAXTOK)) { index++; }
    if (strcmp(command[index-1], backgroundToken) == 0) {
        command[index-1] = NULL;
        return 1;
    } else {
        return 0;
    }
}

// executes command
// first looks for internal commands
// pipeRW: 0 for no piping, 1 for write to pipe, 2 for read from pipe, 3 for read and write to pipes
// pipefd1: write pipe
// pipefd2: read pipe
int* executeCommand(char** tokenizedCommand, char** filenames, int pipeRW, int pipefd2[2], int backgroundFlag) {
    static int pipefd1[2];
    char* internalCommands[6];
    int internalCommand = 0, i = 0, pid, status, pgid, child = 0, fdin, fdout, isInputRed = 0, isOutputRed = 0;
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
	    return pipefd1;
	} else {
	    isInputRed = 1;
	}
    }
    
    //open output file
    if (filenames[1] != NULL) {
	if ((fdout = open(filenames[1], O_CREAT | O_WRONLY | O_TRUNC, 0666)) == -1) {
	     printf("error opening file %s for output\n\r", filenames[1]);
	     return pipefd1;
	} else {
	    isOutputRed = 1;
	}
    }

    //open write pipe (pipefd1)
    if ((pipeRW == 1) || (pipeRW == 3)) {
        if (pipe(pipefd1) != 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    switch(internalCommand) {
        case 1 :    //environ
            pid = fork();
	    if (pid == 0) {
                if (backgroundFlag == 1) { child = setpgid(0, 0); }
		if (isInputRed == 1) { dup2(fdin, 0); }
		if (isOutputRed == 1) { dup2(fdout, 1); }
                if (pipeRW == 1) {    //writing to a pipe only
                    //close pipefd1[0] (read end)
                    close(pipefd1[0]);
                    //replace stdout with write end of pipefd1
                    if (dup2(pipefd1[1], 1) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd1[1]);
                } else if (pipeRW == 2) {   //reading from a pipe only
                    //close pipefd2[1] (write end)
                    close(pipefd2[1]);
                    //replace stdin with read end of pipefd2
                    if (dup2(pipefd2[0], 0) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd2[0]);
                } else if (pipeRW == 3) {  //reading and writing to pipe
                    //close read end of write pipe (pipefd1)
                    close(pipefd1[0]);
                    //close write end of read pipe (pipefd2)
                    close(pipefd2[1]);
                    //replace stdin and stdout
                    if (dup2(pipefd1[1], 1) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd1[1]);
                    if (dup2(pipefd2[0], 0) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd2[0]);
                }
	        execvp("printenv", tokenizedCommand);
		exit(0);
	    } else if (pid > 0) {
                //close read pipe (pipefd2)
                if((pipeRW == 2) || (pipeRW == 3)) {
                    close(pipefd2[0]);
                    close(pipefd2[1]);
                } 
	        if (backgroundFlag == 1) {
                    waitpid(child, &status, WNOHANG);
                } else {
                    waitpid(0, &status, 0);
                }
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
                if (backgroundFlag == 1) { child = setpgid(0, 0); }
                if (isInputRed == 1) { dup2(fdin, 0); }
		if (isOutputRed == 1) { dup2(fdout, 1); }
                if (pipeRW == 1) {    //writing to a pipe only
                    //close pipefd1[0] (read end)
                    close(pipefd1[0]);
                    //replace stdout with write end of pipefd1
                    if (dup2(pipefd1[1], 1) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd1[1]);
                } else if (pipeRW == 2) {   //reading from a pipe only
                    //close pipefd2[1] (write end)
                    close(pipefd2[1]);
                    //replace stdin with read end of pipefd2
                    if (dup2(pipefd2[0], 0) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd2[0]);
                } else if (pipeRW == 3) {  //reading and writing to pipe
                    //close read end of write pipe (pipefd1)
                    close(pipefd1[0]);
                    //close write end of read pipe (pipefd2)
                    close(pipefd2[1]);
                    //replace stdin and stdout
                    if (dup2(pipefd1[1], 1) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd1[1]);
                    if (dup2(pipefd2[0], 0) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd2[0]);
                }
		execvp("ls", tokenizedCommand);
		printf("list failed\n\r");
		exit(0);
	    } else if (pid > 0) {
                //close read pipe (pipefd2)
                if((pipeRW == 2) || (pipeRW == 3)) {
                    close(pipefd2[0]);
                    close(pipefd2[1]);
                }
	        if (backgroundFlag == 1) {
                    waitpid(child, &status, WNOHANG);
                } else {
                    waitpid(0, &status, 0);
                }
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
            //close read pipe (pipefd2)
            if((pipeRW == 2) || (pipeRW == 3)) {
                close(pipefd2[0]);
                close(pipefd2[1]);
            }
            break;

        case 6 :    //quit
            exit(0);

        default :   //not an internal command
            pid = fork();
	    if (pid == 0) {
                if (backgroundFlag == 1) { child = setpgid(0, 0); }
                if (isInputRed == 1) { dup2(fdin, 0); }
                if (isOutputRed == 1) { dup2(fdout, 1); }
                if (pipeRW == 1) {    //writing to a pipe only
                    //close pipefd1[0] (read end)
                    close(pipefd1[0]);
                    //replace stdout with write end of pipefd1
                    if (dup2(pipefd1[1], 1) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd1[1]);
                } else if (pipeRW == 2) {   //reading from a pipe only
                    //close pipefd2[1] (write end)
                    close(pipefd2[1]);
                    //replace stdin with read end of pipefd2
                    if (dup2(pipefd2[0], 0) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd2[0]);
                } else if (pipeRW == 3) {  //reading and writing to pipe
                    //close read end of write pipe (pipefd1)
                    close(pipefd1[0]);
                    //close write end of read pipe (pipefd2)
                    close(pipefd2[1]);
                    //replace stdin and stdout
                    if (dup2(pipefd1[1], 1) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd1[1]);
                    if (dup2(pipefd2[0], 0) == -1) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                    close(pipefd2[0]);
                }
	        execvp(tokenizedCommand[0], tokenizedCommand);
		printf("invalid command\n\r");
		exit(0);
	    } else if (pid > 0) {
                //close read pipe
                if((pipeRW == 2) || (pipeRW == 3)) {
                    close(pipefd2[0]);
                    close(pipefd2[1]);
                }
	        if (backgroundFlag == 1) {
                    waitpid(child, &status, WNOHANG);
                } else {
                    waitpid(0, &status, 0);
                }
		closeIO(isInputRed, isOutputRed, fdin, fdout);
	    } else {
	    }
	    break;
    }

    //write pipe becomes next command's read pipe
    return pipefd1;
}

int main() {
    char input[MAXCHAR];                      //raw user input
    char* tokenizedCommand[MAXTOK];           //tokenized input delimited by whitespace
    char* filenames[2];                       //IO redirect filenames
    int pipeIndices[MAXPIPES];                //indices of the pipe tokens
    int numPipes = 0;                         //number of pipes
    char* pipedCommands[MAXPIPES+1][MAXTOK];  //array of each piped command
    int pipefdRead[2];                        //read pipe addresses
    
    init();
    //execute commands on an infinite loop
    while (1 == 1) {
        readCommand(input);
        tokenizeCommand(input, tokenizedCommand);
        detectIORedirect(tokenizedCommand, filenames);
        numPipes = detectPipes(tokenizedCommand, pipeIndices);
        findPipedCommands(numPipes, pipeIndices, pipedCommands, tokenizedCommand);
        
        //detect which processes need to execute in the background
        int i, backgroundFlag[MAXPIPES+1];
        for (i = 0; i < (numPipes+1); i++) {
            backgroundFlag[i] = detectBackgroundProc(pipedCommands[i]);
        }
        
        if (numPipes > 0) {
            i = 0;
            char* noFiles[2];
            int* returnfd;
            noFiles[0] = NULL;
            noFiles[1] = NULL;

            //execute first command which only writes to a pipe
            returnfd = executeCommand(pipedCommands[0], noFiles, 1, pipefdRead, backgroundFlag[0]);
            pipefdRead[0] = *returnfd;
            pipefdRead[1] = *(returnfd+1);
            
            //execute middle commands which read and write from pipes
            if (numPipes > 1) {
                for (i = 1; i < numPipes; i++) {
                    returnfd = executeCommand(pipedCommands[i], noFiles, 3, pipefdRead, backgroundFlag[i]);
                    pipefdRead[0] = *returnfd;
                    pipefdRead[1] = *(returnfd+1);
                }
            }
            
            //execute final piped command which only reads from a pipe and can have output redirection
            filenames[0] = NULL;
            executeCommand(pipedCommands[numPipes], filenames, 2, pipefdRead, backgroundFlag[numPipes]);

        } else {    //no piping, execute single command
            executeCommand(pipedCommands[0], filenames, 0, pipefdRead, backgroundFlag[0]);
        }
    }

    return 0;
}
