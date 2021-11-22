#ifndef __TRACE_HPP__
#define __TRACE_HPP__

#include <string>       
#include <iostream>  
#include <sstream> 

#include <sys/ptrace.h>
#include <sys/user.h>

using namespace std;

string getBufferValue(pid_t pid) ;
user_regs_struct getRegisters(pid_t pid);

#endif
