#ifndef _UTIL_H
#define _UTIL_H

int get_kb_shift(void);
void del_arg(int argc, char **argv, int index);
int find_arg(int argc, char* argv[], char *arg);
int find_int_arg(int argc, char **argv, char *arg, int def);
float find_float_arg(int argc, char **argv, char *arg, float def);
char *find_char_arg(int argc, char **argv, char *arg, char *def);

#endif