#include <stdio.h>
#include <unistd.h>

static void win(void) {
  puts("[win] Control flow hijacked. Flag would be printed here in a real CTF.");
}

int main(void) {
  char buf[64];

  puts("vuln_target loaded. Send up to 256 bytes:");
  ssize_t n = read(STDIN_FILENO, buf, 256);
  if (n < 0) {
    perror("read");
    return 1;
  }

  puts("Thanks for your input.");

  if (buf[0] == '!') {
    win();
  }

  return 0;
}
