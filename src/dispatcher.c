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

//Does this need to be in dispatcher.h?
static int spawn_child(int in, int out, struct command *pipeline)
{
	//Dup2 write

	//Dup2 read 

	//Execvp()

	//Check if there is another pipe & call spawn_child(pipe output, output,command)



}
static int dispatch_external_command(struct command *pipeline)
{	
	//STDIN_FILENO
	//STDOUT_FILENO - file decriptor

	int status = 0; 
	int fd[2]; //list of 2 ints

	//1. Check if the command has > or >>

	//2. Check if the command has <

	//3. Check if command has a pipe

	if (pipeline->output_type == COMMAND_OUTPUT_PIPE) {
		pipe(fd);
	}
	
	//Ceates a new process by duplicating the calling process
	int child_pid = fork(); 
	

	if (child_pid < 0) {
		fprintf(stderr, "fork failed\n");
		exit(1);
	} 
	else if (child_pid == 0) {
		if (pipeline->output_type == COMMAND_OUTPUT_PIPE) {
			close(fd[0]);		//Close read end
			dup2(fd[2], STDOUT_FILENO); //Child closes read end, pipe filled. 
			execvp(pipeline->argv[0],pipeline->argv); 
			perror("execvp");
			return -1; 
		}
		//Write output  (> and >>)
		if ((pipeline->output_type == COMMAND_OUTPUT_FILE_APPEND) || (pipeline->output_type == COMMAND_OUTPUT_FILE_TRUNCATE)) {
			//file = 0 //flags read and write 
			int fd_write = open(pipeline->output_filename,O_RDWR|O_APPEND,0644);
			dup2(fd[1],fd_write); 
			execvp(pipeline->argv[0],pipeline->argv); 
			perror("execvp");
			return -1; 	

			close(fd_write);

		}
		//file to get input from (<)
		if (pipeline->input_filename) { 
			int fd_write2 = open(pipeline->input_filename,O_RDONLY,0644);
			dup2(fd_write2,STDIN_FILENO);
			execvp(pipeline->argv[0],pipeline->argv); 
			perror("execvp");
			return -1; 			//child failed - returns dispatch pass the results of the chain

			close(fd_write2);
		}
	}
	else if (child_pid > 0) {
		if (pipeline->output_type == COMMAND_OUTPUT_PIPE) {
			close(fd[1]); //Close write
		}
		waitpid(child_pid,&status, 0);

	}
	
	//Parent sends data to the child 
	if (pipeline->output_type == COMMAND_OUTPUT_PIPE) {
		int child_pid2 = fork();
		if (child_pid2 == 0) {
			dup2(fd[0],STDIN_FILENO);
			execvp(pipeline->argv[0],pipeline->argv); 
			perror("execvp");
			return -1; 		
		}

		else if (child_pid2 > 0) {
			close(fd[0]);		//Close read
			waitpid(child_pid2,&status, 0);

		}

	}

 
	return status;
	
	
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
