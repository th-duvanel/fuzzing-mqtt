#ifndef UTIL_H
#define UTIL_H

#include <config.h>

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define EXIT_UTIL 10

#define testBit(value, bit) (value & (1 << bit))

/*
Used to print a byte in binary representation
Usage: printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(foo))
*/
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)     \
  (byte & 0x80 ? '1' : '0'),     \
      (byte & 0x40 ? '1' : '0'), \
      (byte & 0x20 ? '1' : '0'), \
      (byte & 0x10 ? '1' : '0'), \
      (byte & 0x08 ? '1' : '0'), \
      (byte & 0x04 ? '1' : '0'), \
      (byte & 0x02 ? '1' : '0'), \
      (byte & 0x01 ? '1' : '0')

/*
strncatM()
Implementation of strncat that does not override '\0' characters
at dest string. For this, there's a startPoint argument (inclusive),
rather than searching for the first '\0' to override.

This implementation does not put a '\0' character at the end of the
dest string.
*/
char *strncatM(char *dest, char *src, int n, size_t startPoint);

/*
checkSize()
Checks if a specific size has at least the expected size.
if it does not have, exit with errorCode is called.
*/
void checkSize(int size, int expectedSize, int errorCode);

/*
setCleanUpHook()
Makes the overload of the action fired when SIGINT (ctrl + c)
is fired. The overload will clean up all the temporary files
created when the broker was running.
*/
void setCleanUpHook();

/*
resetCleanUpHook()
Resets the overload done by setCleanUpHook.
Must be called for fork() processes.
*/
void resetCleanUpHook();

/*
getOrCreateTopicFile()
Returns the file descriptor associated with the specific topic
pipe. If the topic folder or the specific pipe does not exist,
they'll be created.
*/
int getOrCreateTopicFile(char *topicPath, mode_t mode);
#endif