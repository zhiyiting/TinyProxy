/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_con_hdr = "Proxy-Connection: close\r\n";

void doit(int fd);
void generate_request(rio_t *rp, char *request);
void parse_uri(char *uri, char *hostname, char* port, char *path);
void build_header(char *buf, char *request);
/*
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
*/
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

int main(int argc, char **argv) 
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);

	doit(connfd);
	Close(connfd);
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char request[MAXLINE], hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t rio;
    int clientfd;
  
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);

    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) { 
       clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
        return;
    }

    parse_uri(uri, hostname, port, path);

    if (hostname == NULL) {
        clienterror(fd, "hostname", "400", "Bad Request",
            "The request cannot be fulfilled due to bad syntax");
        return;
    }

    sprintf(request, "%s %s HTTP/1.0\r\n", method, path);

    // generate a http request
    generate_request(&rio, request);

    if (!strstr(request, "Host: ")) {
        sprintf(request, "%sHost: %s\r\n", request, hostname);
    }
    sprintf(request, "%s\r\n", request);

    printf("%s", request);

    /* Write to server*/
    clientfd = Open_clientfd(hostname, atoi(port));
    Rio_writen(clientfd, request, strlen(request));

    /* Send response back */
    Rio_readinitb(&rio, clientfd);

    while (rio_readlineb(&rio, buf, MAXLINE) > 0) {
        Rio_writen(fd, buf, strlen(buf));
    }
    Rio_writen(fd, rio.rio_buf, strlen(rio.rio_buf));

}
/* $end doit */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
void parse_uri(char *uri, char *hostname, char *port, char *path) 
{
    struct in_addr addr;
    struct hostent *hostp;
    char *ptr;
    char temp[MAXLINE];
    int i;
    for (i = 0; i < 7; i++) {
        temp[i] = uri[i];
    }
    temp[i] = '\0';
    if (strcmp(temp, "http://")) {
        ptr = uri;
    }
    else {
        ptr = uri + 7;
    }
    i = 0;
    strcpy(temp, "");
    while (*ptr != '\0' && *ptr != ':' && *ptr != '/') {
        temp[i] = *ptr;
        i++;
        ptr++;
    }
    temp[i] = '\0';

    if (inet_aton(temp, &addr) != 0) {
        hostp = Gethostbyaddr((const char *)&addr, sizeof(addr), AF_INET);
    }
    else {
        hostp = Gethostbyname(temp);
    }
    if (hostp != NULL) {
        strcpy(hostname, hostp->h_name);
    }
    else {
        hostname = NULL;
        return;
    }

    if (*ptr == '\0') {
        strcpy(port, "80");
        strcpy(path, "/index.html");
    }
    else if (*ptr == '/') {
        strcpy(port, "80");
        strcpy(path, ptr);
    }
    else {
        ptr++;
        strcpy(port, "");
        for (i = 0; *ptr != '\0' && *ptr != '/'; ptr++) {
            port[i++] = *ptr;
        }
        port[i] = '\0';
        if (ptr[i] == '\0') {
            strcpy(path, "/index.html");
        }
        else if (ptr[i] == '/') {
            strcpy(path, ptr);
        }
    }

}
/* $end parse_uri */

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
void generate_request(rio_t *rp, char *request) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        build_header(buf, request);
    	Rio_readlineb(rp, buf, MAXLINE);
    }
    if (!strstr(request, "User-Agent")) {
        sprintf(request, "%s%s", request, user_agent_hdr);
    }
    if (!strstr(request, "Accept: ")) {
        sprintf(request, "%s%s", request, accept_hdr);
    }
    if (!strstr(request, "Accept-Encoding: ")) {
        sprintf(request, "%s%s", request, accept_encoding_hdr);
    }
    if (!strstr(request, "Connection: ")) {
        sprintf(request, "%s%s", request, connection_hdr);
    }
    if (!strstr(request, "Proxy-Connection: ")) {
        sprintf(request, "%s%s", request, proxy_con_hdr);
    }
}
/* $end read_requesthdrs */

void build_header(char *buf, char *request) 
{
    if (strstr(buf, "Host: ")) {
        sprintf(request, "%s%s", request, buf);
    }
    else if (strstr(buf, "User-Agent: ")) {
        sprintf(request, "%s%s", request, user_agent_hdr);
    }  
    else if (strstr(buf, "Accept-Encoding: ")) {
        sprintf(request, "%s%s", request, accept_encoding_hdr);
    }
    else if (strstr(buf, "Accept: ")) {
        sprintf(request, "%s%s", request, accept_hdr);
    }
    else if (strstr(buf, "Proxy-Connection: ")) {
        return;
    }
    else if (strstr(buf, "Connection: ")) {
        return;
    }
    else {
        sprintf(request, "%s%s", request, buf);
    }
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
