#include"SystemCall.hpp"

SystemCall::SystemCall(pid_t pid){

  SystemCallMapper M;
  int systemcall;

  // Get system call id
  systemcall = ptrace(PTRACE_PEEKUSER, pid, sizeof(long)*ORIG_RAX);

  this->child = pid;
  this->id = systemcall;
  this->name = M.idSysCallToMapper(this->id);
  this->syscall = M.systemCallToString(this->name);   
}

SystemCall::SystemCall(pid_t pid, unsigned int operation_id, unsigned thread_id){

  SystemCallMapper M;
  int systemcall;

  // Get system call id
  systemcall = ptrace(PTRACE_PEEKUSER, pid, sizeof(long)*ORIG_RAX);
  
  this->child = pid;
  this->operation_id = operation_id;
  this->thread_id = thread_id;
  this->id = systemcall;
  this->name = M.idSysCallToMapper(this->id);
  this->syscall = M.systemCallToString(this->name);   
}


SystemCall::SystemCall(pid_t child, int id, SystemCallKind name, string syscall){
  this->child = child;
  this->id = id;
  this->name = name;
  this->syscall = syscall;
}

bool SystemCall::isSet(){
  return this->name != NONE;
}

static bool isFileSystemCall(SystemCallKind sck){
  return sck == READ || sck == WRITE;
}

FileSystemCall::FileSystemCall(SystemCall s) : SystemCall(s.getProcessChild(), s.id, s.name, s.syscall) {
  // Get file descriptor from the system call 
  user_regs_struct uregs = getRegisters(s.getProcessChild());
  this->fd = uregs.rdi;
}

WriteSystemCall::WriteSystemCall(SystemCall s) : FileSystemCall(s) {
  user_regs_struct uregs = getRegisters(s.getProcessChild());
  unsigned long len = uregs.rdx;
  unsigned long addr = uregs.rsi;
  this->length = len;
  this->address = addr;
  this->value = getBufferValue(s.getProcessChild());
}

// Translate the write system call to the sequence of events!
vector<EventLabel> WriteSystemCall::systemCallToListOfEvents(Count c) {

  /* Sequence of events:
     Lock on file descriptor, and lock on the respective inode 
     I do not know how can I get the specific inode :(
  */

  /* A write system call starts by calling the lock event
     on the file descriptor */

  // Event e(c,this->thread_id,this->operation_id);
  // EventLabel el();

  
}

ostream& operator << (ostream &os, WriteSystemCall wsc) {
  return (os <<
	  wsc.syscall << "(" << wsc.fd << ", " << wsc.address << ", " << wsc.length << ")");
}
