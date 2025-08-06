#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils.h"
#include <signal.h>

// Allows the user to manipulate the selected process.
void open_process(int pid) {
    printf("\033[H\033[J");

    char *p_name = get_process_name(pid);

    if(!p_name) {
        perror("Failed trying to get process ID.");
        return;
    }

    bool input_error = true;

    while(input_error) {
        char choice[INPUT_BUF];

        printf("\n--- Currently Viewing Process: %s (PID: %d) ---", p_name, pid);
        printf("\na) view process information\nb) send signal\nc) inject into file descriptor\nd) go back\n\nWhat would you like to do: ");
        scanf("%s", choice);

        input_error = false;
        if(strcasecmp(choice, "a") == 0) {
            view_process_information(pid);
        } else if(strcasecmp(choice, "b") == 0) {
            send_signal(pid);
        } else if(strcasecmp(choice, "c") == 0) {
            inject(pid);
        } else if(strcasecmp(choice, "d") == 0) {
            select_process();
        } else {
            input_error = true;
            printf("\n");
        }
    }
    
    free(p_name);
}

// Writes to any file descriptor
void inject(int pid) {
    printf("\n--- Which file descriptor would you like to write to? ---");
    printf("\nChoice: ");
    
    int fd;
    scanf("%d", &fd);

    char fd_dir[DIR_BUF];
    snprintf(fd_dir, DIR_BUF, "/proc/%d/fd/%d", pid, fd);

    FILE *fd_file = fopen(fd_dir, "a");
    if(!fd_file) {
        perror("Failed to open file.");
        return;
    }

    // clear input buffer
    while ((getchar()) != '\n');  

    char message[MESSAGE_BUF];

    printf("\nWhat would you like to write to the file descriptor: ");
    scanf("%99[^\n]", message);

    // clear input buffer
    while ((getchar()) != '\n');  

    int end = strcspn(message, "\0");
    message[end] = '\n';
    message[end+1] = '\0';

    fputs(message, fd_file);

    printf("\nWrote to file descriptor!");

    fclose(fd_file);

    char input;
    printf("\nPress [ENTER] to continue...");
    scanf("%c", &input);

    open_process(pid);
}

// Asks the user to select a process and verifies it.
void select_process() {
    printf("\033[H\033[J");
    bool picked_process = false;
    int pid;

    while(!picked_process) {
        printf("\nEnter a process ID (-1 to see all process ID's): ");
        scanf("%d", &pid);

        if(pid == -1) {
            list_processes();
            continue;
        }

        if(!process_exists(pid)) {
            printf("\nInvalid process submitted.");
            continue;
        }

        picked_process = true;
    }

    open_process(pid);
}

// Lists all processes (Name & PID)
void list_processes() {
    printf("\033[H\033[J");
    // Read all processes (folders) in /proc
    DIR *proc = opendir("/proc");

    if(!proc) {
        perror("Failed to read /proc directory.");
        return;
    }

    struct dirent* entry;
    while((entry = readdir(proc)) != NULL) {
        if(isdigit((unsigned char)entry->d_name[0])) {
            int pid = atoi(entry->d_name);
            char* p_name = get_process_name(pid);

            if(p_name) {
                printf("- %s (PID: %d)\n", p_name, pid);
                free(p_name);
            }
        }
    }

    closedir(proc);
}

// Maps a character option to the corresponding signal number
int signal_from_option(char option) {
    switch (option) {
        case 'a': return SIGTERM;
        case 'b': return SIGKILL;
        case 'c': return SIGINT;
        case 'd': return SIGHUP;
        case 'e': return SIGSTOP;
        case 'f': return SIGCONT;
        case 'g': return SIGUSR1;
        case 'h': return SIGUSR2;
        default:  return -1; // Invalid
    }
}

// Sends a signal to the specified process
void send_signal(int pid) {
    printf("\033[H\033[J");

    char input[INPUT_BUF];

    while(true) {
        printf("\n--- What signal would you like to send? ---");
        printf(
    "\na) SIGTERM (15)   - Gracefully ask a process to quit"
            "\nb) SIGKILL (9)    - Forcefully kill a process"
            "\nc) SIGINT  (2)    - Interrupt (like Ctrl+C)"
            "\nd) SIGHUP  (1)    - Hangup / restart signal"
            "\ne) SIGSTOP (19)   - Pause a process (uninterruptible)"
            "\nf) SIGCONT (18)   - Resume a stopped process"
            "\ng) SIGUSR1 (10)   - User-defined signal 1"
            "\nh) SIGUSR2 (12)   - User-defined signal 2"
            "\n"
            "\nCHOICE: "
        );
        scanf("%s", input);

        char option = tolower(input[0]);
        int signal = signal_from_option(option);

        if (signal == -1) {
            printf("\nInvalid option. Please choose a letter between a and h.");
            continue;
        }

        if (kill(pid, signal) == -1) {
            perror("Failed to send signal");
        } else {
            printf("Sent signal %d to process %d.\n", signal, pid);
        }

        break;
    }

    open_process(pid);
}

// Verifies if the process ID is valid/exists.
bool process_exists(int pid) {
    DIR *proc = opendir("/proc");

    if(!proc) {
        perror("Failed to open /proc directory.");
        return false;
    }

    // Check if any process folder has the given ID.
    struct dirent* entry;
    while((entry = readdir(proc)) != NULL) {
        if(isdigit((unsigned char)entry->d_name[0])) {
            int dir_pid = atoi(entry->d_name);

            if(dir_pid == pid) {
                closedir(proc);
                return true;
            }
        }
    }

    closedir(proc);
    return false;
}

// Fetches the process name
char* get_process_name(int pid) {
    // the process name is in /proc/<PID>/comm
    char file_dir[128];
    snprintf(file_dir, sizeof(file_dir), "/proc/%d/comm", pid);

    FILE *fptr = fopen(file_dir, "r");
    if(!fptr) {
        perror("Failed to open process comm file.");
        return NULL;
    }

    char *p_name = malloc(COMM_BUF);

    if(!p_name) {
        perror("Could not allocate space for process name string.");
        fclose(fptr);
        return NULL;
    }

    if(fgets(p_name, COMM_BUF, fptr) == NULL) {
        perror("Failed to fetch process name from comm file.");

        fclose(fptr);
        free(p_name);

        return NULL;
    }

    fclose(fptr);

    // remove new line
    p_name[strcspn(p_name, "\n")] = '\0';

    return p_name;
}

// Shows a process' information
void view_process_information(int pid) {
    printf("\033[H\033[J");

    char *p_name = get_process_name(pid);
    if(!p_name) {
        perror("Failed to get process name.");
        return;
    }

    char *executable_path = fetch_process_information_rl(pid, "exe");
    if(!executable_path) {
        free(p_name);

        perror("Failed to get executable path");
        return;
    }

    char *ppid = fetch_information_st(pid, "PPid");
    if(!ppid) {
        free(p_name);
        free(executable_path);

        perror("Failed to get parent process ID.");
        return;
    }

    char *state = fetch_information_st(pid, "State");
    if(!ppid) {
        free(p_name);
        free(executable_path);
        free(ppid);

        perror("Failed to get process state.");
        return;
    }

    char *threads = fetch_information_st(pid, "Threads");
    if(!ppid) {
        free(p_name);
        free(executable_path);
        free(ppid);
        free(state);

        perror("Failed to get thread count.");
        return;
    }

    char *fds = fetch_file_descriptors(pid);
    if(!fds) {
        free(p_name);
        free(executable_path);
        free(ppid);
        free(state);
        free(threads);

        perror("Failed to allocate memory for file descriptors string.");
        return;
    }
    
    printf("\n--- Currently Viewing Process: %s (PID: %d) ---\nExecutable Path: %s\nParent PID: %s\nState: %s\nThreads: %s\nFile Descriptors: %s\n", p_name, pid, executable_path, ppid, state, threads, fds);

    free(p_name);
    free(executable_path);
    free(ppid);
    free(state);
    free(threads);

    // clear input buffer
    while ((getchar()) != '\n');  

    char input;

    printf("\nPress [ENTER] to continue...");
    scanf("%c", &input);

    open_process(pid);
}

char* fetch_process_information_rl(int pid, char* sub_dir) {
    char dir[COMM_BUF];
    snprintf(dir, DIR_BUF, "/proc/%d/%s", pid, sub_dir);

    // i forgot why I named this link_buff - wth does "link" mean
    char *link_buff = malloc(INFO_BUF);

    if(!link_buff) {
        perror("Failed to allocate space for the link buffer");
        return NULL;
    }

    int size = readlink(dir, link_buff, INFO_BUF);
    if(size == -1) {
        perror("Failed to get executable path.");
        return NULL;
    }

    link_buff[size] = '\0';

    return link_buff;
}

// Fetches information from the /proc/pid/status file
char* fetch_information_st(int pid, char *info) {
    char dir[COMM_BUF];
    snprintf(dir, DIR_BUF, "/proc/%d/status", pid);

    FILE *fptr = fopen(dir, "r");
    if(!fptr) {
        perror("Failed to open status file.");
        return NULL;
    }

    char line_buf[COMM_BUF];

    char *info_content = malloc(COMM_BUF+1);
    info_content[0] = '\0';

    while(fgets(line_buf, COMM_BUF, fptr) != NULL) {
        if(strstr(line_buf, info) != NULL) {
            char *colon = strchr(line_buf, ':');

            *colon = '\0';
            colon++;

            while(*colon == ' ') colon++;
            strcpy(info_content, colon);
            
            if(strstr(colon, "\n") != NULL) {
                info_content[strlen(colon)-1] = '\0';
            } else {
                info_content[strlen(colon)] = '\0';
            }

            break;
        }
    }

    if (info_content[0] == '\0') {
        perror("Failed to fetch status.");
        return NULL;
    }

    return info_content;
}

char* fetch_file_descriptors(int pid) {
    char fd_dir[DIR_BUF];
    snprintf(fd_dir, DIR_BUF, "/proc/%d/fd", pid);

    DIR *fd = opendir(fd_dir);
    if(!fd) {
        perror("Failed to open file descriptor directory.");
        return NULL;
    }

    char *fd_str = malloc(FD_STR_BUF);
    if(!fd_str) {
        perror("Failed to allocate memory for file descriptor string.");
        return NULL;
    }

    fd_str[0] = '\0';

    struct dirent *entry;
    while((entry = readdir(fd)) != NULL) {
        if(isdigit((unsigned char)entry->d_name[0])) {
            int len = strlen(fd_str);
            int fd = atoi(entry->d_name);

            // find what the file descriptor is pointing to
            char fd_path[DIR_BUF];
            snprintf(fd_path, DIR_BUF, "/proc/%d/fd/%d", pid, fd);

            char fd_link[INPUT_BUF];
            int size = readlink(fd_path, fd_link, INPUT_BUF);
            if(size == -1) {
                perror("Failed to readlink for file descriptor.");
                continue;
            }
            fd_link[size] = '\0';

            snprintf(fd_str + len, FD_STR_BUF - len, "\n- %s (%s)", entry->d_name, fd_link);
        }
    }

    return fd_str;
}