#ifndef CONSOLETEXTEDITOR_UTILS_H
#define CONSOLETEXTEDITOR_UTILS_H



#define NEW(T, n) (T*)alloc(sizeof(T) * n)

#define elog(fmt, ...) fprintf(stderr, fmt "\nError occured at %s:%d\n", ##__VA_ARGS__, __FILE__, __LINE__)
#define log(fmt, ...) fprintf(stdout, fmt " - %s:%d\n", ##__VA_ARGS__, __FILE__, __LINE__)

#endi