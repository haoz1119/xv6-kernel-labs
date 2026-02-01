#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  char buffer[512];

  if(getlastcat(buffer) < 0) {
    exit();
  }

  printf(1, "XV6_TEST_OUTPUT Last catted filename: %s\n", buffer);

  exit();
}
