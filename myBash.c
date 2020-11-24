#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

struct sigaction FATHERHANDLER;
struct sigaction CHILDHANDLER;
int children[2];
// prepare and finalize calls for initialization and destruction of anything required
void handlerSIGINTFather(int sig){
    if(children[0])
        kill(children[0],SIGKILL);
    if(children[1])
        kill(children[1],SIGKILL);
    children[0]=0;
    children[1]=0;
    FATHERHANDLER.sa_flags=SA_RESTART; // restart father flags;
    CHILDHANDLER.sa_flags =SA_RESTART; // just in case
    exit(1);
}
void handlerSIGINTChild(int sig){
    fprintf(stderr,"handleed\n");
    return ;
}
int prepare(void)
{
    struct sigaction father;
    father.sa_flags=SA_RESTART;
    father.sa_handler=&handlerSIGINTFather;
    FATHERHANDLER=father;
    struct sigaction child;
    child.sa_flags=SA_RESTART;
    child.sa_handler=&handlerSIGINTChild;
    CHILDHANDLER=child;
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
    FATHERHANDLER.sa_flags=SA_RESTART;
    FATHERHANDLER.sa_handler=&handlerSIGINTFather;
    sigaction(SIGINT,&FATHERHANDLER,NULL);
    CHILDHANDLER.sa_flags=SA_RESTART;
    children[0]=0;
    children[1]=0;
    int fd[2];
    int pipeIndex=0;
    int waitOnChild=0;
    waitOnChild=arglist[count-1]!=NULL && arglist[count-1][0]=='&' && arglist[count-1][1]=='\0';
    pipeIndex=ContainsPipe(arglist,count);
    if( pipeIndex ) printf("Piping\n");
    if(pipeIndex){
        if(pipe(fd)==-1){
            printf("Problem With Pipe\n");
            //exit(2);
        }
    }
    int id2;
    if(pipeIndex){ //piping 
        //blocking SIGINT till we init the handler code taken from 
        //https://stackoverflow.com/questions/25261/set-and-oldset-in-sigprocmask
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask,SIGINT);
        sigprocmask(SIG_BLOCK,&mask,NULL);
        sigaction(SIGINT,&CHILDHANDLER,NULL);
        sigprocmask(SIG_UNBLOCK,&mask,NULL);
        printf("running write pipe \n");
        int id2=fork();
        if(id2==0){       
            sigaction(SIGINT,&CHILDHANDLER,NULL);
            dup2 (fd[1],STDOUT_FILENO);
            close(fd[0]);
            close(fd[1]);
            arglist[pipeIndex]=NULL;
            if(execvp(arglist[0],arglist) <0){
                fprintf(stderr,"request file doesn't exist in left side of pipe\n");
                //exit(2);
            }
        }
       
    }
    children[1]=id2; // always want to be able to kill using SIGINT
    if(pipeIndex){
        printf("\npid: %d ",getpid());
        printf("waiting on write to pipe\n");
        wait(&id2);
        printf(" done waiting\n ");
    } 
    int id1=fork();
    if(id1==0){
        //blocking SIGINT till we init the handler code taken from 
        //https://stackoverflow.com/questions/25261/set-and-oldset-in-sigprocmask
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask,SIGINT);
        sigprocmask(SIG_BLOCK,&mask,NULL);

        raise(SIGINT);
        fprintf(stderr,"here\n");

        sigaction(SIGINT,&CHILDHANDLER,NULL);
        sigprocmask(SIG_UNBLOCK,&mask,NULL);
        if(pipeIndex) pipeIndex++;
        //child
        if(pipeIndex) printf("read Pipe init\n");
        else printf("child init\n");
        if(pipeIndex){  //rerouting stdin
            dup2( fd[0],STDIN_FILENO);
            close(fd[0]);
            close (fd[1]);
        } 
        if(waitOnChild){
              arglist[count-1]=NULL;
        }
        if(pipeIndex) printf("read Pipe running now\n");
        else printf("executing Child\n");
        if( execvp(arglist[pipeIndex],arglist+pipeIndex)<0){
            if(!pipeIndex)fprintf(stderr,"request file doesn't exist right side of pipe");
            else fprintf(stderr,"request fild doesn't exist");
        }
    }
    if(!waitOnChild) children[0]=id1; // kill on SIGINT
    else children[0]=0; // don't kill on SIGINT
    close(fd[0]);
    close(fd[1]);
    if(!waitOnChild){
                printf("Waiting on Children\n");
                wait(NULL);
                printf("Done Waiting\n");
     }
      

    return 1;


}
