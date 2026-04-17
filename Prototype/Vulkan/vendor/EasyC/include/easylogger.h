#ifndef EASYLOGGER_H
#define EASYLOGGER_H

#define EZ_RESET "\033[0m"
#define EZ_RED "\033[31m"
#define EZ_BLUE "\033[34m"
#define EZ_GREEN "\033[32m"
#define EZ_YELLOW "\033[33m"
#define EZ_PURPLE "\033[35m"
#define EZ_CYAN "\033[36m"

#ifndef PROD_BUILD

#include <stdio.h>
#include <stdlib.h>

#define EZ_INFO(...)  {printf("%s[INFO]%s  ", EZ_GREEN, EZ_RESET);  printf(__VA_ARGS__); printf("\n");}
#define EZ_ERROR(...) {printf("%s[ERROR]%s Error detected in %s:%d - \"", EZ_RED, EZ_RESET, __FILE__, __LINE__); printf(__VA_ARGS__); printf("\"\n");}
#define EZ_FATAL(...) {printf("%s[FATAL]%s Critical failure in %s:%d - \"", EZ_RED, EZ_RESET, __FILE__, __LINE__); printf(__VA_ARGS__); printf("\"\n"); exit(1);}
#define EZ_WARN(...)  {printf("%s[WARN]%s  ", EZ_YELLOW, EZ_RESET); printf(__VA_ARGS__); printf("\n");}
#define EZ_DEBUG(...) {printf("%s[DEBUG]%s ", EZ_BLUE, EZ_RESET);   printf(__VA_ARGS__); printf("\n");}
#define EZ_CUSTOM(precursor, ...) {printf("%s[%s]%s  ", EZ_CYAN, precursor, EZ_RESET);   printf(__VA_ARGS__); printf("\n");}
#define EZ_SCAN(...)  {printf("%s[INPUT]%s ", EZ_PURPLE, EZ_RESET); scanf(__VA_ARGS__);}
#define EZ_ASSERT(x, ...) if (!(x)) { printf("%s[FAIL]%s  Assertion failed in %s:%d - \"", EZ_RED, EZ_RESET, __FILE__, __LINE__); printf(__VA_ARGS__); printf("\"\n"); exit(1);}

#else

#define EZ_INFO(...) ((void) 0)
#define EZ_ERROR(...) ((void) 0)
#define EZ_FATAL(...) ((void) 0)
#define EZ_WARN(...) ((void) 0)
#define EZ_DEBUG(...) ((void) 0)
#define EZ_CUSTOM(precursor, ...) ((void) 0)
#define EZ_SCAN(...) ((void) 0)
#define EZ_ASSERT(x, ...) ((void) 0)

#endif

#endif
