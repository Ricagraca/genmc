#include"SystemCallMapper.hpp"

SystemCallMapper::SystemCallMapper(){
    load = false;
}

std::string SystemCallMapper::systemCallToString(SystemCallKind sck){

  switch(sck){

  case READ:
    return "read";

  case WRITE:
    return "write";

  case OPEN:
    return "open";

  case CLOSE:
    return "close";

  case STAT:
    return "stat";

  case FSTAT:
    return "fstat";

  case LSTAT:
    return "lstat";

  case POLL:
    return "poll";

  case LSEEK:
    return "lseek";

  case BRK:
    return "brk";
    
  case MMAP:
    return "mmap";

  case MPROTECT:
    return "mprotect";

  case MUNMAP:
    return "munmap";

  case LOCTL:
    return "loctl";

  case PREAD64:
    return "pread64";

  case PWRITE64:
    return "pwrite64";

  case READV:
    return "readv";

  case WRITEV:
    return "writev";

  case ACCESS:
    return "access";

  case PIPE:
    return "pipe";

  case SELECT:
    return "select";

  case DUP:
    return "dup";

  case DUP2:
    return "dup2";
    
  case EXECVE:
    return "execve";

  case ARCH_PRCTL:
    return "arch_prctl";
    
  case EXIT_GROUP:
    return "exit_group";

  case OPENAT:
    return "openat";

  case NONE:
    return "none";
    
  }
  
}

void SystemCallMapper::hardCode(){
  u = {
       {0,READ},
       {1,WRITE},
       {2,OPEN},
       {3,CLOSE},
       {4,STAT},
       {5,FSTAT},
       {6,LSTAT},
       {7,POLL},
       {8,LSEEK},
       {9,MMAP},
       {10,MPROTECT},
       {11,MUNMAP},
       {12,BRK},
	 
       {16,LOCTL},
       {17,PREAD64},
       {18,PWRITE64},
       {19,READV},
       {20,WRITEV},
       {21,ACCESS},
       {22,PIPE},
       {23,SELECT},
						    
       {32,DUP},
       {33,DUP2},
       {59,EXECVE},

       {158,ARCH_PRCTL},
       {231,EXIT_GROUP},
	 
       {257,OPENAT}
  };
}


void SystemCallMapper::loadSystemCallTable(){
    hardCode();
}

SystemCallKind SystemCallMapper::idSysCallToMapper(int id){

    // Load if not loaded yet
    if(!load){
      loadSystemCallTable();
    }

    if(u.find(id) != u.end())
      return u[id];

    return NONE;
    
}
