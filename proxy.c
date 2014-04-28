/*
 * proxy.c - A simple, concurrent HTTP/1.0 Web proxy that caches recently
 * 		accessed web content.
 *
 * Name: Yiting Zhi
 * yzhi@andrew.cmu.edu
 *
 */
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

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
void serve_content(int fd, char *path);
int parse_path(char *path, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
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

	Signal(SIGPIPE, SIG_IGN);

	listenfd = Open_listenfd(port);
	while (1) {
		connfdp = Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Pthread_create(&tid, NULL, thread, connfdp);
		//doit(connfd);
		//Close(connfd);
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


    if (!strcmp(hostname, "localhost")) {
    	serve_content(fd, path);
    	return;
    }

    sprintf(request, "%s %s HTTP/1.0\r\n", method, path);

    // generate a http request
    generate_request(&rio, request);

    if (!strstr(request, "Host: ")) {
        sprintf(request, "%sHost: %s\r\n", request, hostname);
    }
    sprintf(request, "%s\r\n", request);

    // debugging line //
    printf("%s", request);

    /* Write to server*/
    clientfd = Open_clientfd_r(hostname, atoi(port));
    Rio_writen(clientfd, request, strlen(request));

    /* Send response back */
    //Rio_readinitb(&rio, clientfd);
    /*
    sigset_t mask, oldmask;
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGPIPE);
    Sigprocmask(SIG_BLOCK, &mask, &oldmask);
    */
    while (Rio_readn(clientfd, buf, MAXLINE) != 0) {
        Rio_writen(fd, buf, MAXLINE);
    }
    //Rio_writen(fd, rio.rio_buf, MAXLINE);
    //Sigprocmask(SIG_SETMASK, &oldmask, NULL);
    Close(clientfd);
}

/*
 * parse_uri - parse URI into hostname, port and path
 */
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
	struct in_addr addr;
    struct hostent *hostp;
    char *ptr = uri;
    char temp[MAXLINE];
    int i;

    if (strlen(uri) > 7) {
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
	}
    i = 0;
    strcpy(temp, "");
    while (*ptr != '\0' && *ptr != ':' && *ptr != '/') {
        temp[i] = *ptr;
        i++;
        ptr++;
    }
    temp[i] = '\0';

    // if other location, just print
    // if itself, serve content
    if (strcmp(temp, "")) {
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
    }
    else {
    	strcpy(hostname, "localhost");
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
 * serve_content - act like a host to serve contents
 */
void serve_content(int fd, char *path)
{
	int is_static;
	struct stat sbuf;
	char filename[MAXLINE], cgiargs[MAXLINE];
	is_static = parse_path(path, filename, cgiargs);
	if (stat(filename, &sbuf) < 0) {
		clienterror(fd, filename, "404", "Not found", "Cannot find this file");
		return;
	}

	if (is_static) {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Cannot read file");
			return;
		}
		serve_static(fd, filename, sbuf.st_size);
	}
	else {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Cannot read file");
			return;
		}
		serve_dynamic(fd, filename, cgiargs);
	}
}

/*
 * parse_path - parse Path into filename and CGI args
 *				return 0 if dynamic content, 1 if static
 */
int parse_path(char *path, char *filename, char *cgiargs)
{
	char *ptr;

	if (!strstr(path, "cgi-bin")) {
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, path);
		return 1;
	}
	else {
		ptr = index(path, '?');
		if (ptr) {
			strcpy(cgiargs, ptr+1);
			*ptr = '\0';
		}
		else {
			strcpy(cgiargs, "");
		}
		strcpy(filename, ".");
		strcat(filename, path);
		return 0;
	}
}

/*
 * serve_static - copy a file back to the client
 */
void serve_static(int fd, char *filename, int filesize)
{
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];

	/* Send response headers to client */
	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Proxylab Server\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));

	/* Send response body to client */
	srcfd = Open(filename, O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype)
{
	if (strstr(filename, ".html")) {
		strcpy(filetype, "text/html");
	}
	else if (strstr(filename, ".gif")) {
		strcpy(filetype, "image/gif");
	}
	else if (strstr(filename, ".jpg")) {
		strcpy(filetype, "image/jpeg");
	}
	else {
		strcpy(filetype, "text/plain");
	}
}

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* child */
		/* Real server would set all CGI vars here */
		setenv("QUERY_STRING", cgiargs, 1); 
		Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
		Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); /* Parent waits for and reaps child */
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