/*
 * proxy.c - A simple, concurrent HTTP/1.0 Web proxy that caches recently
 * 		accessed web content.
 * 
 * Implementing Posix threads with Semaphores using first readers-writers 
 * problem idea, which favors readers over writers. Implement a simple LRU
 * policy, update the cache order only before the last write lock to prevent
 * race conditions.
 * 
 *
 * Name: Yiting Zhi
 * yzhi@andrew.cmu.edu
 *
 */
#include "cache.h"

/* Request helper headers*/
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_con_hdr = "Proxy-Connection: close\r\n";

void doit(int fd);
void *thread(void *vargp);
void generate_request(rio_t *rp, char *request);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void build_header(char *buf, char *request);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
	int listenfd, *connfdp, port;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	pthread_t tid;

	/* Check command line args */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	port = atoi(argv[1]);

	/* Handle sigpipe error */
	Signal(SIGPIPE, SIG_IGN);

	/* Initialize cache header, cache size, reader/writer mutex */
	cache_init();

	/* Open a socket listener */
	listenfd = Open_listenfd(port);
	while (1) {
		connfdp = (int *) Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Pthread_create(&tid, NULL, thread, connfdp);
	}
    return 0;
}

/*
 * Thread routine
 */
void *thread(void *vargp)
{
	int connfd = *((int *)vargp);
	Pthread_detach(pthread_self());
	Free(vargp);
	doit(connfd);
	Close(connfd);
	return NULL;
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd)
{
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char request[MAXLINE], hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    unsigned char response[MAX_OBJECT_SIZE];
    rio_t rio;
    cache_t *cache;
    int clientfd;
    size_t n, filesize;
  
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);

    /* Read the first line to get method, uri and version */
    sscanf(buf, "%s %s %s", method, uri, version);

    /* Handle error when method is not GET */
    if (strcasecmp(method, "GET")) { 
       clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
        return;
    }

    /* Find the uri to see if it is in the cache */
    if ((cache = cache_find(uri)) != NULL) {
    	Rio_writen(fd, cache->content, cache->size);
    	return;
    }

    /* Parse uri to get hostname, port and path */
    parse_uri(uri, hostname, port, path);

    /* If hostname doesn't exist throw error */
    if (hostname == NULL) {
        clienterror(fd, "hostname", "400", "Bad Request",
            "The request cannot be fulfilled due to bad syntax");
        return;
    }

    /* put method and path to the request */
    sprintf(request, "%s %s HTTP/1.0\r\n", method, path);

    /* generate a http request */
    generate_request(&rio, request);

    /* Attach host to browser */
    if (!strstr(request, "Host: ")) {
        sprintf(request, "%sHost: %s\r\n", request, hostname);
    }

    /* Put and ending to the request */
    sprintf(request, "%s\r\n", request);

    /* Write to server*/
    clientfd = open_clientfd_r(hostname, atoi(port));
    if (clientfd == -1) {
    	clienterror(fd, hostname, "500", "Internal Server Error",
    		"The server you requested cannot respond at this time");
    	return;
    }
    Rio_writen(clientfd, request, strlen(request));

    /* Send response back */
    Rio_readinitb(&rio, clientfd);

    filesize = 0;
    /* Read the input line by line and count the filezie */
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        Rio_writen(fd, buf, n);       
        if (filesize + n <= MAX_OBJECT_SIZE) {
        	size_t i;
        	for (i = 0; i < n; i++) {
        		response[i + filesize] = buf[i];
        	}
        }
        filesize += n;
    }

    /* If size doesn't exceed max size, cache it to the memory */
    if (filesize <= MAX_OBJECT_SIZE) {
    	cache_store(filesize, uri, response);
    } 

    Close(clientfd);
}

/*
 * parse_uri - parse URI into hostname, port and path
 */
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
    char *ptr = uri;
    char temp[MAXLINE];
    int i;

    /* Check if the header contains 'http' */
    if (strlen(uri) > 7) {
	    for (i = 0; i < 7; i++) {
	        temp[i] = uri[i];
	    }
	    temp[i] = '\0';
	    if (strcmp(temp, "http://")) {
	        ptr = uri;
	    }
	    else {
	    	/* Start from the non-http part */
	        ptr = uri + 7;
	    }
	}
    i = 0;
    /* Copy the hostname to the variable */
    strcpy(temp, "");
    while (*ptr != '\0' && *ptr != ':' && *ptr != '/') {
        temp[i] = *ptr;
        i++;
        ptr++;
    }
    temp[i] = '\0';

    // Get hostname
    strcpy(hostname, temp);

    /* If the address ends just with hostname, default path is index.h */
    if (*ptr == '\0') {
        strcpy(port, "80");
        strcpy(path, "/index.html");
    }
    /* If the address has a path */
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
        if (*ptr == '\0') {
            strcpy(path, "/index.html");
        }
        else if (*ptr == '/') {
            strcpy(path, ptr);
        }
    }
}

/*
 * generate_request - generate HTTP request based on headers
 */
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

/*
 * build_header - build header based on browser information
 */
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
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxylab Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Yiting's Web Proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}