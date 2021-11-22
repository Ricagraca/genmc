#include"Trace.hpp"

user_regs_struct getRegisters(pid_t pid){
  user_regs_struct uregs;
  ptrace(PTRACE_GETREGS, pid, 0, &uregs);
  return uregs;  
}

string getBufferValue(pid_t pid){

  std::stringstream ss;
  user_regs_struct uregs;

  ptrace(PTRACE_GETREGS, pid, 0, &uregs);

  unsigned long len = uregs.rdx;
  unsigned long addr = uregs.rsi;
    
  for(int i=0 ; i<len ; i+=2){
    uint16_t a = ptrace(PTRACE_PEEKDATA, pid, addr+i, 0);
    char first_byte = (char)((a << 8) >> 8);
    char second_byte = (char)(a >> 8);
    ss << first_byte;
    ss << second_byte;
  }

  return ss.str();

}

