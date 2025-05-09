## Large files
背景: xv6文件系统**文件大小上限为268blocks**，即(12个直接块+1个间接块指向的256个块)，此时想要创建一个65803个块的文件
在struct inode结构体中有一个addrs字段，存放了各种块的地址(直接间接块)
```c
struct inode {
	//.......
  uint addrs[NDIRECT+1+1];

};
```
方案:**牺牲一个直接块，添加一个双重间接块**即
前11个是直接块，第12个是间接块，第13个是双重间接块

### 关键函数修改:bmap(inode指针， 逻辑块号bn)
双重间接块的查询方式
通过连续将bn -= (NDIRECT + NINDIRECT) ， 将bn减为双重间接块内索引(0 - 256^2-1)
bn / NINDIRECT = 间接块号
bn % NINDIRECT = 间接块内索引

```c
static uint

bmap(struct inode *ip, uint bn) // 将逻辑块号对应到全局的块地址(块号)

{

  uint addr, *a;

  struct buf *bp;

  // 如果inode指向的第bn块块是直接块

  if(bn < NDIRECT){
	//....
  }
  
  bn -= NDIRECT;  // 获得间接块内索引

  if(bn < NINDIRECT){
    //.....
  }

  bn -= NINDIRECT;

  if(bn < N_DOUBLY_INDIRECT){

    if((addr = ip->addrs[NDIRECT + 1]) == 0){

      ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);

    }

    bp = bread(ip->dev,addr); // 双重简介块缓冲区, 获取缓冲区锁

    struct buf *buf_doubly = bp;

  

    a = (uint*)bp->data; // 指向间接块地址数组

    uint idx_indirect = bn / NINDIRECT;

    if((addr = a[idx_indirect]) == 0){

      a[idx_indirect] = addr = balloc(ip->dev);

      log_write(bp);

    }

    bp = bread(ip->dev, addr); // 间接块缓冲区，获取锁

    a = (uint*)bp->data;  // 指向 间接块的块地址数组

    uint idx_block = bn % NINDIRECT;

    if((addr = a[idx_block]) == 0){

      a[idx_block] = addr = balloc(ip->dev);

      log_write(bp);

    }

    brelse(bp);

    brelse(buf_doubly);

    return addr;

  }

  panic("bmap: out of range");

}
```

## Symbolic links
需求:实现sys_symlink系统调用，用于创建一个symbolic link类型文件，该文件的内容为指向的文件的路径名，即inode的addr字段指向的数据块存储的内容为char数组。
使用到的函数:
argstr():将传入系统调用的参数转化为\0结尾的字符串
writei():将数据写入数据块并让inode指向该数据块
readi():通过inode读取数据块内容
```c
uint64

sys_symlink(void){  // 创建一个symlink文件在path中,内容为target路径

  char target[MAXPATH];// 失误处: char *target = 0;将target设置为空指针

  char path[MAXPATH];// 导致argstr在执行时往地址0x0写入，触发内核panic

  struct inode *ip;

  int n;

  if((n = argstr(0, target, MAXPATH)) < 0)

    return -1;

  if(argstr(1, path, MAXPATH) < 0)

    return -1;

  

  begin_op();

  if((ip = create(path, T_SYMLINK, 0, 0)) == 0){

    end_op();

    return -1;

  }

  // 数据写入inode

  if(writei(ip, 0, (uint64)target, 0, n) != n){

    iunlockput(ip);

    end_op();

    return -1;

  }

  iunlockput(ip);

  end_op();

  return 0;

  

}
```
修改sys_open以支持symlink文件，对于循环链接，通过最大递归次数来限制
使用到的函数:
namei(char*):传入路径名，获得对应struct *inode
```c
struct inode* symlink_open_helper(struct inode *ip, int rt){

  if(rt == 11)

    return 0;

  char target[MAXPATH];

  struct inode *nextinode;

  if(readi(ip, 0, (uint64)target, 0, sizeof(target)) < 0){

    iunlock(ip);

    return 0;

  }

  iunlock(ip);// 释放父节点

  if((nextinode = namei(target)) == 0){ // hold nextinode's lock

    return 0;

  }

  ilock(nextinode);

  if(nextinode->type == T_SYMLINK){

    return symlink_open_helper(nextinode, rt + 1);

  }

  return nextinode;

}

  

uint64

sys_open(void)

{

  char path[MAXPATH];

  int fd, omode;

  struct file *f;

  struct inode *ip;

  int n;

  

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)

    return -1;

  

  begin_op();

  // 获取inode

  if(omode & O_CREATE){ // create标识符

    ip = create(path, T_FILE, 0, 0);  // create自动上锁

    if(ip == 0){

      end_op();

      return -1;

    }

  } else {

    if((ip = namei(path)) == 0){

      end_op();

      return -1;

    }

    ilock(ip);    // 手动上锁

    if(ip->type == T_DIR && omode != O_RDONLY){

      iunlockput(ip);

      end_op();

      return -1;

    }

  }

  

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){

    iunlockput(ip);

    end_op();

    return -1;

  }

  

  // When a process specifies O_NOFOLLOW in the flags to open,

  // open should open the symlink (and not follow the symbolic link).

  if(!(omode & O_NOFOLLOW) && ip->type == T_SYMLINK){  

    struct inode *target_inode;

    if((target_inode = symlink_open_helper(ip,0)) == 0){

      iput(ip);

      end_op();

      return -1;

    }

    ip = target_inode;

  }


  // ...分配fd并返回
  iunlock(ip);

  end_op();

  

  return fd;

}
```