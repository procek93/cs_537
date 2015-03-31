#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  char * ptr;
  ptr = NULL;
  printf(1, "%x", *ptr);
  exit();
}
