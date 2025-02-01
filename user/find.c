#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path);
void find(char *path, char *filename);

int main(int argc,char *argv[]){
    if(argc < 3){
        fprintf(2,"missing argument, usage: find <path> <filename>\n");
        exit(1);
    }
    char buf[512];
    strcpy(buf,argv[1]);
    for(int i = 2;i<argc;i++){
        find(argv[1], argv[i]);
    }
    exit(0);
}
char* fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--);
    
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void find(char *path, char *filename){
    char *c;
    int fd;
    struct stat st;
    struct dirent dr;
    char buf[512];
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type){
    case T_FILE:
        char *file;
        file = fmtname(path);
        if(strcmp(file,filename) == 0)
            printf("%s\n", path);   // if is file, print path
        break;
        
    case T_DIR:
        strcpy(buf,path);
        c = (buf+strlen(path));
        *(c++) = '/';
        while(read(fd,&dr,sizeof(dr)) == sizeof(dr)){   // traverse dir
            if(dr.inum == 0 || dr.inum == 1 || strcmp(dr.name, ".") == 0 || strcmp(dr.name, "..") == 0)
                continue;
            if(strcmp(dr.name, filename) == 0){      
                printf("%s/%s\n",path,dr.name); // print path/dirname
            }
            memmove(c, dr.name,DIRSIZ);
            c[DIRSIZ] = 0;  // Ensure the string is null-terminated
            if(stat(buf,&st) == 0 ){
                if(st.type == T_DIR)
                    find(buf,filename);     // if still a dir,dfs
            }else{
                fprintf(2, "find: cannot stat %s\n", buf);
            }
            
        }
        break;
    }
    close(fd);
}