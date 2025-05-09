
## part2_printPgtbl

```c
void vmprint_helper(pagetable_t p,int level){
  for(int i = 0; i < 512; i++){
    pte_t pte = p[i];
    if(pte & PTE_V){
      uint64 child = PTE2PA(pte);
      if(level == 1){
        printf("..%d: pte %p pa %p\n", i, pte, child);
      }else if(level == 2){
        printf(".. ..%d: pte %p pa %p\n", i, pte, child);
      }else if(level == 3){
        printf(".. .. ..%d: pte %p pa %p\n", i, pte, child);
      }

      if((pte & (PTE_R|PTE_W|PTE_X)) == 0)  // if not mapping to pmem directly
        vmprint_helper((pagetable_t)child, level + 1);
    }
  }
}
void vmprint(pagetable_t p){
  int level = 1;
  printf("page table %p\n", p);
  vmprint_helper(p, level);
}
```
## part3_pgaccess

```c
int
sys_pgaccess(void)
{
  uint64 start_va;
  int num;
  uint64 buffer;
  uint64 bitmask = 0;

  if (argaddr(0, &start_va) < 0 || argint(1, &num) < 0 || argaddr(2, &buffer) < 0) {
    return -1;
  }
  if(num < 0 || num > 64){
    return -1;
  }
  if(start_va > MAXVA || start_va + num * PGSIZE > MAXVA) {
    return -1;
  }
  pagetable_t pagetable= myproc()->pagetable;

  for(int i = 0; i < num; i++){
    uint64 va = start_va + i * PGSIZE;   // address to next continuous page;
    pte_t *pte = walk(pagetable, va, 0);
    if(pte && (*pte & PTE_V) && (*pte & PTE_A)){
      bitmask |= (1 << i);
      *pte &= (~PTE_A);
    }

  }
  if(copyout(pagetable, buffer, (char*)&bitmask, sizeof(bitmask)) < 0){
    return -1;
  }
  return 0;
}
```