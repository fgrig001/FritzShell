/*
* Course: CS 100 Winter 2014
*
* First Name: Frederick
* Last Name: Grigsby
* Username: fgrig001
* email address: fgrig001@ucr.edu
*
*
* Assignment: Homework #6
*
* I hereby certify that the contents of this file represent
* my own original individual work. Nowhere herein is there
* code from any outside resources such as another individual,
* a website, or publishings unless specifically designated as
* permissible by the instructor or TA. */


//HW #6 NOTE FOR TA: The functions added this week are //void handle_sigint(int i);
//bool my_exec(char *arg,char *argv[]);


#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pwd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <vector>
#include <iostream>
using namespace std;

//GLOBAL VARIABLES
#define READ_END 0              //Pipe read end
#define WRITE_END 1             //pipe write end
bool IS_BG;                     //Background job flag
char *UNAME;					//User name
char *argv[40];        		    //Argument vector
const int BUFSIZE=256; 			//Size of input buffer
char buf[BUFSIZE];      		//Input buffer
int fd_out,fd_in;       		//File descriptors
int saved_stdout,saved_stdin;	//File descriptors to save stdin and stdout

//FUNCTION PROTOTYPES

//Primary Functions
//*******************************************
int my_shell();
//my_shell: primary function that runs the shell,
//  returns 0 on sucesfull exit; 
void setup_shell();
//setup_shell: sets up all shell variables on startup
void get_command();
//get_command: gets a command from the user and stores 
// it in the global char array buf
void get_argv();
//get_argv: makes pointers in argv point to arguments
// in buff and is terminated with a null pointer
int execute(char *arg[]);
//execute: executes arguments stored in argv
void io_redirect(char *arg[]);
//io_redirect: searches argv for redirecion arguments
//  (<,>) , null's them out and redirects stdin and 
// std out acordingly
void io_reset();
//io_reset: to be used after execute to reset 
//  stdin and stdout
void pipe_run();
//pipe_run: parses argv by pipes (|), and calls a sub-proces to 
// execute and pipe io between multiple proceses
int execute_sub_proces(char **arg,int *pipe,int pipe_index,int max_index);
//execute_sub_proces: executes a subproces. Takes in arguments for
// pipe identifier, specific pipe index, and the total number of 
// pipes being used across subsiquent proeseses.
bool my_exec(char *arg,char *argv[]);
//Searches PATH for executable "arg" and calls exec when it
// finds it. Returns false if "arg" is not found, returns 
// true elswise.
void handle_sigchld(int i);
//handle_sigchld: handles the SIGCHLD signal appropriatly.
void handle_sigint(int i);
//handle_siging: handles the SIGINT signal such that it is 
// ignored in the parent process, but will exit out of a 
// child process.
//*******************************************

//Auxilary Functions
//*******************************************
bool isExit();
//isExit: returns true if the paramater char *c
//   is "Exit" 
//*******************************************

//MAIN
int main(){
	//setup signal handeling
	signal(SIGCHLD,handle_sigchld);
	signal(SIGINT,handle_sigint);
	//run shell
	return my_shell();
}

//*********************************
// PRIMARY FUNCTIONS
//*********************************

int my_shell(){
	setup_shell();
	get_command();
	while(!isExit()){
		get_argv();
		pipe_run();
		get_command();
	}
	return 0;
}

void setup_shell(){
	struct passwd *pwd=getpwuid(getuid());
	UNAME=pwd->pw_name;
}

void get_command(){
	printf("%s $ ",UNAME);
	fgets(buf,BUFSIZE,stdin);
}

int execute(char *arg[]){
	int pid=fork();
	int status;
	switch(pid){
		case 0:
			if(my_exec(arg[0],arg)==false)
				fprintf(stderr,"-fritzshell: %s: command not found\n",arg[0]);
			_exit(0);
			break;
		case -1:
			return -1;
			break;
	}
}

int execute_sub_proces(char **arg,int *pipe,int pipe_index,int max_index){
	int pid=fork();
	int status;
	switch(pid){
		case 0:
			//first/middle proces 
			if(pipe_index!=max_index){
				if(dup2(pipe[pipe_index+1],STDOUT_FILENO)==-1){
					fprintf(stderr,"-fritzshell: duplicating file descriptor failed: errno %i\n",errno);
					_exit(errno);
				}
			}
			//last/middle proces
			if(pipe_index!=0){
				if(dup2(pipe[pipe_index-2],STDIN_FILENO)==-1){
					fprintf(stderr,"-fritzshell: duplicating file descriptor failed: errno %i\n",errno);
					_exit(errno);
				}
			}
			//close pipes
			for(int i=0;i<max_index;++i)
				close(pipe[i]);
			//redirect io and exec
			io_redirect(arg);
			if(my_exec(arg[0],arg)==false)
				fprintf(stderr,"-fritzshell: %s: command not found\n",arg[0]);
			io_reset();
			_exit(0);
			break;
		case -1:
			fprintf(stderr,"-fritzshell: forking failed\n");
			break;
	}
}

void io_redirect(char *arg[]){
	saved_stdout = dup(1);
	saved_stdin = dup(0);
	for(int i=0;arg[i];++i){
		// IF <
		if(*arg[i]=='<'){
			arg[i]=NULL;
			if((fd_out=open(argv[++i],O_RDONLY,S_IRUSR | S_IWUSR
				| S_IROTH))<0){
				fprintf(stderr,"Error: file redirection failed!\n");
				exit(0);
			}
			if(dup2(fd_out,0)==-1){
				fprintf(stderr,"-fritzshell: duplicating file descriptor failed: errno %i\n",errno);
				_exit(errno);
			}
		}
		//IF >
		else if(*arg[i]=='>'){
			arg[i]=NULL;
			if((fd_in=open(arg[++i],O_WRONLY | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))<0){
				fprintf(stderr,"Error: file redirection failed!\n");
				exit(0);
			}
			if(dup2(fd_in,1)==-1){	
				fprintf(stderr,"-fritzshell: duplicating file descriptor failed: errno %i\n",errno);
				_exit(errno);
			}
		}
	}
}

void io_reset(){
	if(dup2(saved_stdout, 1)==-1){
		fprintf(stderr,"-fritzshell: duplicating file descriptor failed: errno %i\n",errno);
		_exit(errno);
	}
	if(dup2(saved_stdin, 0)==-1){
		fprintf(stderr,"-fritzshell: duplicating file descriptor failed: errno %i\n",errno);
		_exit(errno);
	}
}

void pipe_run(){
	int argcount=1;
	char *arg[10][64];
	int count=0;
	IS_BG=false;
	//Parse argv by pipe (|)
	arg[0][count]=argv[0];
	for(int i=0;argv[i];++i){
		if(*argv[i]=='&'){
			IS_BG=true;
			argv[i]=NULL;
			break;
		}
		arg[argcount-1][count++]=argv[i];
		if(*argv[i]=='|'){
			arg[argcount-1][count-1]=NULL;
			count=0;
			argv[i]=NULL;
			argcount++;
		}
	}
	//Create child proces to initialize sub_proceses and 
	// inter-proces piping
	int *pipe_fd;
	int handle_pid=fork();	
	switch(handle_pid){
		case 0:
			if(argcount==1){
				io_redirect(argv);
				execute(argv);
				io_reset();
			}
			else{	
				int pipecount=argcount-1;
				pipe_fd=new int[pipecount*2];
				for(int i=0;i<pipecount;++i)
					pipe((pipe_fd+i*2));
				for(int i=0;i<argcount;++i){
					execute_sub_proces(arg[i],pipe_fd,i*2,pipecount*2);		
				}
				for(int i=0; i<pipecount*2;++i){
					close(pipe_fd[i]);
				}
			}
			while(wait(NULL)>0)
				;
			_exit(0);
			break;
		default:
			//Wait for proceses if not specified to 
			// run in background
			if(IS_BG==false)
				waitid(P_PID,handle_pid,NULL,WSTOPPED);
	}
}


void get_argv(){
	char **next=argv;
	char *c = strtok(buf," \n");
	while(c != NULL){
	    *next++ = c;
	    c=strtok(NULL," \n");
	}
    *next=NULL;
}

bool my_exec(char *arg,char *argv[]){
	char *path;
	char full_path[64];
	int num_paths=1;
	vector<char*>parse;	
	char *PATH=getenv("PATH");
	parse.push_back(PATH);
	for(int i=0;PATH[i];++i){
		if(PATH[i]==':'){
			num_paths++;
			PATH[i]=0;	
			parse.push_back(PATH+i+1);
		}
	}
	for(int i=0;i<num_paths;++i){
		path=parse.back();
		sprintf(full_path,"%s/%s",path,arg);
		if((execv(full_path,argv))!=-1) return true;
		parse.pop_back();
	}
	return false;
}

//*********************************
// AUXILARY FUNCTIONS
//*********************************

bool isExit(){
	if(buf[0]=='E'&&buf[1]=='x'&&buf[2]=='i'&&buf[3]=='t')
		return true;
	return false;
}


void handle_sigchld(int i){
	wait(NULL);	
}

void handle_sigint(int i){}
	

