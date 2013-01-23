#include "http.h"
#include "debug.h"

/**
 * Emptyblock is a statically defined variable for functions to return if they are unable
 * to complete a request
 */

//The maximum amount of bytes to send per net_write() call
#define NET_BUFFER_SIZE 1024

// Write our message to the server
static s32 send_message(s32 server, char *msg) {
	s32 bytes_transferred = 0;
	s32 remaining = strlen(msg);
	while (remaining) {
		if ((bytes_transferred = net_write(server, msg, remaining > NET_BUFFER_SIZE ? NET_BUFFER_SIZE : remaining)) > 0) {
			remaining -= bytes_transferred;
			usleep (20 * 1000);
		} else if (bytes_transferred < 0) {
			return bytes_transferred;
		} else {
			return -ENODATA;
		}
	}
	return 0;
}

/**
 * Connect to a remote server via TCP on a specified port
 *
 * @param u32 ip address of the server to connect to
 * @param u32 the port to connect to on the server
 * @return s32 The connection to the server (negative number if connection could not be established)
 */
static s32 server_connect(u32 ipaddress, u32 socket_port) {
	//Initialize socket
	s32 connection = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (connection < 0) return connection;

	struct sockaddr_in connect_addr;
	memset(&connect_addr, 0, sizeof(connect_addr));
	connect_addr.sin_family = AF_INET;
	connect_addr.sin_port = socket_port;
	connect_addr.sin_addr.s_addr= ipaddress;

	//Attemt to open the socket
	if (net_connect(connection, (struct sockaddr*)&connect_addr, sizeof(connect_addr)) == -1) {
		net_close(connection);
		return -1;
	}
	return connection;
}

//The amount of memory in bytes reserved initially to store the HTTP response in
//Be careful in increasing this number, reading from a socket on the Wii
//will fail if you request more than 20k or so
#define HTTP_BUFFER_SIZE 1024 * 5

//The amount of memory the buffer should expanded with if the buffer is full
#define HTTP_BUFFER_GROWTH 1024 * 5

/**
 * This function reads all the data from a connection into a buffer which it returns.
 * It will return an empty buffer if something doesn't go as planned
 *
 * @param s32 connection The connection identifier to suck the response out of
 * @return block A 'block' struct (see http.h) in which the buffer is located
 */
 
 #define CONTENTLENGHT "Content-Length:"
u8 *read_message(s32 connection, u32 *size)
{
	//Create a block of memory to put in the response
	u8 *buffer;
	u32 bytes;
	bool contentLength = false;
	bool fileStart = false;
	
	*size = 0;
	
	buffer = calloc(1, HTTP_BUFFER_SIZE);
	bytes = HTTP_BUFFER_SIZE;

	if (buffer == NULL) 
		return NULL;

	//The offset variable always points to the first byte of memory that is free in the buffer
	u32 offset = 0;

	while (true)
		{
		//Fill the buffer with a new batch of bytes from the connection,
		//starting from where we left of in the buffer till the end of the buffer
		s32 bytes_read = net_read(connection, buffer + offset, bytes - offset);

		//Anything below 0 is an error in the connection
		if (bytes_read < 0)
			{
			//printf("Connection error from net_read()  Errorcode: %i\n", bytes_read);
			free (buffer);
			return NULL;
			}
			
		//No more bytes were read into the buffer,
		//we assume this means the HTTP response is done
		
		//Debug ("read_message->net_read = %d", bytes_read);

		if (bytes_read == 0) 
			break;

		offset += bytes_read;

		if (!contentLength && offset < (HTTP_BUFFER_SIZE - strlen (CONTENTLENGHT)))
			{
			char *p = strstr ((char*)buffer, CONTENTLENGHT);
			if (p)
				{
				p += strlen (CONTENTLENGHT);
				if (strstr (p, "\r\n"))
					{
					http.size = atoi(p);
					Debug ("read_message->contentLenght = %d", http.size);
					contentLength = true;
					}
				}
			}
			
		if (!fileStart && offset < (HTTP_BUFFER_SIZE - 4))
			{
			char *p = strstr ((char*)buffer, "\r\n\r\n");
			if (p)
				{
				p += 4;
				http.headersize = (void*)p - (void*)buffer;
				Debug ("read_message->headersize = %d", http.headersize);
				fileStart = true;
				}
			}
			
		http.bytes = offset - http.headersize;
		
		if (http.cb) http.cb();

		//Check if we have enough buffer left over,
		//if not expand it with an additional HTTP_BUFFER_GROWTH worth of bytes
		if (offset >= bytes)
			{
			bytes += HTTP_BUFFER_GROWTH;
			buffer = realloc(buffer, bytes);

			if (buffer == NULL)
				{
				return NULL;
				}
			}
		}

	//At the end of above loop offset should be precisely the amount of bytes that were read from the connection
	bytes = offset;

	//Shrink the size of the buffer so the data fits exactly in it (+1, hidden with \0, usefull for strings)
	buffer = realloc(buffer, bytes + 1);
	buffer[bytes] = '\0';

	*size = bytes;
	return buffer;
	}

/**
 * Downloads the contents of a URL to memory
 * This method is not threadsafe (because networking is not threadsafe on the Wii)
 */
u8 *downloadfile (const char *url, u32 *size, http_Callback cb)
	{
	*size = 0;
	u8 *buff = NULL;
	
	memset (&http, 0, sizeof(s_http));

	http.cb = cb;
	
	//Check if the url starts with "http://", if not it is not considered a valid url
	if (strncmp(url, "http://", strlen("http://")) != 0)
		{
		//printf("URL '%s' doesn't start with 'http://'\n", url);
		return NULL;
		}

	//Locate the path part of the url by searching for '/' past "http://"
	char *path = strchr(url + strlen("http://"), '/');

	//At the very least the url has to end with '/', ending with just a domain is invalid
	if (path == NULL)
		{
		//printf("URL '%s' has no PATH part\n", url);
		return NULL;
		}

	//Extract the domain part out of the url
	int domainlength = path - url - strlen("http://");

	if (domainlength == 0)
		{
		//printf("No domain part in URL '%s'\n", url);
		return NULL;
		}

	char domain[domainlength + 1];
	strncpy(domain, url + strlen("http://"), domainlength);
	domain[domainlength] = '\0';

	//Parsing of the URL is done, start making an actual connection
	u32 ipaddress = getipbynamecached(domain);

	if (ipaddress == 0)
		{
		//printf("\ndomain %s could not be resolved", domain);
		return NULL;
		}

	Debug ("downloadfile:connecting");
	s32 connection = server_connect(ipaddress, 80);

	if(connection < 0) {
		//printf("Error establishing connection");
		return NULL;
	}
	
	Debug ("downloadfile:success");

	//Form a nice request header to send to the webserver
	char* headerformat = "GET %s HTTP/1.0\r\nHost: %s\r\nReferer: %s\r\nUser-Agent: postLoader2\r\n\r\n";
	char header[strlen(headerformat) + strlen(path) + strlen(domain)*2 + 1];
	sprintf(header, headerformat, path, domain, domain);

	//Do the request and get the response
	send_message(connection, header);
	buff = read_message(connection, size);
	net_close(connection);

	//Search for the 4-character sequence \r\n\r\n in the response which signals the start of the http payload (file)
	unsigned char *filestart = NULL;
	u32 filesize = 0;
	unsigned int i;
	for(i = 3; i < *size; i++)
	{
		if(buff[i] == '\n' &&
			buff[i-1] == '\r' &&
			buff[i-2] == '\n' &&
			buff[i-3] == '\r')
		{
			filestart = buff + i + 1;
			filesize = *size - i - 1;
			break;
		}
	}

	if(filestart == NULL)
	{
		//printf("HTTP Response was without a file\n");
		free(buff);
		return NULL;
	}

	//Copy the file part of the response into a new memoryblock to return
	u8 *file = malloc(filesize);

	if(file == NULL)
	{
		//printf("No more memory to copy file from HTTP response\n");
		free(buff);
		return NULL;
	}

	memcpy(file, filestart, filesize);

	//Dispose of the original response
	free(buff);

	*size = filesize;
	return file;
}

s32 GetConnection(char * domain)
{
	if(!domain)
		return -1;

	u32 ipaddress = getipbynamecached(domain);
	if(ipaddress == 0)
		return -1;
	s32 connection = server_connect(ipaddress, 80);
	return connection;

}

int network_request(int connection, const char * request, char * filename)
{
	char buf[1024];
	char *ptr = NULL;

	u32 cnt, size;
	s32 ret;

	ret = net_send(connection, request, strlen(request), 0);
	if (ret < 0)
		return ret;

	memset(buf, 0, sizeof(buf));

	for (cnt = 0; !strstr(buf, "\r\n\r\n"); cnt++)
		if (net_recv(connection, buf + cnt, 1, 0) <= 0)
			return -1;

	if (!strstr(buf, "HTTP/1.1 200 OK"))
		return -1;

	if(filename)
	{
		/* Get filename */
		ptr = strstr(buf, "filename=\"");

		if(ptr)
		{
			ptr += sizeof("filename=\"")-1;

			for(cnt = 0; ptr[cnt] != '\r' && ptr[cnt] != '\n' && ptr[cnt] != '"'; cnt++)
			{
				filename[cnt] = ptr[cnt];
				filename[cnt+1] = '\0';
			}
		}
	}

	ptr = strstr(buf, "Content-Length:");
	if (!ptr)
		return -1;

	sscanf(ptr, "Content-Length: %u", &size);
	return size;
}

int network_read(int connection, u8 *buf, u32 len)
{
	u32 read = 0;
	s32 ret = -1;

	while (read < len)
	{
		ret = net_read(connection, buf + read, len - read);
		if (ret < 0)
			return ret;

		if (!ret)
			break;

		read += ret;
	}

	return read;
}
