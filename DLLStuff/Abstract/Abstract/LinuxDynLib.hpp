#ifndef _LINUXDYNLIB_HPP_
#define _LINUXDYNLIB_HPP_

#ifdef __linux__
#include "IDynLib.hpp"
#include <dlfcn.h>
#include <stdio.h>
#include <iostream>

class LinuxDynLib : public IDynLib
{
private:
  //int flagOpen;
  void* HandleOpen;
  std::string libName;
  std::string SymbolName;
public:
  LinuxDynLib();
  ~LinuxDynLib();
  virtual void* openLib();
  virtual void* dlSymb();
  virtual int closeLib();
  virtual char* errorLib();
  //int getflagOpen();
  //void setflagOpen(int);
  void* getHandleOpen();
  void setHandleOpen(void*);
  std::string getlibName();
  void setlibName(std::string);
  std::string getSymbolName();
  void setSymbolName(std::string);
};
#endif

#endif
