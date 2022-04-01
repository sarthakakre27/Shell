#include <stdio.h>
#include <string.h>			//string library
#include <stdlib.h>			// exit()
#include <unistd.h>			// fork(), getpid(), exec()
#include <sys/wait.h>		// wait()
#include <signal.h>			// signal()
#include <fcntl.h>			// close(), open()

#define SUCCESS_CODE 1
#define FAILURE_CODE 0
#define MAX_COMMANDS 4
#define STR_MAXLEN 400

typedef int STATUS_CODE;
typedef enum OpType{Redirect,Sequential,Parallel,ChangeDirectory,Exit,Execute} OpType;
int exitflag = 0;//exit in parallel process execution flag

void handle_sigint(int sig)//only child will execute this if not using exec
{
	if(sig == SIGINT)//ctrl c 
	{
		pid_t pid = getpid();
		kill(pid,SIGINT);//kill the child process by sending SIGINT signal to it
	}
	else if(sig == SIGTSTP)
	{
		pid_t pid = getpid();
		kill(pid,SIGTSTP);//stop the child process by sending SIGTSTP signal to it
	}
}

STATUS_CODE parseForDifferentSymbols(char** cmds, char* inputString, char* Symbol)
{
	STATUS_CODE ret_val = SUCCESS_CODE;
	int i = 0;
	int flag = 0;
	int limit = MAX_COMMANDS;
	
	//redirection only for two streams
	if(strcmp(Symbol,">") == 0)
	{
		limit = 2;
	}
	while(i < limit && flag == 0)
	{
		//separating on the Symbol
		cmds[i] = strsep(&inputString,Symbol);
		if(cmds[i] == NULL)
		{
			flag = 1;
		}
		else
		{
			//empty commands between two symbols
			int len = strlen(cmds[i]);
			if(len == 0)
			{
				i--;
			}
		}
		i++;
	}
	if(cmds[1] == NULL)//only one command after parsing
	{
		ret_val = FAILURE_CODE;
	}
	return ret_val;
}


OpType parseInput(char** cmdsPlusArgsArray[],char* inputString)
{
	// This function will parse the input string into multiple commands or a single command with arguments depending on the delimiter (&&, ##, >, or spaces).
	int i = 0;
	OpType ret_val;//opertion type enum

	STATUS_CODE code = FAILURE_CODE;

	//array for commands and its arguments
	char* cmdsPlusArgs[MAX_COMMANDS];

	//checks for each case
	code = parseForDifferentSymbols(cmdsPlusArgs, inputString, "##"); //Sequential checking
	if (code == SUCCESS_CODE)
	{
		// for handling if the string is a serial chain of commands
		while (i < MAX_COMMANDS && cmdsPlusArgs[i] != NULL)
		{
			parseForDifferentSymbols(cmdsPlusArgsArray[i], cmdsPlusArgs[i] , " ");//parse for spaces
			i++;
		}
		ret_val = Sequential;
	}
	else
	{
		code = parseForDifferentSymbols(cmdsPlusArgs, inputString, "&&"); //Parallel checking
		if (code == SUCCESS_CODE)
		{
			// for handling if the string is a parallel chain of commands
			i = 0;
			while (i < MAX_COMMANDS && cmdsPlusArgs[i] != NULL)
			{
				parseForDifferentSymbols(cmdsPlusArgsArray[i], cmdsPlusArgs[i], " ");//parse for spaces
				i++;
			}
			ret_val = Parallel;
		}
		else
		{
			code = parseForDifferentSymbols(cmdsPlusArgs, inputString, ">"); //redirection checking
			if (code == SUCCESS_CODE)
			{
				// for handling if the string is a parallel chain of commands
				i = 0;
				while (i < MAX_COMMANDS && cmdsPlusArgs[i] != NULL)
				{
					parseForDifferentSymbols(cmdsPlusArgsArray[i], cmdsPlusArgs[i], " ");//parse for spaces
					i++;
				}
				ret_val = Redirect;
			}
			else//not the sequential, parallel or redirection
			{
				parseForDifferentSymbols(cmdsPlusArgsArray[0], inputString, " ");//parse for spaces
				if(strcmp(cmdsPlusArgsArray[0][0],"cd") == 0)//change directory command check
				{
					ret_val = ChangeDirectory;
				}
				else
				{
					if(strcmp(cmdsPlusArgsArray[0][0],"exit") == 0)//exit command check
					{
						ret_val = Exit;
					}
					else//normal execute
					{
						ret_val = Execute;
					}
				}
			}
		}

	}
	return ret_val;

}

void executeCommand(char* cmdPlusArgs[])
{
	int status;
	// This function will fork a new process to execute a command
	if(strcmp(cmdPlusArgs[0], "cd") == 0)
	{
		if(chdir(cmdPlusArgs[1]) < 0)
		{
			printf("Shell: Incorrect command\n");
		}
	}
	else if(strcmp(cmdPlusArgs[0], "exit") == 0)//exit command --> quit shell
	{
		printf("Exiting shell...\n");
		exit(0);
	}
	else
	{
		//printf("hello\n");
		pid_t procID = fork();//new child process
		if(procID < 0)//fork failure
		{
			printf("Shell: Incorrect command\n");
		}
		else if(procID == 0)//child
		{	
			//signal(SIGINT, SIG_DFL);//default the SIGINT interrupt
			signal(SIGINT,handle_sigint);
			//signal(SIGTSTP,SIG_DFL);//default the SIGTSTP interrupt
			signal(SIGTSTP,handle_sigint);
			int ret_val = execvp(cmdPlusArgs[0],cmdPlusArgs);//successfull execution will not return
			if(ret_val < 0)//exec failure
			{
				printf("Shell: Incorrect command\n");
				exit(0);
			}
		}
		else//parent --> wait for child
		{
			waitpid(procID,&status,WUNTRACED | WCONTINUED);
			if(!WIFSTOPPED(status))
			{
				waitpid(procID,&status,0);
			}
			if(WIFCONTINUED(status))
			{
				waitpid(procID,&status,0);
			}
		}

	}

}

void executeParallelCommands(char** cmdPlusArgsArray[])
{
	// This function will run multiple commands in parallel
	int status;
	int k = 0;
	while(k != MAX_COMMANDS && cmdPlusArgsArray[k] != NULL && cmdPlusArgsArray[k][0] != NULL)//count the number of commands
	{
		k++;
	}
	if(k > 0)//at least one command
	{
		int procID1 = fork();//child1 for command 1
		if(procID1 < 0)//fork fail
		{
			printf("Shell: Incorrect command\n");
		}
		else if(procID1 == 0)//child 1
		{
			signal(SIGINT, SIG_DFL);//default the SIGINT interrupt
			signal(SIGTSTP,SIG_DFL);//default the SIGTSTP interrupt

			if(strcmp(cmdPlusArgsArray[0][0], "cd") == 0)
			{
				exit(0);//kill child for cd --> cd to be done in parent
			}
			else if(strcmp(cmdPlusArgsArray[0][0], "exit") == 0)//exit command --> quit shell
			{
				exit(0);//kill child for exit --> exit to be done in parent also
			}
			else
			{
				if(execvp(cmdPlusArgsArray[0][0],cmdPlusArgsArray[0]) < 0)//execute command 1 in child 1
				{
					printf("Exiting shell...\n");
					return;
				}
			}
		}
		else//parent
		{
			if(strcmp(cmdPlusArgsArray[0][0], "cd") == 0)//change directory in parent
			{
				if(chdir(cmdPlusArgsArray[0][1]) < 0)
				{
					printf("Shell: Incorrect command\n");
				}
			}
			else if(strcmp(cmdPlusArgsArray[0][0], "exit") == 0)//exit command --> quit shell
			{
				printf("Exiting shell...\n");//exit from parent and while(1) loop
				exitflag = 1;
				return;
			}
			if(k > 1)//atleast 2 commands
			{
				int procID2 = fork();//child 2 for command 2
				if(procID2 < 0)//fork fail
				{
					printf("Shell: Incorrect command\n");
				}
				else if(procID2 == 0)//child 2
				{
					signal(SIGINT, SIG_DFL);//default the SIGINT interrupt
					signal(SIGTSTP,SIG_DFL);//default the SIGTSTP interrupt
					if(strcmp(cmdPlusArgsArray[1][0], "cd") == 0)
					{
						exit(0);//kill child for cd --> cd to be done in parent
					}
					else if(strcmp(cmdPlusArgsArray[1][0], "exit") == 0)//exit command --> quit shell
					{
						exit(0);//kill child for exit --> exit to be done in parent also
					}
					else
					{
						if(execvp(cmdPlusArgsArray[1][0],cmdPlusArgsArray[1]) < 0)//execute command 2 in child 2 parallelly
						{
							printf("Exiting shell...\n");
						}
					}
				}
				else//parent
				{
					if(strcmp(cmdPlusArgsArray[1][0], "cd") == 0)//change directory in parent
					{
						if(chdir(cmdPlusArgsArray[1][1]) < 0)
						{
							printf("Shell: Incorrect command");
						}
					}
					else if(strcmp(cmdPlusArgsArray[1][0], "exit") == 0)//exit command --> quit shell
					{
						waitpid(procID1,&status,0);//before exiting wait for any children
						//can be skipped --> just prevents ill printing of prompt
						printf("Exiting shell...\n");
						exitflag = 1;
						return;
					}
					if(k > 2)//atleast 3 commands
					{
						int procID3 = fork();//child 3 for command 3
						if(procID3 < 0)//fork fail
						{
							printf("Shell: Incorrect command\n");
						}
						else if(procID3 == 0)//child 3
						{
							signal(SIGINT, SIG_DFL);//default the SIGINT interrupt
							signal(SIGTSTP,SIG_DFL);//default the SIGTSTP interrupt
							if(strcmp(cmdPlusArgsArray[2][0], "cd") == 0)
							{
								exit(0);//kill child for cd --> cd to be done in parent
							}
							else if(strcmp(cmdPlusArgsArray[2][0], "exit") == 0)//exit command --> quit shell
							{
								exit(0);//kill child for exit --> exit to be done in parent also
							}
							else
							{
								if(execvp(cmdPlusArgsArray[2][0],cmdPlusArgsArray[2]) < 0)//execute command 3 in child 3 parallelly
								{
									printf("Exiting shell...\n");
								}
							}
						}
						else//parent
						{
							if(strcmp(cmdPlusArgsArray[2][0], "cd") == 0)//change directory in parent
							{
								if(chdir(cmdPlusArgsArray[2][1]) < 0)
								{
									printf("Shell: Incorrect command");
								}
							}
							else if(strcmp(cmdPlusArgsArray[2][0], "exit") == 0)//exit command --> quit shell
							{
								waitpid(procID1,&status,0);//before exiting wait for any children
								waitpid(procID2,&status,0);//can be skipped --> just prevents ill printing of prompt
								printf("Exiting shell...\n");
								exitflag = 1;
								return;
							}
							if(k > 3)//atleast 4 commands
							{
								int procID4 = fork();//child 4 for command 4
								if(procID4 < 0)//fork fail
								{
									printf("Shell: Incorrect command\n");
								}
								else if(procID4 == 0)//child 4
								{
									signal(SIGINT, SIG_DFL);//default the SIGINT interrupt
									signal(SIGTSTP,SIG_DFL);//default the SIGTSTP interrupt
									if(strcmp(cmdPlusArgsArray[3][0], "cd") == 0)
									{
										exit(0);//kill child for cd --> cd to be done in parent
									}
									else if(strcmp(cmdPlusArgsArray[3][0], "exit") == 0)
									{
										exit(0);//kill child for exit --> exit to be done in parent also
									}
									else
									{
										if(execvp(cmdPlusArgsArray[3][0],cmdPlusArgsArray[3]) < 0)//execute command 3 in child 3 parallelly
										{
											printf("Exiting shell...\n");
										}
									}
								}
								else//parent
								{
									if(strcmp(cmdPlusArgsArray[3][0], "cd") == 0)//change directory in parent
									{
										if(chdir(cmdPlusArgsArray[3][1]) < 0)
										{
											printf("Shell: Incorrect command");
										}
									}
									else if(strcmp(cmdPlusArgsArray[3][0], "exit") == 0)//exit command --> quit shell
									{
										waitpid(procID1,&status,0);//before exiting wait for any children
										waitpid(procID2,&status,0);//can be skipped --> just prevents ill printing of prompt
										waitpid(procID3,&status,0);
										printf("Exiting shell...\n");
										exitflag = 1;
										return;
									}
									waitpid(procID1,&status,0);//before exiting wait for any children
									waitpid(procID2,&status,0);//can be skipped --> just prevents ill printing of prompt
									waitpid(procID3,&status,0);
									waitpid(procID4,&status,0);
								}
							}
							else
							{
								waitpid(procID1,&status,0);//before exiting wait for any children
								waitpid(procID2,&status,0);//can be skipped --> just prevents ill printing of prompt
								waitpid(procID3,&status,0);
							}
						}

					}
					else
					{
						waitpid(procID1,&status,0);//before exiting wait for any children
						waitpid(procID2,&status,0);//can be skipped --> just prevents ill printing of prompt
					}
				}
			}
			else
			{
				waitpid(procID1,&status,0);//before exiting wait for any children
				//can be skipped --> just prevents ill printing of prompt
			}
		}
	}
	else//no command
	{
		return;
	}
}

void executeSequentialCommands(char** cmdPlusArgsArray[])
{	
	// This function will run multiple commands in sequence
	int i = 0;
	//execute commands one by one for valid arguments --> cmd should not be NULL upto max commands
	while(i < MAX_COMMANDS && cmdPlusArgsArray[i] != NULL && cmdPlusArgsArray[i][0] != NULL && (strlen(cmdPlusArgsArray[i][0]) != 0))
	{
		executeCommand(cmdPlusArgsArray[i]);
		i++;
	}
	
}

void executeCommandRedirection(char** cmdPlusArgsArray[])
{
	int status;
	// This function will run a single command with output redirected to an output file specificed by user
	//checking for valid two arguments
	if(cmdPlusArgsArray[1] != NULL && cmdPlusArgsArray[1][0] != NULL && (strlen(cmdPlusArgsArray[1][0]) != 0))
	{
		pid_t procID = fork();//new child process

		if(procID < 0)//fork fail
		{
			printf("Shell: Incorrect command\n");
		}
		else if(procID == 0)//child
		{
			close(STDOUT_FILENO);//close stdout
			open(cmdPlusArgsArray[1][0], O_CREAT | O_RDWR, S_IRWXU);//open given stream with create,read-write prmissions

			int ret_val = execvp(cmdPlusArgsArray[0][0],cmdPlusArgsArray[0]);//never returns on success
			if(ret_val < 0)//exec failure
			{
				printf("Shell: Incorrect command\n");
			}
		}
		else//parent --> wait for child to execute
		{
			waitpid(procID,&status,0);
		}
	}
	else//arguments error
	{
		printf("Shell: Incorrect command\n");
	}
}

int main()
{
	// Initial declarations
	char* input;//input string from user
	input = NULL;//initialize to NULL
	size_t inputSize = 0;
	OpType operationCode;//code for the operation to be performed
	char** cmds[5];//commands + args array
	char currWorkDir[400];//current working directory string
	//allocating space for multiple commands
	int k = 0;
	while(k < MAX_COMMANDS)
	{
		cmds[k] = (char**)malloc(sizeof(char**)*STR_MAXLEN);
		k++;
	}
	cmds[MAX_COMMANDS] = NULL;//last element NULL

	signal(SIGINT, SIG_IGN);//ignore the default behaviour of SIGINT
	//signal(SIGINT,handle_sigint);
	signal(SIGTSTP,SIG_IGN);//ignore the default behaviour of SIGTSTP
	
	while(1)	// This loop will keep your shell running until user exits.
	{
		// Print the prompt in format - currentWorkingDirectory$
		getcwd(currWorkDir,sizeof(currWorkDir));
		printf("%s$",currWorkDir);
		
		// accept input with 'getline()'
		getline(&input,&inputSize,stdin);
		input = strsep(&input,"\n");

		if(strlen(input) != 0)//valid input string
		{
			// Parse input with 'strsep()' for different symbols (&&, ##, >) and for spaces.
			operationCode = parseInput(cmds, input);//parseInput uses strsep()

			if(operationCode == Exit)	// When user uses exit command.
			{
				printf("Exiting shell...\n");
				break;
			}
			if(operationCode == Parallel)
			{
				executeParallelCommands(cmds);		// This function is invoked when user wants to run multiple commands in parallel (commands separated by &&)
				if(exitflag)
					break;
			}
			else if(operationCode == Sequential)
			{
				executeSequentialCommands(cmds);	// This function is invoked when user wants to run multiple commands sequentially (commands separated by ##)
				if(exitflag)
					break;
			}
			else if(operationCode == Redirect)
			{
				executeCommandRedirection(cmds);	// This function is invoked when user wants redirect output of a single command to and output file specificed by user
				if(exitflag)
					break;
			}
			else if(operationCode == Execute || operationCode == ChangeDirectory)
			{
				executeCommand(cmds[0]);		// This function is invoked when user wants to run a single commands
				if(exitflag)
					break;
			}
		}
	}
}