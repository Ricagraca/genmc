#include"Intercept.hpp"

int Intercept::wait_for_syscall(pid_t child) {
  int status;
      
  while (1) {
    ptrace(PTRACE_SYSCALL, child, 0, 0);
    waitpid(child, &status, 0);
    if (WIFSTOPPED(status) && WSTOPSIG(status) & 0x80)
      return 0;
    if (WIFEXITED(status))
      return 1;
  }
}

void Intercept::child_call(int argc, char **argv){
  char *args[argc+1];

  memcpy(args, argv, argc * sizeof(char*));
  args[argc] = NULL;
  ptrace(PTRACE_TRACEME, 0, 0, PTRACE_O_TRACEFORK|PTRACE_O_TRACEVFORK|PTRACE_O_TRACECLONE);
  kill(getpid(), SIGSTOP);
    
  execvp(args[0], args);
}

void Intercept::parent_call(pid_t child){
  int status, syscall, retval;

  waitpid(child, &status, 0);
  ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD);
    
  while(1) {
    if (wait_for_syscall(child) != 0) break;
    cb->before(child);
      
    if (wait_for_syscall(child) != 0) break;
      
    cb->after(child);
  }

}

Intercept::Intercept(Callback *callback){
  cb = callback;
}
  
void Intercept::start(int argc, char **argv){
  pid_t child = fork();

  if(child == 0){
    child_call(argc -1, argv +1);
  }
  else{
    parent_call(child);
  }
}
