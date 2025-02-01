#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


void sieve(int p_left[2]){
    int num;
    int prime; 
    int p_right[2];

    close(p_left[1]);   // close all write side so that read can reach EOF

    if(read(p_left[0],&prime,sizeof(prime)) == 0){
        close(p_left[0]);
        return;
    }
    printf("prime %d\n", prime);

    pipe(p_right);

    if(fork() == 0){    // child
        close(p_left[0]);
        sieve(p_right);
    }else{              // parent
        close(p_right[0]);
        while(read(p_left[0],&num,sizeof(num))){    //read from left
            if(num % prime != 0)
                write(p_right[1],&num,sizeof(num)); // write to right
        }
        close(p_left[0]);
        close(p_right[1]);
        wait(0);
    }
    exit(0);
}
int main(int argc,char **argv){
    int p[2];
    pipe(p);

    if(fork() == 0){
        sieve(p);
    }else{
        close(p[0]);
        for(int i = 2;i<=35;i++){
            write(p[1],&i,sizeof(i));
        }
        close(p[1]);
        wait(0);
    }
    exit(0);

}