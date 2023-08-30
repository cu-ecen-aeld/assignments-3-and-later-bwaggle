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

#define PORT "9000"
#define BUFFER_SIZE 100000

int global_sockfd;
FILE* global_file = NULL;

void handle_sigterm(int signum) {
    // Perform cleanup actions here (e.g., releasing resources, closing files)
    printf("Received SIGTERM. Cleaning up.\n");
    close(global_sockfd);
    fclose(global_file);
    const char* file_path = "/var/tmp/aesdsocketdata";
    if (remove(file_path) == 0) {
        printf("Removing file %s\n", file_path);
    } else {
        perror("Error deleting file");
    }
    exit(EXIT_SUCCESS);
}

void handle_sigint(int signum) {
    // Perform cleanup actions here (e.g., releasing resources, closing files)
    printf("Received SIGINT. Exiting gracefully.\n");
    close(global_sockfd);
    fclose(global_file);
    const char* file_path = "/var/tmp/aesdsocketdata";
    if (remove(file_path) == 0) {
        printf("Removing file %s\n", file_path);
    } else {
        perror("Error deleting file");
    }
    exit(EXIT_SUCCESS);
}

int register_signal_handlers() {
    struct sigaction sa;

    // Register signal handlers for SIGTERM and SIGINT
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

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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
        if ((global_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(global_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            return -1;
        }

        if (bind(global_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(global_sockfd);
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

char* extractDataUpToNewLine(char *buf, int nread, int* new_line_found) {
    int new_line_index = -1;

    // Search for new line character
    for (int i = 0; i < nread; i++) {
        if (buf[i] == '\n') {
            new_line_index = i;
            *new_line_found = 1;
            break;
        }
    }

    // If no newline found, return NULL
    if (new_line_index == -1) {
        *new_line_found = 0;
        new_line_index = nread - 1;
        //return NULL;
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

char* readEntireFile(FILE* file, long* fsize) {

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *fsize = file_size;
    //printf("File size = %lu\n", file_size);

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

    return file_contents;
}


int main() {
    int new_fd;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
 
    char s[INET6_ADDRSTRLEN];
    ssize_t nread;
    char buf[BUFFER_SIZE];
    pid_t pid;

    // Open syslog connection
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Register signal handlers
    if (register_signal_handlers() == -1) return -1;

    // Establish connection
    establish_socket_connection();
    if (global_sockfd == -1) {
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
    if (listen(global_sockfd, 10) == -1) {
        perror("listen");
        return -1;
    }
    printf("Listening on port 9000\n");
    printf("server: waiting for connections...\n");

    // File handler
    FILE* file = fopen("/var/tmp/aesdsocketdata", "w+");
    global_file = file;
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    while(1) {
        sin_size = sizeof client_addr;
        char host[NI_MAXHOST], service[NI_MAXSERV];

        new_fd = accept(global_sockfd, (struct sockaddr *)&client_addr, &sin_size);
        printf("\nNow accepting packets on new_fd: %d\n", new_fd);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family,
            get_in_addr((struct sockaddr *)&client_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        syslog(LOG_INFO, "Accepted connection from %s", s);

        read_data:
        
        nread = recvfrom(new_fd, buf, BUFFER_SIZE, 0, 
                        (struct sockaddr *) &client_addr, &sin_size);
        
        if (nread == -1) {
            perror("recvfrom");
            close(new_fd);
            continue;
        };
        printf("received %zd bytes from %s", nread, s);

        buf[nread] = '\0'; 
        int new_line_found = 0;
        char* newString = extractDataUpToNewLine(buf, nread, &new_line_found);

        fprintf(file, "%s", newString);
        free(newString);

        if (new_line_found) {
            printf("\nNEWLINE FOUND\n");
        } else {
            printf("\nNEWLINE NOT FOUND ... continuing\n");
            // fprintf(file, "%c", '\n');
            goto read_data;
            // close(new_fd);
            // continue;
            // free(newString);
        }
        

        long fsize;
        char* contents_to_send = readEntireFile(file, &fsize);

 
        if (contents_to_send != NULL) {
            // Now you can use the "file_contents" variable containing the file content
            //printf("File Contents:\n%s\n", contents_to_send);

             // Don't forget to free the memory when you're done using it
        }

        if (sendto(new_fd, contents_to_send, fsize, 0, (struct sockaddr *) &client_addr, sin_size) != fsize) {
            perror("sendto");
            fprintf(stderr, "Error sending response\n");
            close(new_fd);

        } else {
            //printf("Sent file contents to client\n%s\nsize: %lu\n", contents_to_send, fsize);
            printf("Sent file contents to client\nsize: %lu\n", fsize);
            syslog(LOG_INFO, "Closed connection from %s", s);
            close(new_fd);
        }
        free (contents_to_send);

    }
    closelog();
    close(global_sockfd);
    
    return 0;
}
