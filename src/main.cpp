#include "/api/cli.h"

#include <span>

std::int32_t main(std::int32_t argc,const char* argv[]) {
  Shell Shell;
  Shell.SACInfo();
  std::int32_t error = Shell.Parse(std::span<const char*>(argv, argc));
  if(error == 0) { error = Shell.Process();}
  return error;
}