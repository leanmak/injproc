#ifndef UTILS_H
#define UTILS_H

#define COMM_BUF 64
#define INPUT_BUF 128
#define INFO_BUF 128
#define DIR_BUF 64
#define FD_STR_BUF 2048
#define MESSAGE_BUF 512

void select_process();
void list_processes();
void view_process_information(int pid);
bool process_exists(int pid);
char* get_process_name(int pid);
void open_process(int pid);
void send_signal(int pid);
char* fetch_process_information_rl(int pid, char* sub_dir);
char* fetch_information_st(int pid, char *info);
char* fetch_file_descriptors(int pid);
void inject(int pid);

#endif