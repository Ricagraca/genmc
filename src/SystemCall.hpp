#ifndef __SYSTEMCALL_HPP__
#define __SYSTEMCALL_HPP__

#include"SystemCallMapper.hpp"
#include"EventLabel.hpp"
#include"Trace.hpp"

#include"types/Count.hpp"
#include"types/SystemCallTypes.hpp"

#include <sys/reg.h>
#include <sys/ptrace.h>

#include<vector>
#include<cassert>

using namespace std;

class SystemCall{

protected:
  pid_t child;
  
public:
  int id;
  SystemCallKind name;
  string syscall;
  
  unsigned int operation_id;
  unsigned int thread_id;
  
  virtual ~SystemCall() = default;
  pid_t getProcessChild(){ return child; }
  SystemCall(pid_t child, int id, SystemCallKind name, string syscall); // Constructor with all the elements
  SystemCall(pid_t child, unsigned int operation_id, unsigned thread_id);
  SystemCall(pid_t child);
  bool isSet();
  
  // This is the most important function
  vector<EventLabel> systemCallToListOfEvents(); 
  
};

// TODO: Create a file system call class

/*
-----------------------------------------

FILE SYSTEM CALL CLASS 

-----------------------------------------
*/


class FileSystemCall : public SystemCall {

private:

  static bool isFileSystemCall(SystemCallKind sck);

  
public:

  unsigned int fd;

  virtual ~FileSystemCall() = default;
  FileSystemCall(SystemCall s); 
  
};


/*
-----------------------------------------

WRITE SYSTEM CALL CLASS 

-----------------------------------------
*/

class WriteSystemCall : public FileSystemCall {

public:

  unsigned long address;
  unsigned long length;
  string value;
  
  virtual ~WriteSystemCall() = default;
  WriteSystemCall(SystemCall systemcall);

  // We just assume that the system call is a certain sequence of events
  vector<EventLabel> systemCallToListOfEvents(Count counter); 

};


// Put the ostream for all system call
ostream& operator << (ostream &os, WriteSystemCall wsc);

#endif
