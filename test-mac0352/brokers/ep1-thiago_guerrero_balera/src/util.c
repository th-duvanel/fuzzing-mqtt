#include <util.h>

struct sigaction sigIntAction;

char *strncatM(char *dest, char *src, int n, size_t startPoint) {
    for (size_t i = 0 ; i < n; i++)
        dest[startPoint + i] = src[i];
    return dest;
}

void checkSize(int size, int expectedSize, int errorCode) {
    if(size < expectedSize) {
        fprintf(stderr, "Malformed packet! Closing connection.\n");
        exit(errorCode);
    }
}

void cleanTmpFiles(char *folder) {
    DIR *tmpFolder;
    struct dirent *nextPipe;
    char * filePath;

    if((tmpFolder = opendir(folder)) == NULL) {
        perror("opendir :(\n");
        exit(EXIT_UTIL);
    }

    while ((nextPipe = readdir(tmpFolder)) != NULL) {
        if(!strcmp(nextPipe->d_name, ".") || !strcmp(nextPipe->d_name, ".."))
            continue;

        filePath = malloc((strlen(folder) + strlen(nextPipe->d_name) + 2) * sizeof(char));
        sprintf(filePath, "%s/%s", folder, nextPipe->d_name);
        if(nextPipe->d_type == DT_DIR) {
            cleanTmpFiles(filePath);
        }
        
        if(remove(filePath) == -1) {
            perror("remove :(\n");
            exit(EXIT_UTIL);
        }

        free(filePath);
    }

    if(closedir(tmpFolder) == -1) {
        perror("closedir :(\n");
        exit(EXIT_UTIL);
    }
    if(strcmp(folder, TMP_FOLDER)  == 0 && rmdir(folder) == -1) {
        perror("rmdir :(\n");
        exit(EXIT_UTIL);
    }
}

void sigIntHandler(int sig_no) {
    printf("\n[CTRL-C pressed. Closing connection.]\n");
    cleanTmpFiles(TMP_FOLDER);
    //Sets default behavior back
    sigaction(SIGINT, &sigIntAction, NULL);
    kill(0, SIGINT);
}

void setCleanUpHook() {
    struct sigaction action;
    bzero(&action, sizeof(action));

    action.sa_handler = &sigIntHandler;
    sigaction(SIGINT, &action, &sigIntAction);
}

void resetCleanUpHook() {
    sigaction(SIGINT, &sigIntAction, NULL);
}

int getOrCreateTopicFile(char *topicPath, mode_t mode) {
    pid_t pid = getpid();
    int pidDigits = floor(log10(abs(pid))) + 1;

    char * specificTopicPath = malloc((strlen(topicPath) + pidDigits + 2) * sizeof(char));
    sprintf(specificTopicPath, "%s/%d", topicPath, pid);
    if(access(specificTopicPath, F_OK) == -1) {
        if(access(topicPath, F_OK) == -1) {
            if(mkdir(topicPath, 0) == -1) {
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