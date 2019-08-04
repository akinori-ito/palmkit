#include <stdio.h>

int main()
{
  printf("#ifndef INTSIZE_H\n");
  printf("#define INTSIZE_H\n");
  printf("typedef unsigned short uint2;\n");
  if (sizeof(unsigned long) == 4) {
    printf("typedef unsigned long uint4;\n");
    printf("typedef long int4;\n");
  }
  else if (sizeof(unsigned int) == 4) {
    printf("typedef unsigned int uint4;\n");
    printf("typedef int int4;\n");
  }
  if (sizeof(char*) == sizeof(int)) {
    printf("typedef int ptr_int;\n");
    printf("typedef unsigned int u_ptr_int;\n");
  }
  else if (sizeof(char*) == sizeof(long)) {
    printf("typedef long ptr_int;\n");
    printf("typedef unsigned long u_ptr_int;\n");
  }
  else {/* hope it helps */
    printf("typedef int ptr_int;\n");
    printf("typedef unsigned int u_ptr_int;\n");
  }
  printf("#endif\n");
  return 0;
}
