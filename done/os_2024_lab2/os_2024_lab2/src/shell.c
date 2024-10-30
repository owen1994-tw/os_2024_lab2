#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p){

	/* ">" */
	if(p->out_file != NULL)
	{
		// printf("p->out_file = %s \n", p->out_file);
		int fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		if (dup2(fd, STDOUT_FILENO) == -1) {
			perror("dup2");
			close(fd);
			exit(EXIT_FAILURE);
		}
		close(fd);
	}

	/* "<" */
	if(p->in_file != NULL)
	{
		// printf("p->in_file = %s \n", p->in_file);
		int fd = open(p->in_file, O_RDONLY, 0644);
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		if (dup2(fd, STDIN_FILENO) == -1) {
			perror("dup2");
			close(fd);
			exit(EXIT_FAILURE);
		}
		close(fd);
	}

	/* "|" */
	// if(p->in != STDIN_FILENO || p->out != STDOUT_FILENO  )
	// {
	// 	int dupin = dup(STDIN_FILENO), dupout = dup(STDOUT_FILENO);
	// 	if (dup2(p->in , dupin) == -1) {
	// 		perror("dup2");
	// 		exit(EXIT_FAILURE);
	// 	}

	// 	if (dup2(p->out , STDOUT_FILENO) == -1) {
	// 		perror("dup2");
	// 		exit(EXIT_FAILURE);
	// 	}
	// 	close(dupin);
	// 	close(dupout);
	// }

	
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{


	
    pid_t pid;

    pid = fork();
    switch (pid) {
    case -1:
        perror("fork error");
        fprintf(stderr, "fork error\n");
        exit(EXIT_FAILURE);

    case 0:
        // 只對於子程序進行重定向
        redirection(p);
        execvp(p->args[0], p->args);
        perror("execvp error");
        exit(EXIT_FAILURE);

    default:
        waitpid(pid, NULL, 0);
    }



  	return 1;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * 如果是多個指令，則使用 pipe() 建立 process 之間的溝通橋樑, 並依照 cmd_node 的數量來呼叫 spawn_proc(),
 * 過程中會需要考慮是否為外部指令或是內建指令
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
    int pipe_fd[2];
    int prev_pipe_fd[2] = {-1, -1}; // 用於保存前一個管道的文件描述符

    while (cmd->head) {
        struct cmd_node *temp = cmd->head;

        // 創建新的管道
        if (pipe(pipe_fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            // 子進程
            if (prev_pipe_fd[0] != -1) {
                // 不是第一個命令，將標準輸入重定向到前一個管道的讀端
                if (dup2(prev_pipe_fd[0], STDIN_FILENO) == -1) {
                    perror("dup2 prev_pipe_fd[0]");
                    exit(EXIT_FAILURE);
                }
            }

            if (cmd->head->next != NULL) {
                // 不是最後一個命令，將標準輸出重定向到當前管道的寫端
                if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
                    perror("dup2 pipe_fd[1]");
                    exit(EXIT_FAILURE);
                }
            }

            // 關閉所有管道端
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            if (prev_pipe_fd[0] != -1) {
                close(prev_pipe_fd[0]);
                close(prev_pipe_fd[1]);
            }

            // 執行內建命令或外部命令
            int status = searchBuiltInCommand(temp);
            if (status != -1) {
                redirection(temp);
                execBuiltInCommand(status, temp);
            } else {
                spawn_proc(temp);
            }

            exit(EXIT_SUCCESS);
        } else {
            // 父進程
            // 關閉當前管道的寫端
            close(pipe_fd[1]);
            if (prev_pipe_fd[0] != -1) {
                // 關閉前一個管道的讀端
                close(prev_pipe_fd[0]);
            }

            // 保存當前管道的讀端，供下一個命令使用
            prev_pipe_fd[0] = pipe_fd[0];
            prev_pipe_fd[1] = pipe_fd[1];

            // 移動到下一個命令
            cmd->head = cmd->head->next;
        }
    }

    // 父進程等待所有子進程結束
    while (wait(NULL) > 0);

    return 1;
}
// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				// printf("There is only a single command\n");
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			// printf("There are multiple commands ( | )\n");
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
