#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int main(int argc,char **argv){
    if(argc > 1){
        fprintf(2,"usage: pingpong...\n");
        exit(1);
    }
    int p1[2];
    int p2[2];
    char buf[1];
    pipe(p1);
    pipe(p2);
    
    if(fork() == 0){
        close(p1[1]);
        close(p2[0]);

        read(p1[0],buf,1);
        write(p2[1],buf,1);
        printf("%d: received ping\n", getpid());
    }else{
        close(p1[0]);
        close(p2[1]);

        write(p1[1],"x",1);
        read(p2[0],buf,1);
        wait(0);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}