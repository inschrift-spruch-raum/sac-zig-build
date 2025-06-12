#include "/api/cli.h"

#include <span>

int main(int argc,const char* argv[]) {
  Shell Shell;
  Shell.SACInfo();
  int error = Shell.Parse(std::span<const char*>(argv, argc));
  if(error == 0) error = Shell.Process();
  return error;
}