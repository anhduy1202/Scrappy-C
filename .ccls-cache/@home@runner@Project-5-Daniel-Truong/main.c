#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void print_menu();
void error(char *msg) {
  perror(msg);
  exit(0);
};
int host_to_ip(const char *hostname, char *ip_add);

int debug_on = 0; // 0: off, 1: on
int head_tag = 0;
int body_tag = 0;
int http_only = 0;

int main(int argc, char **argv) {
  int next_idx = 1;
  int position = 1;
  // Process command line args
  while (next_idx < argc) {
    if (!strcmp(argv[next_idx], "-h") || !strcmp(argv[next_idx], "--help")) {
      ++next_idx;
      print_menu();
      exit(1);
    }
    // Check if a URL was provided on the command line
    else if (argc < 2) {
      printf("ERROR: NOT ENOUGH ARGUMENTS \n \nexpect to have 2, but "
             "received %d \n \n",
             argc - 1);
      print_menu();
      exit(1);
    } else if (!strcmp(argv[next_idx], "-d")) {
      ++next_idx;
      debug_on = 1;
      position += 1;
    } else if (!strcmp(argv[next_idx], "-H")) {
      ++next_idx;
      head_tag = 1;
      position += 1;
      ++next_idx;
    } else if (!strcmp(argv[next_idx], "-B") || !strcmp(argv[next_idx], "-b")) {
      if (debug_on) {
        printf("BODY TAG ONLY \n \n");
      }
      ++next_idx;
      position += 1;
      body_tag = 1;
      ++next_idx;
    } else if (!strcmp(argv[next_idx], "-o") || !strcmp(argv[next_idx], "-O")) {
      if (debug_on) {
        printf("HTTP ONLY ON");
      }
      position += 1;
      ++next_idx;
      http_only = 1;
      ++next_idx;
    } else {
      ++next_idx;
    }
  }

  // Check if input is domain name or IP address

  // Parse the URL into its protocol, site, and path
  char *protocol = strtok(argv[position], ":");
  char *site = strtok(NULL, "/");
  char *path = strtok(NULL, "");

  char ip_address[INET_ADDRSTRLEN];


  // If path empty
  if (path == NULL) {
    path = "/";
  }
  // Check if the protocol is HTTP
  if (strcmp(protocol, "http") != 0) {
    error("ERROR: Invalid protocol. Only HTTP is supported\n");
    print_menu();
  }

  // Set protocol to be http if user don't provide the protocol
  if (strlen(protocol) > 4) {
    protocol = "http";
    site = strtok(argv[position], "/");
    path = strtok(NULL, "");
  }

  if (debug_on) {
    printf("Protocol: %s \n \n", protocol);
    printf("Site: %s \n \n", site);
    printf("path: %s \n \n", path);
  }

  // Create a socket to connect to the website
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR: Could not create socket\n");
    exit(1);
  }
  // Get the IP address of the website
  struct hostent *host = gethostbyname(site);
  if (host == NULL) {
    error("ERROR: Could not resolve host\n");
    exit(1);
  }
  int valid_ip = host_to_ip(host->h_name, ip_address);
  if (valid_ip < 0) {
    error("ERROR: failed to get host ip address \n");
  }
  if (debug_on) {
    printf("The IP address of %s is %s\n", host->h_name, ip_address);
  }

  // Set up the server address
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(80);
  // Convert IP address to binary representation
  inet_pton(AF_INET, ip_address, &serv_addr.sin_addr);

  // Connect to the server
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("ERROR: Could not connect to server\n");
    return 1;
  }

  // Send a request to the server to access the specified web page
  char request[1024];
  sprintf(request,
          "GET /%s HTTP/1.1\r\nHost: %s\r\nAccept: text/plain, text/html, "
          "application/json, application/xml\r\n\r\n",
          path, site);
  if (write(sockfd, request, strlen(request)) < 0) {
    error("ERROR: Could not send request to server\n");
    return 1;
  }

  // Read the response from the server
  char response[1024];
  int n = read(sockfd, response, sizeof(response));
  if (n < 0) {
    error("ERROR: Could not receive response from server\n");
    exit(1);
  }

  // Print the response

  // Print what between <HEADER> </HEADER>
  if (head_tag) {
    if (debug_on) {
      printf("HEAD TAG ONLY \n\n");
    }
    int start = strstr(response, "<HEADER>") - response;
    int end = strstr(response, "</HEADER>") - response;
    for (int i = start + 6; i < end; i++) {
      printf("%c", response[i]);
    }
  }
  // Print what between <BODY> </BODY>
  if (body_tag) {
    int start = strstr(response, "<BODY>") - response;
    for (int i = start + 6; i < strlen(response); i++) {
      printf("%c", response[i]);
    }
  }
  if (http_only) {
    int blank_line = strstr(response, "\r\n\r\n") - response;
    for (int i = 0; i < blank_line; i++) {
      printf("%c", response[i]);
    }
  }
  if (!head_tag && !body_tag && !http_only) {
    printf("%s", response);
  }

  // Parse response message
  char *response_code = strtok(response, "\r");
  response_code = strtok(response, " ");
  response_code = strtok(NULL, " ");
  int response_code_number = atoi(response_code);
  char *response_text = strtok(NULL, "\r\n");
  if (debug_on) {
    printf("Response code: %d \n \n", response_code_number);
  }
  // Response code 2xx - Success
  if (response_code_number >= 200 && response_code_number < 300) {
    // extract the html/text
    printf("%s\n", response_text);
  }
  // Response code 3xx - redirect
  if (response_code_number >= 300 && response_code_number < 400) {
    printf("Redirect to https://%s", site);
  }

  // Close the socket
  close(sockfd);

  return 0;
}

int host_to_ip(const char *hostname, char *ip_address) {
  struct addrinfo hints, *result, *rp;
  int s;

  // Set up the hints structure to specify that we want
  // an IPv4 address and a stream (TCP) socket.
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  // Call getaddrinfo() to resolve the host name to an IP address.
  s = getaddrinfo(hostname, NULL, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "ERROR: %s\n", gai_strerror(s));
    return -1;
  }

  // Loop through the results and use the first one we can connect to.
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    // Convert the IP address to a string and copy it to the output buffer.
    inet_ntop(AF_INET, &(((struct sockaddr_in *)rp->ai_addr)->sin_addr),
              ip_address, INET_ADDRSTRLEN);

    // We've found an IP address, so we can stop now.
    break;
  }

  freeaddrinfo(result);

  return 0;
}

void print_menu() {
  printf(
      "\nHelp commands \n \n"
      "-h --help: print this help \n \n"
      "-d : Debug mode on \n \n"
      "-H : print out <HEAD> </HEAD> only \n \n"
      "-B -b: print out <BODY> </BODY> only \n \n"
      "-O -o: print out HTTP response only ( no html ) \n \n"
      "Example: \n\n"
      "./main http://info.cern.ch/hypertext/WWW/TheProject.html: Send "
      "request to info.cern.ch  \n \n"
      "./main -H hhttp://info.cern.ch/hypertext/WWW/TheProject.html: Send "
      "request to info.cern.ch return html between <HEADER> </HEADER> only  \n "
      "\n"
      "./main -d -H hhttp://info.cern.ch/hypertext/WWW/TheProject.html: Send "
      "request to info.cern.ch return html between <HEADER> </HEADER> only "
      "with debug mode on \n "
      "\n"
      "./main -B hhttp://info.cern.ch/hypertext/WWW/TheProject.html: Send "
      "request to info.cern.ch return html between <BODY> </BODY> only  \n \n");
}