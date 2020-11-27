#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

sigset_t set;

int prepare(void)
{
    struct sigaction ZombieHandler;
	ZombieHandler.sa_handler = SIG_DFL;
	ZombieHandler.sa_flags = SA_NOCLDWAIT;
	if (sigaction(SIGCHLD, &ZombieHandler, NULL) != 0) { //override the default handling for the SIGCHLD to stop children from becoming zombies
        // if you are a zombie and don't say anything, you are the problem, much like covid.
		fprintf(stderr, "error occurred sigaction\n ");
        return 1;
	}
    struct sigaction ignoreHandler;
	ignoreHandler.sa_handler = SIG_IGN;
	ignoreHandler.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &ignoreHandler, NULL) != 0) { //override the default handling for SIGINT to ignoring it
		fprintf(stderr, "error occurred sigaction \n");
                return 1;

	}
    if(sigemptyset(&set)!=0){
        fprintf(stderr,"error occured in sigemptyset\n");
                return 1;

    }
    if(sigaddset(&set,SIGINT)!=0){
        fprintf(stderr,"error occured in sigaddset\n");\
                return 1;

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

int process_arglist(int count, char** arglist){
    // we need a handler for the children running in the foreground, a little something for them not being shy
    struct sigaction NORMALHANDLER; 
    NORMALHANDLER.sa_flags=SA_RESTART;
    NORMALHANDLER.sa_handler=SIG_DFL; 
    int fd[2];
    int status=-1;
    int pipeIndex=0;
    int waitOnChild=0;
    // checking for & as the last word
    waitOnChild=arglist[count-1]!=NULL && arglist[count-1][0]=='&' && arglist[count-1][1]=='\0';
    pipeIndex=ContainsPipe(arglist,count);
    if(pipeIndex){
        if(pipe(fd)==-1){ // 
            printf("likes like uncle Joe clogged the pipe again\n");
            return 0;
        }
    }
    int id2=0;
    if(pipeIndex){ // pipe was detected
        if(sigprocmask(SIG_BLOCK,&set,NULL)==-1){
            fprintf(stderr,"error blocking signal write side of pipe\n");
            return 0;
        }
        id2=fork();
        if(id2==0){// code for the write side of our pipe
	        if(sigaction(SIGINT,&NORMALHANDLER,NULL)==-1){// good child, exists on SIGINT.
                fprintf(stderr,"error occured in sigaction: writeside of pipe\n");
                exit(1);
            } 
            if(sigprocmask(SIG_UNBLOCK,&set,NULL)==-1){
                fprintf(stderr,"error blocking signal write side of pipe\n");
                exit(1);
            }
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
   
    if(!pipeIndex) sigprocmask(SIG_BLOCK,&set,NULL);
    int id1=fork();
    if(id1==0){ // code for either the passed function or the read side of our pipe
    // good child exists on SIGINT, otherwise we don't change the handler and the child acts like the parent.
        return 1;
        if(!waitOnChild) sigaction(SIGINT,&NORMALHANDLER,NULL); 
        if(sigproctmask(SIG_UNBLOCK,&set,NULL)==-1){
            fprintf(stderr,"problem occured with sigprocmask: UNBLOCK CHILD\n");
            exit(1);
        }
        sleep (200);
        if(pipeIndex) pipeIndex++;
        if(pipeIndex){  //rerouting stdin
            dup2( fd[0],STDIN_FILENO);
            close(fd[0]);
            close (fd[1]);
        } 
        if(waitOnChild){ // & was found and need to be deleted
              arglist[count-1]=NULL;
        }
        if( execvp(arglist[pipeIndex],arglist+pipeIndex)==0){
            if(pipeIndex)fprintf(stderr,"request file doesn't exist read side of pipe");
            else fprintf(stderr,"request file doesn't exist");
	        exit(1);
        }
    }
    sigprocmask(SIG_UNBLOCK,&set,NULL);
    if(pipeIndex){
	    close(fd[0]);
    	close(fd[1]);
	while(1){
        	if(wait(&status)==-1){
			if (errno != ECHILD) {// because we changed the SIGCHILD handle
			fprintf(stderr,"error occured in wait\n");
			return 0;
			}
		}
		break;
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
