#ifndef __INTERCEPT_HPP__
#define __INTERCEPT_HPP__

#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include<iostream>

#include"CallBack.hpp"

using namespace std;

class Intercept {

private:
  
  int wait_for_syscall(pid_t child);
  void child_call(int argc, char **argv);
  void parent_call(pid_t child);

  Callback *cb;
  
public:

  Intercept(Callback *callback);  
  void start(int argc, char **argv);

};

#endif
