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
        printf("Removing file %s", file_path);
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
        printf("Removing file %s", file_path);
    } else {
        perror("Error deleting file");
    }
    exit(EXIT_SUCCESS);
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Function to extract data up to the first newline character
char* extractDataUpToNewline(char* buf, int nread) {
    char newString[BUFFER_SIZE];
    int newlineIndex = -1;

    for (int i = 0; i < nread; i++) {
        if (buf[i] == '\n') {
            newlineIndex = i;
            break;
        }
    }

    if (newlineIndex != -1) {
        strncpy(newString, buf, newlineIndex); // Copy characters up to the newline
        newString[newlineIndex] = '\0'; // Null-terminate the new string
    } else {
        strcpy(newString, buf); // If no newline found, copy the entire buf to newString
    }

    char* result = strdup(newString); // Allocate memory for the result and copy the newString
    return result;
}

char* readEntireFile(FILE* file, long* fsize) {

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *fsize = file_size;
    printf("File size = %lu", file_size);

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
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    int rv;
    char s[INET6_ADDRSTRLEN];
    struct sigaction sa;
    ssize_t nread;
    char buf[BUFFER_SIZE];
    pid_t pid;
    int yes=1;


    // Obtain address matching host port
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Open syslog connection
    openlog("aesdsocket", LOG_PID, LOG_USER);

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

    // Get Address Info to open socket
    printf(">> getaddrinfo");
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        return -1;
    }

    // Establish socket connection and bind to the socket
    printf(">> Establish socket and bind\n");
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            return -1;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);
    global_sockfd = sockfd;

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }

    // Daemonize
    pid = fork();
    printf("Starting daemon on pid: %d\n", pid);
    if (pid == -1)
        return -1;
    else if (pid != 0)
        exit (EXIT_SUCCESS);

    if (setsid() == -1)
        return -1;

    if (chdir("/") == -1)
        return -1;

    // open ("/dev/null", O_RDWR);
    // dup (0);
    // dup (0);

    // Start listening on port 9000
    if (listen(sockfd, 10) == -1) {
        perror("listen");
        return -1;
    }
    printf("Listening on port 9000\n");

    // sa.sa_handler = sigchld_handler;
    // sigemptyset(&sa.sa_mask);
    // sa.sa_flags = SA_RESTART;
    // if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    //     perror("sigaction");
    //     return -1;
    // }

    printf("server: waiting for connections...\n");

    // remove file
    // const char* file_path = "/var/tmp/aesdsocketdata";
    // if (remove("file_path") == 0) {
    //     printf("Removing file %s", file_path);
    // } else {
    //     perror("Error deleting file");
    // }

    // File handler
    FILE* file = fopen("/var/tmp/aesdsocketdata", "w+");
    global_file = file;
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }
    // Truncate file
    // fseek(file, 0, SEEK_SET);
    // ftruncate(fileno(file), 0);


    while(1) {
        sin_size = sizeof client_addr;
        char host[NI_MAXHOST], service[NI_MAXSERV];

        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        printf("Now accepting packets on new_fd: %d\n", new_fd);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family,
            get_in_addr((struct sockaddr *)&client_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        syslog(LOG_INFO, "Accepted connection from %s", s);
        
        nread = recvfrom(new_fd, buf, BUFFER_SIZE, 0, 
                        (struct sockaddr *) &client_addr, &sin_size);
        
        if (nread == -1) continue;
        printf("received %zd bytes from %s", nread, s);

        buf[nread] = '\0'; 
        char* newString = extractDataUpToNewline(buf, nread);
        strcat(newString, "\n");

        fprintf(file, "%s", newString);
        printf("New string up to the newline: %s", newString);
        free(newString);

        long fsize;
        char* contents_to_send = readEntireFile(file, &fsize);

 
        if (contents_to_send != NULL) {
            // Now you can use the "file_contents" variable containing the file content
            printf("File Contents:\n%s\n", contents_to_send);

             // Don't forget to free the memory when you're done using it
        }

        if (sendto(new_fd, contents_to_send, fsize, 0, (struct sockaddr *) &client_addr, sin_size) != fsize) {
            perror("send");
            fprintf(stderr, "Error sending response\n");
        } else {
            printf("Sent file contents to client\n%s\nsize: %lu\n", contents_to_send, fsize);
            close(new_fd);
            syslog(LOG_INFO, "Closed connection from %s", s);
        }

    }
    closelog();
    close(sockfd);
    
    return 0;
}
