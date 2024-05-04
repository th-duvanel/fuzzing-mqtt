#include <util.h>

/* Structure describing the action to be taken when a signal arrives.  */
struct sigaction sigIntAction;

char *byteToBinary(char *dest, const char *src, int n, size_t startPoint) {
    for (size_t i = 0; i < n; i++)
        dest[startPoint + i] = src[i];
    return dest;
}

void matchValues(int val, int expectedVal, int errorCode) {
    if (val < expectedVal) {
        fprintf(stderr, "Malformed packet! Closing connection.\n");
        exit(errorCode);
    }
}

void cleanFolder(char *folder) {
    DIR *dirp;
    struct dirent *nextPipe;
    char *filePath;

    if ((dirp = opendir(folder)) == NULL) {
        perror("opendir :(\n");
        exit(EXIT_UTIL);
    }

    while ((nextPipe = readdir(dirp)) != NULL) {
        if (!strcmp(nextPipe->d_name, ".") || !strcmp(nextPipe->d_name, ".."))
            continue;

        filePath = malloc((strlen(folder) + strlen(nextPipe->d_name) + 2) * sizeof(char));
        sprintf(filePath, "%s/%s", folder, nextPipe->d_name);
        if (nextPipe->d_type == DT_DIR) {
            cleanFolder(filePath);
        }

        if (remove(filePath) == -1) {
            perror("remove :(\n");
            exit(EXIT_UTIL);
        }

        free(filePath);
    }

    if (closedir(dirp) == -1) {
        perror("closedir :(\n");
        exit(EXIT_UTIL);
    }
    if (strcmp(folder, TMP_DIR) == 0 && rmdir(folder) == -1) {
        perror("rmdir :(\n");
        exit(EXIT_UTIL);
    }
}

void signIntHandler(int sig_no) {
    printf("\n[CTRL-C pressed. Closing connection.]\n");
    cleanFolder(TMP_DIR);
    //Sets default behavior back
    sigaction(SIGINT, &sigIntAction, NULL);
    kill(0, SIGINT);
}

void setSignIntAction() {
    struct sigaction action;
    bzero(&action, sizeof(action));

    action.sa_handler = &signIntHandler;
    sigaction(SIGINT, &action, &sigIntAction);
}

void resetSignIntAction() {
    sigaction(SIGINT, &sigIntAction, NULL);
}

int getTopicPipe(char *topicPath, mode_t mode) {
    pid_t pid = getpid();
    int pidDigits = floor(log10(abs(pid))) + 1;

    char *specificTopicPath = malloc((strlen(topicPath) + pidDigits + 2) * sizeof(char));
    sprintf(specificTopicPath, "%s/%d", topicPath, pid);
    if (access(specificTopicPath, F_OK) == -1) {
        if (access(topicPath, F_OK) == -1) {
            if (mkdir(topicPath, 0) == -1) {
                perror("mkdir :(\n");
                exit(EXIT_UTIL);
            }
        }
        if (mkfifo(specificTopicPath, 0) == -1) {
            perror("mkfifo :(\n");
            exit(EXIT_UTIL);
        }
    }

    int fd = open(specificTopicPath, mode);
    free(specificTopicPath);
    return fd;
}
