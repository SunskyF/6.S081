#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void generate(int port)
{
  int p, n;
  if (read(port, &p, 4) == 0) {
    return;
  }
  printf("prime %d\n", p);

  int pip[2];
  pipe(pip);

  if (fork() == 0)
  {
    close(pip[1]);
    generate(pip[0]);
    close(pip[0]);
  }
  else
  {
    close(pip[0]);
    while (1)
    {
      if (read(port, &n, 4) == 0)
        break;
      if (n % p != 0)
      {
        write(pip[1], &n, 4);
      }
    }
    close(pip[1]);
    wait(0);
  }
}

int main(int argc, char *argv[])
{
  int pip[2];
  pipe(pip);

  if (fork() == 0)
  {
    close(pip[1]);
    generate(pip[0]);
    close(pip[0]);
  }
  else
  {
    close(pip[0]);
    for (int i = 2; i <= 35; i++)
    {
      write(pip[1], &i, 4);
    }
    close(pip[1]);
    wait(0);
  }
  exit(0);
}
