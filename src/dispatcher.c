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

static void IO_helper(struct command* pipeline)
{	
	int fd_in;
	int fd_write;
	
	//case we have an input file
	if (pipeline->input_filename) { 	   
		fd_in = open(pipeline->input_filename,O_RDONLY,0644);
		if (fd_in == -1) {
			perror("open");
			exit(1);
		}
		
		if(dup2(fd_in,STDIN_FILENO) == -1) {
			perror("dup2");
			exit(1);
		}
	}


	if (pipeline->pipe_to == NULL) {  //not the first command
		dup2(fd_in,STDIN_FILENO);
	}

	//case output append
	if (pipeline->output_type == COMMAND_OUTPUT_FILE_APPEND) {
		fd_write = open(pipeline->output_filename,O_WRONLY|O_CREAT|O_APPEND,0600);
		
		if (fd_write == -1) {
			perror("open");
			exit(1);
		}
		
		///dup2 for out - will only executes if fd_out is 1
		if (dup2(fd_write,STDOUT_FILENO) == -1) {
			perror("dup2");
			exit(1);
		}

	}
	//case output truncate
	if (pipeline->output_type == COMMAND_OUTPUT_FILE_TRUNCATE) {
		fd_write = open(pipeline->output_filename,O_WRONLY| O_CREAT|O_TRUNC,0600);
		
		if (fd_write == -1) {
			perror("open");
			exit(1);
		}
		//dup2 for out - will only executes if fd_out is 1
		if (dup2(fd_write,STDOUT_FILENO) == -1) {
			perror("dup2");
			exit(1);
		}
	}
}



static int dispatch_external_command(struct command *pipeline)
{	
	int fd[2]; //PIPE
	int child_pid;
	int child_pid2; 
	int status2 = 0;
	int status1 = 0;
	int piping = 0;
	

    if (pipeline->output_type == COMMAND_OUTPUT_PIPE) { //ex:pipe to cowsay
            //Pipe & check eror
			if (pipe(fd) == -1) {    
                perror("pipe");
                exit(1);
            }	
			piping = 1;
	}

    //Ceates a new process by duplicating the calling process
    child_pid = fork(); 

    if (child_pid < 0) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    } 
	//Fork first arg
    else if (child_pid == 0) {
		//Forward the output to the input of the command specified by pipe_to.
        if (pipeline->output_type == COMMAND_OUTPUT_PIPE) { 
			//Close the read end
			close(fd[0]);
		
			if (dup2(fd[1], STDOUT_FILENO) == -1) {
				perror("dup2");
				exit(1);
			}	
		}

		IO_helper(pipeline);
		
		//Child executes (for I/O stuff)
		execvp(pipeline->argv[0],pipeline->argv); 
		perror("execvp");
		exit(1);

	}
	
	//1st Parent wait for the 1st child
	else if (child_pid > 0) {

		close(fd[1]);
		//Wait for a child matching pid to die
		int w = waitpid(child_pid,&status1, 0);
			if (w == -1) {
       			perror("waitpid");
       			exit(1);
   			}

		//This is being checked in the 1st parent
		if (piping) {  
			//Create a second child & run the 2nd command						
			child_pid2 = fork(); 
	
		
			if (child_pid2 < 0) {
				fprintf(stderr, "Second fork failed\n");
				exit(1);
			} 

			//2nd child
			if (child_pid2 == 0) {
				//close read end
				close(fd[0]);
				//Redirect input
				dup2(fd[0],STDIN_FILENO);
				

				IO_helper(pipeline);

				//2nd child executes
				execvp(pipeline->argv[0],pipeline->argv); 
				perror("execvp");
				exit(1); 

			}
			//Parent
			else if (child_pid2 > 0) {
				
				close(fd[0]);
				int w = waitpid(child_pid2,&status2, 0);
				

				if (w == -1) {
					perror("waitpid");
					exit(1);
				}

				//check exit status of waitpid C - if true its normal
				if ( WIFEXITED(status2) ){
					int exit_status = WEXITSTATUS(status2);  
					return exit_status; 
				}
			}

		}
		//Return the exit status of the 1st child
		else { 

			if ( WIFEXITED(status1) )	{
				int exit_status = WEXITSTATUS(status1); 
				return exit_status;      	
			}

		}		

	}



	return status2; 


                    
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
