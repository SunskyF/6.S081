#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  char *xargv[MAXARG];
  int xargc = argc;
  for (int i = 0; i < argc; i++)
  {
    xargv[i] = argv[i];
  }

  char c;
  char buf[MAXARG];
  int idx = 0;
  while (1)
  {
    read(0, &c, 1);
    if (c == ' ' || c == '\n')
    {
      if (idx == 0) break;
      char *tmp = (char *)malloc(idx * sizeof(char));
      memcpy(tmp, buf, idx);
      tmp[idx] = 0;
      xargv[xargc++] = tmp;
      idx = 0;
      if (c == '\n')
      {
        if (fork() == 0)
        {
          xargv[xargc] = 0;
          exec(xargv[1], xargv + 1);
          exit(0);
        }
        else
        {
          wait(0);
          xargc = argc;
        }
      }
    }
    else
    {
      buf[idx++] = c;
    }
  }
  exit(0);
}