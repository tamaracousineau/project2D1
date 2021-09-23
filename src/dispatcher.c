#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "dispatcher.h"
#include "shell_builtins.h"
#include "parser.h"

#include <unistd.h>
#include <sys/wait.h>

/**
 * dispatch_external_command() - run a pipeline of commands
 *
 * @pipeline:   A "struct command" pointer representing one or more
 *              commands chained together in a pipeline.  See the
 *              documentation in parser.h for the layout of this data
 *              structure.  It is also recommended that you use the
 *              "parseview" demo program included in this project to
 *              observe the layout of this structure for a variety of
 *              inputs.
 *
 * Note: this function should not return until all commands in the
 * pipeline have completed their execution.
 *
 * Return: The return status of the last command executed in the
 * pipeline.
 */


// static void run_command(struct command* pipeline, int fd_in, int fd_out) 
// {

	


// }

static int dispatch_external_command(struct command *pipeline)
{	
	int fd[2]; //PIPE
	int child_pid;
	int status;
	
	//  if (pipeline->pipe_to == NULL) {  //not the first command
	// 	dup2(fd[1],STDOUT_FILENO);
	// }


	if (pipeline->output_type == COMMAND_OUTPUT_PIPE) { //ex: ls -l | wc -l
		printf("pipe\n");
		pipe(fd);

		//handle error
		if (pipe(fd) == -1) {
			perror("pipe");
			exit(EXIT_FAILURE);
		}

		child_pid = fork(); //first fork

		if (child_pid == 0) {
		
			//Send the info through output side of pipe
			dup2(fd[1], STDOUT_FILENO);
			//Child process closes up input side of pipe
			close(fd[0]);
			close(fd[1]);

			execvp(pipeline->argv[0],pipeline->argv); 
			perror("execvp");
			return -1;
		}

		else {
			child_pid = fork(); 

			if (child_pid ==0) {

				dup2(fd[0],STDIN_FILENO);
				//close write end (output side)
				close(fd[1]);
				//close read end
				close(fd[0]);

				execvp(pipeline->argv[0],pipeline->argv); 
				perror("execvp");
				return -1; 

			}

			else {
				close(fd[0]);
				close(fd[1]);
				waitpid(child_pid,&status, 0);

			}
			
			return status; 

		}	
		

	}

	else  {	//D1 with file redirections (ex: echo hello > test.txt)

		status = 0;
		//Ceates a new process by duplicating the calling process
		child_pid = fork(); 

		if (child_pid < 0) {
			fprintf(stderr, "fork failed\n");
			exit(1);
		} 
		else if (child_pid == 0) {
			
			if (pipeline->input_filename) { 	   //you have an input 
				int fd_in = open(pipeline->input_filename,O_RDONLY,0644);
				if (fd_in == -1) {
					perror("open");
					exit(1);
				}

				if(dup2(fd_in,STDIN_FILENO)==-1) {
					perror("dup2");
					exit(1);
				}
			
			}
			if (pipeline->output_type == COMMAND_OUTPUT_FILE_APPEND) {
				int fd_write = open(pipeline->output_filename,O_WRONLY|O_CREAT|O_APPEND,0600);
				//if neg one
				if (fd_write == -1) {
					perror("open");
					exit(1);
				}
				if(dup2(fd_write,STDOUT_FILENO)==-1) {
					perror("dup2");
					exit(1);
				}


			}
			else if (pipeline->output_type == COMMAND_OUTPUT_FILE_TRUNCATE) {
				int fd_write = open(pipeline->output_filename,O_WRONLY| O_CREAT|O_TRUNC,0600);
				if (fd_write == -1) {
					perror("open");
					exit(1);
				}
				if (dup2(fd_write,STDOUT_FILENO) == -1) {
					perror("dup2");
					exit(1);
				}
			}

			execvp(pipeline->argv[0],pipeline->argv); 
			perror("execvp");
	
			return -1;
			 			
		}
		//Parent:
		else if (child_pid > 0) {
			waitpid(child_pid,&status, 0);
			 
		}

		return status;

	}

	
	
}

/**
 * dispatch_parsed_command() - run a command after it has been parsed
 *
 * @cmd:                The parsed command.
 * @last_rv:            The return code of the previously executed
 *                      command.
 * @shell_should_exit:  Output parameter which is set to true when the
 *                      shell is intended to exit.
 *
 * Return: the return status of the command.
 */
static int dispatch_parsed_command(struct command *cmd, int last_rv,
				   bool *shell_should_exit)
{
	/* First, try to see if it's a builtin. */
	for (size_t i = 0; builtin_commands[i].name; i++) {
		if (!strcmp(builtin_commands[i].name, cmd->argv[0])) {
			/* We found a match!  Run it. */
			return builtin_commands[i].handler(
				(const char *const *)cmd->argv, last_rv,
				shell_should_exit);
		}
	}

	/* Otherwise, it's an external command. */
	return dispatch_external_command(cmd);
}

int shell_command_dispatcher(const char *input, int last_rv,
			     bool *shell_should_exit)
{
	int rv;
	struct command *parse_result;
	enum parse_error parse_error = parse_input(input, &parse_result);

	if (parse_error) {
		fprintf(stderr, "Input parse error: %s\n",
			parse_error_str[parse_error]);
		return -1;
	}

	/* Empty line */
	if (!parse_result)
		return last_rv;

	rv = dispatch_parsed_command(parse_result, last_rv, shell_should_exit);
	free_parse_result(parse_result);
	return rv;
}
