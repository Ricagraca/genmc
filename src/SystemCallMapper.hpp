#ifndef __SYSTEMCALLMAPPER_HPP__
#define __SYSTEMCALLMAPPER_HPP__

#include<unordered_map>
#include"types/SystemCallTypes.hpp"

/*

  TODO: Generalize the table for all OS

*/

class SystemCallMapper{
  
public:
  
  SystemCallMapper();
  
  // Let us just load from hardcode now
  void loadSystemCallTable();
  SystemCallKind idSysCallToMapper(int id);
  std::string systemCallToString(SystemCallKind sck);

private:
  bool load;
  std::unordered_map<int, SystemCallKind> u;

  // Hardcoded for x86 for now and only for file system calls
  void hardCode();
  
};


#endif
