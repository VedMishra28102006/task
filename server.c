#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_SIZE 256
#define MAX_BUFFER 1024
#define MAX_FILE 10240

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void freeArr(char **arr, int arrSize){
    for(int i=0; i < arrSize; i++){
        free(arr[i]);
    }
    free(arr);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    FILE *fptr;
    char response[MAX_FILE];
    char html[MAX_FILE];
    char xml[MAX_FILE];
    char id[MAX_SIZE];
    char newId[MAX_SIZE];
    char val[MAX_SIZE];
    char newVal[MAX_SIZE];
    char err[MAX_SIZE];
    char field[MAX_SIZE];
    char line[MAX_SIZE];
    char *start;

    char **lines = (char**)malloc(MAX_SIZE * sizeof(char*));
    if(lines == NULL){
        fprintf(stderr, "Memory allocation for array failed\n");
        exit(1);
    }

    for(int i=0; i < MAX_SIZE; i++){
        lines[i] = (char*)malloc(MAX_SIZE * sizeof(char));
        if(lines[i] == NULL){
            fprintf(stderr, "Memory allocation for array element indexed at %i failed\n", i);
            for(int j=0; j < i; j++){
                free(lines[j]);
            }
            free(lines);
            exit(1);
        }
    }

    int sockfd, newsockfd, portno, n;
    char buffer[MAX_BUFFER];
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("Unable to open socket");
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("Unable to bind");
    }

    listen(sockfd, 1);
    printf("Serving on port: %d\n", portno);

    clilen = sizeof(cli_addr);

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            error("Unable to accept connection");
        }

        bzero(buffer, sizeof(buffer));
        n = read(newsockfd, buffer, sizeof(buffer) - 1);

        if (n < 0) {
            error("Failed to read");
        }

        printf("Request:\n%s\n", buffer);

        if (strncmp(buffer, "GET / ", 6) == 0) {
            fptr = fopen("index.html", "r");
            if (fptr == NULL) {
                fprintf(stderr, "Error opening view file\n");
                exit(1);
            }

            fread(html, 1, sizeof(html), fptr);
            fclose(fptr);

            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Length: %zu\r\n"
                     "Content-Type: text/html\r\n\r\n%s",
                     strlen(html), html);

            n = write(newsockfd, response, strlen(response));
            if (n < 0) {
                error("Failed to write");
            }
        } else if (strncmp(buffer, "GET /getTasks ", 10) == 0) {
            fptr = fopen("data.xml", "r");
            if (fptr == NULL) {
                fprintf(stderr, "Error opening data file\n");
                exit(1);
            }

            fread(xml, 1, sizeof(xml), fptr);
            fclose(fptr);

            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Length: %zu\r\n"
                     "Content-Type: text/xml\r\n\r\n%s",
                     strlen(xml), xml);

            n = write(newsockfd, response, strlen(response));
            if (n < 0) {
                error("Failed to write");
            }
        } else if (strncmp(buffer, "POST /postTask", 14) == 0) {
            start = strstr(buffer, "\"value\":\"");
            if (start == NULL) {
                sprintf(err, "{\"error\":\"Bad post string\"}");
                snprintf(response, sizeof(response),
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Length: %zu\r\n"
                    "Content-Type: application/json\r\n\r\n%s",
                strlen(err), err);

                n = write(newsockfd, response, strlen(response));
                if (n < 0) {
                    error("Failed to write");
                }
            } else {
                start += 9;
                int i = 0;
                while (*start != '\"') {
                    val[i++] = *start++;
                }
                val[i] = '\0';
            }
            if(strcmp(val, "") == 0){
                sprintf(err, "{\"error\":\"The field is empty\", \"field\":\"taskInput\"}");
                snprintf(response, sizeof(response),
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Length: %zu\r\n"
                    "Content-Type: application/json\r\n\r\n%s",
                strlen(err), err);

                n = write(newsockfd, response, strlen(response));
                if (n < 0) {
                    error("Failed to write");
                }
            } else if (strlen(val) > 30){
                sprintf(err, "{\"error\":\"The field must have under near 30 chars\", \"field\":\"taskInput\"}");
                snprintf(response, sizeof(response),
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Length: %zu\r\n"
                    "Content-Type: application/json\r\n\r\n%s",
                strlen(err), err);

                n = write(newsockfd, response, strlen(response));
                if (n < 0) {
                    error("Failed to write");
                }
            } else {
                fptr = fopen("data.xml", "r+");
                while (fgets(line, sizeof(line), fptr) != NULL) {
                    start = strstr(line, "<id>");
                    if (start != NULL) {
                        start += 4;
                        int i = 0;
                        while (*start != '<') {
                            newId[i++] = *start++;
                        }
                        newId[i] = '\0';
                    }
                }

                fseek(fptr, -7, SEEK_END);
                fprintf(fptr,"    <task><id>%i</id><value>%s</value></task>\r\n</data>", atoi(newId)+1, val);

                sprintf(err, "{\"success\":\"1\", \"id\":\"%i\"}", atoi(newId)+1);
                snprintf(response, sizeof(response),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %zu\r\n"
                    "Content-Type: application/json\r\n\r\n%s",
                strlen(err), err);

                n = write(newsockfd, response, strlen(response));
                if (n < 0) {
                    error("Failed to write");
                }

                fclose(fptr);
            }
        } else if (strncmp(buffer, "DELETE /deleteTask ", 18) == 0) {
            start = strstr(buffer, "\"id\":\"");
            if (start == NULL || *(start+6) == '\"') {
                    sprintf(err, "{\"error\":\"Bad post string\"}");
                    snprintf(response, sizeof(response),
                        "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Length: %zu\r\n"
                        "Content-Type: application/json\r\n\r\n%s",
                    strlen(err), err);

                    n = write(newsockfd, response, strlen(response));
                    if (n < 0) {
                        error("Failed to write");
                    }
            } else {
                start += 6;
                int i = 0;
                while (*start != '\"') {
                    id[i++] = *start++;
                }
                id[i] = '\0';

                start = strstr(buffer, "\"value\":\"");

                fptr = fopen("data.xml", "r");
                int lineIndex = 0;

                while (fgets(line, sizeof(line), fptr) != NULL) {
                    start = strstr(line, "<id>");
                    if (start == NULL) {
                        snprintf(lines[lineIndex], MAX_SIZE, line);
                        lineIndex++;
                    } else {
                        start += 4;
                        int i = 0;
                        while (*start != '<') {
                            newId[i++] = *start++;
                        }
                        newId[i] = '\0';
                        if(strcmp(newId, id) != 0){
                            snprintf(lines[lineIndex], MAX_SIZE, line);
                            lineIndex++;
                        } else {
                            strcpy(id, "OK");
                        }
                    }
                }

                fclose(fptr);
                remove("data.xml");

                fptr = fopen("data.xml", "w");
                fclose(fptr);

                fptr = fopen("data.xml", "a");
                for(int i=0; i < MAX_SIZE; i++){
                    if(i <= lineIndex){
                        fprintf(fptr, lines[i]);
                    }
                    memset(lines[i], 0, MAX_SIZE);
                }

                if(strcmp(id, "OK") == 0){
                    sprintf(err, "{\"success\":\"1\"}");
                    snprintf(response, sizeof(response),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: %zu\r\n"
                        "Content-Type: application/json\r\n\r\n%s",
                    strlen(err), err);
                } else {
                    sprintf(err, "{\"error\":\"Task not found\"}");
                    snprintf(response, sizeof(response),
                        "HTTP/1.1 404 Not Found\r\n"
                        "Content-Length: %zu\r\n"
                        "Content-Type: application/json\r\n\r\n%s",
                    strlen(err), err);
                }

                n = write(newsockfd, response, strlen(response));
                if (n < 0) {
                    error("Failed to write");
                }

                fclose(fptr);
            }
        } else if (strncmp(buffer, "PUT /putTask ", 13) == 0) {
            start = strstr(buffer, "\"id\":\"");
            if (start == NULL || *(start + 6) == '\"') {
                    sprintf(err, "{\"error\":\"Bad post string\"}");
                    snprintf(response, sizeof(response),
                        "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Length: %zu\r\n"
                        "Content-Type: application/json\r\n\r\n%s",
                    strlen(err), err);

                    n = write(newsockfd, response, strlen(response));
                    if (n < 0) {
                        error("Failed to write");
                    }
            } else {
                start += 6;
                int i = 0;
                while (*start != '\"') {
                    id[i++] = *start++;
                }
                id[i] = '\0';
                start = strstr(buffer, "\"newValue\":\"");
                if (start == NULL) {
                    sprintf(err, "{\"error\":\"Bad post string\"}");
                    snprintf(response, sizeof(response),
                        "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Length: %zu\r\n"
                        "Content-Type: application/json\r\n\r\n%s",
                    strlen(err), err);

                    n = write(newsockfd, response, strlen(response));
                    if (n < 0) {
                        error("Failed to write");
                    }
                } else {
                    start += 12;
                    int i = 0;
                    while (*start != '\"') {
                        newVal[i++] = *start++;
                    }
                    newVal[i] = '\0';
                }
                if(strcmp(newVal, "") == 0){
                    sprintf(err, "{\"error\":\"The field is empty\", \"field\":\"editInput%s\"}", id);
                    printf("%s\n", err);
                    snprintf(response, sizeof(response),
                        "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Length: %zu\r\n"
                        "Content-Type: application/json\r\n\r\n%s",
                    strlen(err), err);

                    n = write(newsockfd, response, strlen(response));
                    if (n < 0) {
                        error("Failed to write");
                    }
                } else if(strlen(newVal) > 30){
                    sprintf(err, "{\"error\":\"The field must have under near 30 chars\", \"field\":\"editInput%s\"}", id);
                    snprintf(response, sizeof(response),
                        "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Length: %zu\r\n"
                        "Content-Type: application/json\r\n\r\n%s",
                    strlen(err), err);

                    n = write(newsockfd, response, strlen(response));
                    if (n < 0) {
                        error("Failed to write");
                    }
                } else {
                    fptr = fopen("data.xml", "r");
                    int lineIndex = 0;
                    while (fgets(line, sizeof(line), fptr) != NULL) {
                        start = strstr(line, "<id>");
                        if (start == NULL) {
                            sprintf(lines[lineIndex], line);
                            lineIndex++;
                        } else {
                            start += 4;
                            int i = 0;
                            while (*start != '<') {
                                newId[i++] = *start++;
                            }
                            newId[i] = '\0';
                            if(strcmp(newId, id) == 0){
                                snprintf(lines[lineIndex], MAX_SIZE, "    <task><id>%s</id><value>%s</value></task>\r\n", id, newVal);
                                strcpy(id, "OK");
                                lineIndex++;
                            } else {
                                snprintf(lines[lineIndex], MAX_SIZE, line);
                                lineIndex++;
                            }
                        }
                    }

                    fclose(fptr);
                    remove("data.xml");

                    fptr = fopen("data.xml", "w");
                    fclose(fptr);

                    fptr = fopen("data.xml", "a");
                    for(int i=0; i < MAX_SIZE; i++){
                        if(i <= lineIndex){
                            fprintf(fptr, lines[i]);
                        }
                        memset(lines[i], 0, MAX_SIZE);
                    }

                    if(strcmp(id, "OK") == 0){
                        sprintf(err, "{\"success\":\"1\"}");
                        snprintf(response, sizeof(response),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Length: %zu\r\n"
                            "Content-Type: application/json\r\n\r\n%s",
                        strlen(err), err);
                    } else {
                        sprintf(err, "{\"error\":\"Task not found\"}");
                        snprintf(response, sizeof(response),
                            "HTTP/1.1 404 Not Found\r\n"
                            "Content-Length: %zu\r\n"
                            "Content-Type: application/json\r\n\r\n%s",
                        strlen(err), err);
                    }

                    n = write(newsockfd, response, strlen(response));
                    if (n < 0) {
                        error("Failed to write");
                    }

                    fclose(fptr);
                }
            }
        }
        close(newsockfd);
    }

    freeArr(lines, MAX_SIZE);
    close(sockfd);
    return 0;
}
