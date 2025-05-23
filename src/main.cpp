#include "/entrance/shell.h"

#include <span>

int main(int argc, char* argv[]) {
  Shell Shell;
  Shell.SACInfo();
  int error = Shell.Parse(std::span<char*>(argv, argc));
  if(error == 0) error = Shell.Process();
  return error;
}