#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: write <writefile> <writestr>\n");
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    // Open syslog connection
    openlog("writer", LOG_PID, LOG_USER);

    // Open File 
    FILE *file = fopen(writefile, "w");

    if (file == NULL) {
        syslog(LOG_ERR, "Error opening file '%s'", writefile);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);

    // Write the text string to the file and log errors
    if (fprintf(file, "%s\n", writestr) < 0) {
        syslog(LOG_ERR, "Error writing to file '%s'", writefile);
        fclose(file);
        return 1;
    }

    fclose(file);
    closelog();

    // Print success message
    printf("Successfully wrote '%s' to file '%s'\n", writestr, writefile);

    return 0;
}
