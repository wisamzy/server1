#include "threadpool.h"
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>


#define MAX_VALUE 2028
#define BUFFER_SIZE 265
#define protocol1 "HTTP/1.1"
#define protocol0 "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

/*This function is called whenever an error occurs */
void err_handling(char err[]) {
    perror(err);
}

int isNumber(char *string) {
    int i;
    if(string == NULL)
        return 1;
    int len = strlen(string);
    for(i=0; i < len;i++){
        if(!isdigit(string[i])){
            return 1;
        }
    }
    return 0;
}

char *get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

void write_index(char *buf, char *root, char *time, struct stat file_stat, int n_socket_fd, DIR *dir) {
    /*This function is called when a directory is requested.
 * It constructs an HTML response containing a table of the contents of the directory, and writes it to the client using the write function.*/
    char last_modified[MAX_VALUE];
    char content_length[MAX_VALUE];
    strftime(last_modified, sizeof(last_modified), RFC1123FMT, gmtime(&file_stat.st_mtime));
    sprintf(content_length, "%ld", file_stat.st_size);
    sprintf(buf, "HTTP/1.1 200 OK\r\n"
                 "Server: webserver/1.1\r\n"
                 "Date: %s\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %s\r\n"
                 "Connection: close\r\n\r\n", time, "");
    if (write(n_socket_fd, buf, strlen(buf)) < 0)
        err_handling("write");
    sprintf(buf, "<HTML>\n"
                 "<HEAD><TITLE>Index of %s</TITLE></HEAD>\n"
                 "<BODY>\n"
                 "<H4>Index of %s</H4>\n"
                 "<table CELLSPACING=8>\n"
                 "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>", root, root);
    if (write(n_socket_fd, buf, strlen(buf)) < 0) {
        err_handling("write");
    }
    struct dirent *dir_open = NULL;
    struct stat f_stat;
    memset(buf, '\0', MAX_VALUE);
    char last_mod[128];
    while ((dir_open = readdir(dir))) {
        char file[128];
        memset(file, '\0', sizeof(file));
        memset(last_mod, '\0', sizeof(last_mod));
        strcpy(file, ".");
        strcat(file, root);
        strcat(file, dir_open->d_name);
        stat(file, &f_stat);
        int file_t = S_ISDIR(f_stat.st_mode);
        int isFile = S_ISREG(f_stat.st_mode);
        strftime(last_mod, sizeof(last_mod), RFC1123FMT, gmtime(&f_stat.st_mtime));
        strcpy(buf, "<tr><td><a HREF=\"");
        strcat(buf, dir_open->d_name);
        if (file_t != 0) {
            strcat(buf, "/");
        }
        strcat(buf, "\">");
        strcat(buf, dir_open->d_name);
        strcat(buf, "</a></td><td>");
        strcat(buf, last_mod);
        strcat(buf, "</td>");
        if (isFile != 0) {
            char content_size[120];
            sprintf(content_size, "%lu", f_stat.st_size);
            strcat(buf, "<td>");
            strcat(buf, content_size);
            strcat(buf, "</td>\n");
        }
        strcat(buf, "</tr>");
        if (write(n_socket_fd, buf, strlen(buf)) < 0)
            err_handling("write");
    }
    printf("\n\n\n");
    strcpy(buf, "</table>\n"
                "<HR>\n"
                "<ADDRESS>webserver/1.1</ADDRESS>\n"
                "</BODY></HTML>");
    if (write(n_socket_fd, buf, strlen(buf)) < 0)
        err_handling("write");
}

void response_err(char *text, int socket_fd, char *t_buffer, struct stat file_stat) {
    /* This function is called when an error occurs while processing the request. It sends an appropriate error response to the client.*/
    char last_modified[100];
    char content_length[100];
    char resp[MAX_VALUE];
    memset(resp, '\0', MAX_VALUE);
    memset(last_modified, '\0', 100);
    memset(content_length, '\0', 100);
    strftime(last_modified, sizeof(last_modified), RFC1123FMT, gmtime(&file_stat.st_mtime));
    sprintf(content_length, "%ld", file_stat.st_size);
    if ((strcmp(text, "501")) == 0) {
        sprintf(resp, "HTTP/1.1 501 Not supported\r\n"
                      "Server: webserver/1.1\r\n"
                      "Date: %s\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 129\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n"
                      "<BODY><H4>501 Not supported</H4>\n"
                      "Method is not supported.\n"
                      "</BODY></HTML>", t_buffer);
    } else if ((strcmp(text, "500")) == 0) {
        sprintf(resp, "HTTP/1.1 500 Internal Server Error\r\n"
                      "Server: webserver/1.1\r\n"
                      "Date: %s\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 144\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n"
                      "<BODY><H4>500 Internal Server Error</H4>\n"
                      "Some server side error.\n"
                      "</BODY></HTML>", t_buffer);
    } else if ((strcmp(text, "404")) == 0) {
        sprintf(resp, "HTTP/1.1 404 Not Found\r\n"
                      "Server: webserver/1.1\r\n"
                      "Date: %s\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 112\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n"
                      "<BODY><H4>404 Not Found</H4>\n"
                      "File not found.\n"
                      "</BODY></HTML>", t_buffer);

    } else if (strcmp(text, "400") == 0) {
        sprintf(resp, "HTTP/1.1 400 Bad Request\r\n"
                      "Server: webserver/1.1\r\n"
                      "Date: %s\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 113\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n"
                      "<BODY><H4>400 Bad request</H4>\n"
                      "Bad Request.\n"
                      "</BODY></HTML>", t_buffer);
    } else if (strcmp(text, "302") == 0) {
        sprintf(resp, "HTTP/1.1 302 Found\r\n"
                      "Server: webserver/1.1\r\n"
                      "Date: %s\r\n"
                      "Location: <path>\\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: 123\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\n"
                      "<BODY><H4>302 Found</H4>\n"
                      "Directories must end with a slash.\n"
                      "</BODY></HTML>", t_buffer);
    } else if (strcmp(text, " ") == 0) {
        sprintf(resp, "HTTP/1.1 200 OK\r\n"
                      "Server: webserver/1.1\r\n"
                      "Date: %s\r\n"
                      "Content-Type: <type/subtype>\n"
                      "Content-Length: %s\n"
                      "Last-Modified: %s\n"
                      "Connection: close", t_buffer, content_length, last_modified);
    }
    if ((write(socket_fd, resp, strlen(resp))) < 0) {
        err_handling("write");
    }
}

int create_request(void *args) {
/*This function is called when a new request is received.
 * It reads the request from the socket, parses it to determine the method, protocol, and path, and sends a response accordingly*/
    time_t time_ = time(NULL);
    char t_buffer[BUFFER_SIZE], input[MAX_VALUE];
    memset(input, '\0', MAX_VALUE);
    memset(t_buffer, '\0', BUFFER_SIZE);
    int n_socket_fd = *((int *) args);
    size_t bytes, totalBytes = 0;
    strftime(t_buffer, sizeof(t_buffer), RFC1123FMT, gmtime(&time_));
    while ((bytes = read(n_socket_fd, input, MAX_VALUE)) > 0) {
        totalBytes += bytes;
        if(totalBytes == 0)
            err_handling("write");
        break;
    }
    char Method_Get[MAX_VALUE]={'\0'};
    char Path[MAX_VALUE]={'\0'};
    char Protocol_HTTP[MAX_VALUE]={'\0'};
    sscanf(input,"%s %s %s",Method_Get,Path,Protocol_HTTP);
    char *root = strstr(Path, "/");
    char path_root[MAX_VALUE];
    strcpy(path_root, ".");
    strcat(path_root, Path);
    char buffer[MAX_VALUE];
    memset(buffer, '\0', MAX_VALUE);
    struct stat file_stat;
    stat(path_root, &file_stat);
    DIR *dir=NULL;
    dir= opendir(path_root);
    if (strcmp(Protocol_HTTP, protocol1) != 0) {
        if (strcmp(Protocol_HTTP, protocol0) != 0) {
            response_err("400", n_socket_fd, t_buffer, file_stat);
        }
    } else if (strcmp(Method_Get, "GET") != 0) {
        response_err("501", n_socket_fd, t_buffer, file_stat);
    } else if (access(path_root, F_OK) == -1 && strcmp(path_root, "./") != 0) {
        response_err("404", n_socket_fd, t_buffer, file_stat);
    } else if (path_root[strlen(path_root) - 1] != '/' && dir != NULL){//is a directory
        response_err("302", n_socket_fd, t_buffer, file_stat);
    } else if (path_root[strlen(path_root) - 1] == '/' && dir != NULL) {
        char index_html[MAX_VALUE];
        strcpy(index_html, path_root);
        strcat(index_html, "index.html");
        int fd;
        if (access(index_html, F_OK) != -1) { // if index exists
            if ((fd = open(index_html, O_WRONLY)) < 0) {
                err_handling("open");
            }
            size_t Bytes, total_bytes = 0;
            while ((Bytes = read(fd, buffer, MAX_VALUE - 1)) > 0) {
                total_bytes += Bytes;
                if (total_bytes == 0)
                    err_handling("read");
                if (write(n_socket_fd, buffer, strlen(buffer)) < 0)
                    err_handling("write");
            }
        } else {
            write_index(buffer, root, t_buffer, file_stat, n_socket_fd, dir);
        }

    } else if (S_ISREG(file_stat.st_mode)) {
        int ofd;
        if (access(path_root, R_OK) != 0) {
            response_err("403", n_socket_fd, t_buffer, file_stat);
        } else {
            if ((ofd = open(path_root, O_RDONLY)) < 0) {
                err_handling("open");
            }
            char *content_type = get_mime_type(path_root);
            memset(buffer, '\0', sizeof(buffer));
            char content_length[100];
            char last_modified[100];
            strftime(last_modified, sizeof(last_modified), RFC1123FMT, gmtime(&file_stat.st_mtime));
            sprintf(content_length, "%ld", file_stat.st_size);
            strftime(t_buffer, sizeof(t_buffer), RFC1123FMT, gmtime(&time_));
            if (content_type == NULL) {
                sprintf(buffer, "HTTP/1.1 200 OK\r\n"
                                "Server: webserver/1.1\r\n"
                                "Date: %s\r\n"
                                "Content-Type: \r\n"
                                "Content-Length: %s\r\n"
                                "Last-Modified: %s\r\n"
                                "Connection: close\r\n\r\n", t_buffer, content_length, last_modified);
            } else {
                sprintf(buffer, "HTTP/1.1 200 OK\r\n"
                                "Server: webserver/1.1\r\n"
                                "Date: %s\r\n"
                                "Content-Type: %s\r\n"
                                "Content-Length: %s\r\n"
                                "Last-Modified: %s\r\n"
                                "Connection: close\r\n\r\n", t_buffer,content_type,content_length, last_modified);
            }
            write(n_socket_fd,buffer, strlen(buffer));
            char unsigned unsignedBuffer[MAX_VALUE]={0};
            unsigned long long int readBytes;
            while ((readBytes = read(ofd, unsignedBuffer, MAX_VALUE-1)) > 0) {
                if (write(n_socket_fd, unsignedBuffer, readBytes) < 0) {
                    err_handling("write");
                }
            }
        }
    }
    close(n_socket_fd);
    return 0;
}

void creat_server_socket(char *args[]) {
    /*This function creates a server socket and configures it to listen for incoming connections on a specified port.
     * It creates a thread pool with a specified number of threads, and enters a loop to accept incoming connections and dispatch them to the thread pool for processing. Once all the requests have been handled,
     * it closes the socket and destroys the thread pool.*/
    int port = atoi(args[1]), pool_size = atoi(args[2]), socket_fd, n_socket_fd, client_address, max_req = atoi(args[3]);
    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd < 0) {
        err_handling("socket");
        exit(1);
    } else {
        struct sockaddr_in server_addr = {0};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
            err_handling("bind");
            exit(1);
        }
        if (listen(socket_fd, max_req) == -1) {
            err_handling("listen");
            exit(1);
        }
        threadpool *pool = create_threadpool(pool_size);
        if (pool == NULL){
            printf("thread pool creat");
            return;
        }
        int i = 0;
        while (i <= max_req) {
            client_address = sizeof(struct sockaddr_in);
            if ((n_socket_fd = accept(socket_fd, (struct sockaddr *) &server_addr, (socklen_t *) &client_address)) < 0) {
                err_handling("accept");
            } else {
                dispatch(pool, (void *) create_request, (void *) &n_socket_fd);
            }
            i++;
        }
        destroy_threadpool(pool);
        close(socket_fd);
    }
}

int main(int argc, char *argv[]) {

    if (argc != 4){
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(0);
    }
    creat_server_socket(argv);
    return 0;
}