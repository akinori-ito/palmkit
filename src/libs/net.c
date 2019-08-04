#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <regex.h>
#include <netdb.h>

/* openSocket, originally from w3m source */
/* modified to avoid w3m-specific functions and GC */
int
openSocket(char *const hostname,unsigned short remoteport_num)
{
    volatile int sock = -1;
#ifdef INET6
    int *af;
    struct addrinfo hints, *res0, *res;
    int error;
    char *hname;
    char buf[1024];
    char portbuf[32];
#else				/* not INET6 */
    struct sockaddr_in hostaddr;
    struct hostent *entry;
    unsigned short s_port;
    int a1, a2, a3, a4;
    unsigned long adr;
#endif				/* not INET6 */
    regex_t reg;

#ifdef INET6
    /* rfc2732 compliance */
    hname = hostname;
    if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
	strncpy(buf,hostname+1,strlen(hostname)-2);
	hname = buf;
	if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
	    goto error;
    }
    for (af = ai_family_order_table[DNS_order];; af++) {
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = *af;
	hints.ai_socktype = SOCK_STREAM;
	if (remoteport_num != 0) {
	    sprintf(portbuf,"%d", remoteport_num);
	    error = getaddrinfo(hname, portbuf, &hints, &res0);
	}
	else {
	    error = -1;
	}
	if (error && remoteport_name && remoteport_name[0] != '\0') {
	    /* try default port */
	    error = getaddrinfo(hname, remoteport_name, &hints, &res0);
	}
	if (error) {
	    if (*af == PF_UNSPEC) {
		goto error;
	    }
	    /* try next ai family */
	    continue;
	}
	sock = -1;
	for (res = res0; res; res = res->ai_next) {
	    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	    if (sock < 0) {
		continue;
	    }
	    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
		close(sock);
		sock = -1;
		continue;
	    }
	    break;
	}
	if (sock < 0) {
	    freeaddrinfo(res0);
	    if (*af == PF_UNSPEC) {
		goto error;
	    }
	    /* try next ai family */
	    continue;
	}
	freeaddrinfo(res0);
	break;
    }
#else				/* not INET6 */
    s_port = htons(remoteport_num);
    memset(&hostaddr, 0, sizeof(struct sockaddr_in));
    if ((sock = socket(AF_INET, SOCK_STREAM, 6)) < 0) {
	goto error;
    }
    regcomp(&reg,"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$",
	    REG_NOSUB|REG_EXTENDED);
    if (regexec(&reg,hostname, 0, NULL,0) == 0) {
	sscanf(hostname, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
	adr = htonl((a1 << 24) | (a2 << 16) | (a3 << 8) | a4);
	bcopy((void *)&adr, (void *)&hostaddr.sin_addr, sizeof(long));
	hostaddr.sin_family = AF_INET;
	hostaddr.sin_port = s_port;
	if (connect(sock, (struct sockaddr *)&hostaddr,
		    sizeof(struct sockaddr_in)) < 0) {
	    goto error;
	}
    }
    else {
	char **h_addr_list;
	int result = -1;
	if ((entry = gethostbyname(hostname)) == NULL) {
	    goto error;
	}
	hostaddr.sin_family = AF_INET;
	hostaddr.sin_port = s_port;
	for (h_addr_list = entry->h_addr_list; *h_addr_list; h_addr_list++) {
	    memcpy((void *)&hostaddr.sin_addr,(void *)h_addr_list[0], 
		   entry->h_length);
	    if ((result = connect(sock, (struct sockaddr *)&hostaddr,
				  sizeof(struct sockaddr_in))) == 0) {
		break;
	    }
	}
	if (result < 0) {
	    goto error;
	}
    }
    regfree(&reg);
#endif				/* not INET6 */

    return sock;
  error:
    return -1;

}

