#include"SystemCallFactory.hpp"
#include<iostream> // TODO: Delete afterwards

SystemCall* SystemCallFactory(pid_t pid){

  SystemCall *S = new SystemCall(pid);

  /* TODO: unset in the future when all system call ids are up
  if(!S->isSet())
  assert(0); */

  switch(S->name){

  case WRITE :
    return new WriteSystemCall(*S);

    // Should not reach this point
  default:
    return S;
  }
  
}
