
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <pthread.h>
#include <sys/queue.h>
#include <stddef.h>
#include "../aesd-char-driver/aesd_ioctl.h"

#define PORT "9000"
#define BUFFER_SIZE 100000
#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
#define FILE_NAME "/dev/aesdchar"
#else
#define FILE_NAME "/var/tmp/aesdsocketdata"
#endif


int server_socket;
FILE* global_file = NULL;

struct ThreadInfo {
    pthread_t thread_id;
    int client_socket;
    int thread_complete_flag;
    struct sockaddr_storage client_thread_addr;
    socklen_t sin_thread_size;
    SLIST_ENTRY(ThreadInfo) entries;
};

SLIST_HEAD(ThreadList, ThreadInfo) thread_list = SLIST_HEAD_INITIALIZER(thread_list);

pthread_mutex_t thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

void add_timestamp_to_file() {
    time_t current_time;
    struct tm *time_info;
    char timestamp[128];

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z", time_info);

    pthread_mutex_lock(&file_mutex);
    FILE* file = fopen(FILE_NAME, "a+");
    fprintf(file, "%s\n", timestamp);
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    printf("%s\n", timestamp);

  
}

// Thread that writes timestamp to file every 10 seconds
void *timer_thread_function(void *arg) {

    printf("Timer thread is starting ... \n");

    while(1) {
        add_timestamp_to_file();
        sleep(10);
    }
    return NULL;
}

// Get Socket IP address
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Extract packet data up to the newline character
char* extract_data_up_to_newline(char *buf, int nread, int* new_line_found) {
    int new_line_index = -1;

    // Search for new line character
    for (int i = 0; i < nread; i++) {
        if (buf[i] == '\n') {
            new_line_index = i;
            *new_line_found = 1;
            break;
        }
    }

    // Set newline flag
    if (new_line_index == -1) {
        *new_line_found = 0;
        new_line_index = nread - 1;
    }

    // Allocate memory for the new string
    int new_size = new_line_index + 2;
    char* new_string = (char*)malloc(new_size * sizeof(char));
    if (new_string == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    // Copy the data up to the newline character
    for (int j = 0; j < new_size; j++) {
        new_string[j] = buf[j];
    }
    new_string[new_line_index + 1] = '\0';

    return new_string;

}

char* read_char_device(long* fsize, int found_cmd, uint32_t write_cmd, uint32_t write_cmd_offset) {

    struct aesd_seekto seekto;
    int file_num;
    uint32_t encoded_value = 0;
    int error;

    // Lock file access
    pthread_mutex_lock(&file_mutex);
    FILE* file = fopen(FILE_NAME, "r");

    // Send an ioctl to SEEK_SET the file position if the appropriate
    // command was found in the socket data stream
    if (found_cmd) {
        // Encode both the write_cmd and write_cmd_offset into a 
        // single uint32_t value.
        // This works around an issue where the seekto struct is not
        // being passed properly to the ioctl. Only the first member
        // is being passed.
        encoded_value = (write_cmd << 4) | write_cmd_offset;
        seekto.write_cmd = encoded_value;
        seekto.write_cmd_offset = 0;
        file_num = fileno(file);

        printf("\nSending ioctl command write_cmd '%u', write_cmd_offset '%u'\n", seekto.write_cmd, seekto.write_cmd_offset);
        error = ioctl(file_num, AESDCHAR_IOCSEEKTO, &seekto);
        printf("Error status is : %d\n", error);
    }

    // Allocate memory for the file contents (+1 for null-terminator)
    char* file_contents = (char*)malloc(BUFFER_SIZE);
    if (file_contents == NULL) {
        fclose(file);
        perror("Memory allocation failed");
        return NULL;
    }

    // Read the entire file into file_contents
    size_t read_size = fread(file_contents, 1, BUFFER_SIZE, file);
    *fsize = read_size;
    printf("Read file of size %ld\n", read_size);

    // Null-terminate the string
    file_contents[read_size] = '\n';
    file_contents[read_size] = '\0';
    fclose(file);
    pthread_mutex_unlock(&file_mutex);

    return file_contents;
}

char* read_file(long* fsize) {

    pthread_mutex_lock(&file_mutex);
    FILE* file = fopen(FILE_NAME, "r");
    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *fsize = file_size;

    // Allocate memory for the file contents (+1 for null-terminator)
    char* file_contents = (char*)malloc(file_size + 1);
    if (file_contents == NULL) {
        fclose(file);
        perror("Memory allocation failed");
        return NULL;
    }

    // Read the entire file into file_contents
    size_t read_size = fread(file_contents, 1, file_size, file);
    if (read_size != file_size) {
        fclose(file);
        free(file_contents);
        perror("Error reading file");
        return NULL;
    }

    // Null-terminate the string
    file_contents[file_size] = '\0';
    fclose(file);
    pthread_mutex_unlock(&file_mutex);

    return file_contents;
}

// Thread that processes each client packet and echos back the response
void *handle_connection(void *arg) {
    struct ThreadInfo *info = (struct ThreadInfo *)arg;
    int client_socket = info->client_socket;
    struct sockaddr_storage client_thread_address_local = info->client_thread_addr;
    socklen_t sin_thread_size_local = info->sin_thread_size;
    ssize_t nread;
    long fsize;
    char buf[BUFFER_SIZE];
    char s[INET6_ADDRSTRLEN];
    int found_ioctl_cmd = 0;
    uint32_t write_cmd, write_cmd_offset;


    printf("Thread %lu processing client socket %d\n", info->thread_id, client_socket);

    inet_ntop(client_thread_address_local.ss_family,
    get_in_addr((struct sockaddr *)&client_thread_address_local),
    s, sizeof s);
    printf("server: got connection from %s\n", s);
    syslog(LOG_INFO, "Accepted connection from %s", s);

    // Process incoming data packets until no data left
    while ((nread = recvfrom(client_socket, buf, BUFFER_SIZE, 0, (struct sockaddr *) &client_thread_address_local, &sin_thread_size_local)) > 0) {
        
        printf("received %zd bytes", nread);

        if (nread == -1) {
            perror("recvfrom");
            close(client_socket);
            break;
        };
        
        pthread_mutex_lock(&file_mutex);

        // TODO Move buf into ThreadInfo structure (shared between threads)
        buf[nread] = '\0'; 
        int new_line_found = 0;
        char* newString = extract_data_up_to_newline(buf, nread, &new_line_found);

        const char *search_string = "AESDCHAR_IOCSEEKTO:";
        const char *match_string = NULL;
        const char *result = strstr(newString, search_string);

        if (result != NULL) {
            found_ioctl_cmd= 1;
            match_string = result + strlen(search_string);
            sscanf(match_string, "%d,%d", &write_cmd, &write_cmd_offset);
            printf("Found ioctl command write_cmd %d, write_cmd_offset %d", write_cmd, write_cmd_offset);
        }

        if (!found_ioctl_cmd) {
            FILE * file = fopen(FILE_NAME, "a+");
            fprintf(file, "%s", newString);
            fclose(file);
        }

        free(newString);
        pthread_mutex_unlock(&file_mutex);

        if (new_line_found) {
            printf("\nNEWLINE FOUND\n");
            break;
        } else {
            printf("\nNEWLINE NOT FOUND ... continuing\n");
            continue;
        }
        
    }

    // Read entire contents of appended file
    // Packets are appended by each thread independently
    
    #ifdef USE_AESD_CHAR_DEVICE
    char* contents_to_send = read_char_device(&fsize, found_ioctl_cmd, write_cmd, write_cmd_offset);
    #else
    char* contents_to_send = read_file(&fsize);
    #endif


    if (contents_to_send == NULL) {
        free(contents_to_send);
    }

    // Send file contents pack to client
    if (sendto(client_socket, contents_to_send, fsize, 0, (struct sockaddr *) &client_thread_address_local, sin_thread_size_local) != fsize) {
        perror("sendto");
        fprintf(stderr, "Error sending response\n");
        close(client_socket);

    } else {
        printf("\nSent file contents to client\nsize: %lu\n", fsize);
        syslog(LOG_INFO, "Closed connection from %s", s);
    }

    // Cleanup
    free (contents_to_send);
    close (client_socket);

    // Lets the cleanup process in main know that the thread is done
    info->thread_complete_flag = 1;
    pthread_exit(NULL);
}

// Loop the threads, check for compelete status.
// If complete, remove from linked list and exit
void cleanup_threads() {
    struct ThreadInfo *info, *tmp;
    pthread_mutex_lock(&thread_list_mutex);
    
    info = SLIST_FIRST(&thread_list);
    while (info != NULL) {
        tmp = SLIST_NEXT(info, entries);
        if (info->thread_complete_flag) {
            printf("Removing thread %lu\n", info->thread_id);
            close(info->client_socket);
            pthread_join(info->thread_id, NULL);
            SLIST_REMOVE(&thread_list, info, ThreadInfo, entries);
            free(info);
        }
        info = tmp;
    }

    pthread_mutex_unlock(&thread_list_mutex);

    // if (info == NULL) {
    //     pthread_kill(timer_thread_id, SIGTERM);
    // }
}


void handle_sigterm(int signum) {
    // Perform cleanup actions here (e.g., releasing resources, closing files)
    printf("Received SIGTERM. Cleaning up.\n");
    close(server_socket);

    #ifndef USE_AESD_CHAR_DEVICE
    fclose(global_file);
    unlink(FILE_NAME);
    const char* file_path = "/var/tmp/aesdsocketdata";
    if (remove(file_path) == 0) {
        printf("Removing file %s\n", file_path);
    } else {
        perror("Error deleting file");
    }
    #endif
    exit(EXIT_SUCCESS);
}

void handle_sigint(int signum) {
    // Perform cleanup actions here (e.g., releasing resources, closing files)
    printf("Received SIGINT. Exiting gracefully.\n");
    close(server_socket);
    closelog();

    #ifndef USE_AESD_CHAR_DEVICE
    fclose(global_file);
    unlink(FILE_NAME);
   
    const char* file_path = "/var/tmp/aesdsocketdata";
    if (remove(file_path) == 0) {
        printf("Removing file %s\n", file_path);
    } else {
        perror("Error deleting file");
    }
    #endif
    exit(EXIT_SUCCESS);
}

// Register signal handlers for SIGTERM and SIGINT
int register_signal_handlers() {
    struct sigaction sa;

    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigterm handler");
        return -1;
    }

    sa.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigint handler");
        return -1;
    }
}

int establish_socket_connection() {
    struct addrinfo hints, *servinfo, *p;
    int yes=1;
    int rv;
    // Configure host port
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Get Address Info to open socket
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        return -1;
    }

    // Establish socket connection and bind to the socket
    printf(">> Establish socket and bind\n");
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            return -1;
        }

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }
}

int start_daemon_mode() {
    int pid = fork();
    printf("Starting daemon on pid: %d\n", pid);
    if (pid == -1)
        return -1;
    else if (pid != 0)
        exit (EXIT_SUCCESS);

    if (setsid() == -1)
        return -1;

    if (chdir("/") == -1)
        return -1;

    return pid;
}

int main() {
    int client_socket;
    struct sockaddr_storage client_addr;
    socklen_t sin_size = sizeof client_addr;
    char s[INET6_ADDRSTRLEN];
    pid_t pid;

    // All child threads write to this file
    printf("File name: %s\n", FILE_NAME);

    #ifndef USE_AESD_CHAR_DEVICE
    global_file = fopen(FILE_NAME, "a+");
    #endif

    // Open syslog connection
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Register signal handlers
    if (register_signal_handlers() == -1) return -1;

    // Establish server connection
    establish_socket_connection();
    if (server_socket == -1) {
        perror("Error establishing socket connection");
        return -1;
    }

    // Daemonize
    pid = start_daemon_mode();
    if (pid == -1) {
        perror("Error starting daemon mode");
        return -1;
    }

    // Start listening on port 9000
    if (listen(server_socket, 10) == -1) {
        perror("listen");
        return -1;
    }

    printf("Listening on port 9000\n");
    printf("server: waiting for connections...\n");

    #ifndef USE_AESD_CHAR_DEVICE
    pthread_t timer_thread;
    if(pthread_create(&timer_thread, NULL, timer_thread_function, global_file)) {
        perror("Timer thread create");
        goto cleanup_timer;
    }
    #endif

    while(1) {
        // Wait for and accept a client connection
        printf("\n Waiting for connection \n");
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sin_size);
        
        printf("\nNow accepting packets on client_socket: %d\n", client_socket);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        // Populate the thread info structure
        struct ThreadInfo *info = (struct ThreadInfo *)malloc(sizeof(struct ThreadInfo));
        if (info == NULL) {
            perror("Thread memory allocation error");
            close(client_socket);
            continue;
        }
        info->client_socket = client_socket;
        info->sin_thread_size = sizeof(client_addr);
        info->client_thread_addr = client_addr;
        info->thread_complete_flag = 0;

        // Create the thread that handles reading and writing
        // packets for the client connetion
        pthread_create(&info->thread_id, NULL, handle_connection, info);

        // Insert the thread's info into the linked list
        pthread_mutex_lock(&thread_list_mutex);
        SLIST_INSERT_HEAD(&thread_list, info, entries);
        pthread_mutex_unlock(&thread_list_mutex);

        cleanup_threads();
    }

    cleanup_timer:
    close(server_socket);
    
    return 0;
}
