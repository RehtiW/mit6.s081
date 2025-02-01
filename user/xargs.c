#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

// echo "test" | xargs echo
int main(int argc, char **argv){
    if (argc < 2) { // 检查参数合法
        fprintf(2, "Usage: xargs <command> [args...]\n");
        exit(1);
    }

    char *final_argv[MAXARG + 1];
    int final_argc = 0;
    char line[512];

    for(int i = 1; i < argc; i++){
        final_argv[final_argc++] = argv[i];
    }
    

    int n;
    while((n = read(0,line,sizeof(line))) > 0){ // 1 byte / char
        int line_pos = 0;
        while(line_pos < n){
            int line_end = line_pos;
            while(line_end < n && line[line_end] != '\n')   //定位行尾
                line_end ++;

            int arg_len =  line_end - line_pos;    // 排除'\n'在外
            char current_arg[512];
            memmove(current_arg, &line[line_pos], arg_len);
            current_arg[arg_len] = '\0';    // 确保字符串终止

            if (final_argc >= MAXARG) {
                fprintf(2, "xargs: too many arguments\n");
                exit(1);
            }          
            final_argv[final_argc++] = current_arg;
            line_pos = line_end + 1;

            // 子进程执行，每行执行一次
            if(fork() == 0){
                final_argv[final_argc] = 0; // 确保参数以 NULL 结尾
                exec(final_argv[0],final_argv);
                fprintf(2,"exec %s failed\n", final_argv[0]);
                exit(1);
            }else{
                wait(0);
                final_argc = argc - 1;  // 重置参数数量变量,仍然排除"xargs"
            }
        }
        
    }
    exit(0);
}

