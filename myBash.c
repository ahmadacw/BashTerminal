#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

int children[2];
int FatherID;
int background=0;
// prepare and finalize calls for initialization and destruction of anything required

int prepare(void)
{
   	struct sigaction action1;
	struct sigaction action2;
	memset(&action1, 0, sizeof(action1));
	memset(&action2, 0, sizeof(action2));
	action1.sa_handler = SIG_IGN;
	action1.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &action1, NULL) != 0) { //override the default handling for SIGINT to ignoring
		fprintf(stderr, "error occurred sigaction \n");
	}
	action2.sa_handler = SIG_DFL;
	action2.sa_flags = SA_NOCLDWAIT;
	if (sigaction(SIGCHLD, &action2, NULL) != 0) { //override the default handling for the SIGCHLD to make the background
		fprintf(stderr, "error occurred sigaction\n ");// childrens to prevent zombies
	}
	return 0;
}
int finalize(void){
    return 0;
}

int ContainsPipe(char** arglist,int count){
    for(int i=0 ;i<count;i++){
        if(arglist[i]!=NULL && arglist[i][0]=='|') return i;
    }
    return 0;
}

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char** arglist){
    // I realize this might fail if a signal is sent while running the first few lines of 
    //      code, but sigprocmask is breaking stdin for some reason
    struct sigaction NORMALHANDLER;
    NORMALHANDLER.sa_flags=SA_RESTART;
    NORMALHANDLER.sa_handler=SIG_DFL;
    int fd[2];
    int status=-1;
    int pipeIndex=0;
    int waitOnChild=0;
    waitOnChild=arglist[count-1]!=NULL && arglist[count-1][0]=='&' && arglist[count-1][1]=='\0';
    background=waitOnChild;
    pipeIndex=ContainsPipe(arglist,count);
    if( pipeIndex ) printf("Piping\n");
    if(pipeIndex){
        if(pipe(fd)==-1){
            printf("Problem With Pipe\n");
        }
    }
    int id2=0;
    if(pipeIndex){ // pipe was detected
        id2=fork();
        if(id2==0){// code for the write side of our pipe
	    sigaction(SIGINT,&NORMALHANDLER,NULL);
            dup2 (fd[1],STDOUT_FILENO);
            close(fd[0]);
            close(fd[1]);
            arglist[pipeIndex]=NULL;
            if(execvp(arglist[0],arglist) <0){
                fprintf(stderr,"request file doesn't exist in left side of pipe\n");
                exit(1);
            }
        }
       
    }
    	
    // again could cause a problem but it brakes stdin if I fix it
    if(pipeIndex){
	close(fd[0]);
    	close(fd[1]);
        printf("waiting on write to pipe\n");
	while(1){
        	if(wait(&status)==-1){
			if (errno != ECHILD) {                           // because we changed the SIGCHILD handle
			fprintf(stderr,"error occured in wait\n");
			return 0;
			}
		}
		break;
	}
        printf(" done waiting on pipe\n ");
    } 
    int id1=fork();
    if(id1==0){ // code for either the passed function or the read side of our pipe
	if(!waitOnChild) sigaction(SIGINT,&NORMALHANDLER,NULL);
        if(pipeIndex) pipeIndex++;
        if(pipeIndex){  //rerouting stdin
            dup2( fd[0],STDIN_FILENO);
            close(fd[0]);
            close (fd[1]);
        } 
        if(waitOnChild){ // & was found and need to be deleted
              arglist[count-1]=NULL;
        }
        if( execvp(arglist[pipeIndex],arglist+pipeIndex)<0){
            if(pipeIndex)fprintf(stderr,"request file doesn't exist right side of pipe");
            else fprintf(stderr,"request fild doesn't exist");
	    exit(1);
        }
    }
   
    if(!waitOnChild){
                printf("Waiting on Children\n");
		if(-1==waitpid(id1,&status,0)){
			if(errno!=ECHILD){
					fprintf(stderr,"error occured in waitpid \n");
					return 0;
				}
			}                
                printf("Done Waiting\n");
     }
   
    
    return 1;


}
