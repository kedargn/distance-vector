#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>

char config_file_name[100];
int ttl, port_no, period, split_horizon, node_count=0, graph[100][100], *dist, *pi;     //split horizon can be either 1 or 0
long infinity;

int n_port_no; //TODO: temp variable. Remove this

struct sockaddr_in server_addr;
struct sockaddr_in neighbour_addr;
int sock, port;
int neighbour_addr_length;

void create_socket();
void bind_socket();
void check_result(char*, int);

struct neighbouring_routers{
 char ip_addr[15];
 int is_neighbour;
};

typedef struct{
	//struct neighbouring_routers destination;
	//struct neighbouring_routers next_hop;
	char destination[15], next_hop[15];
	int cost, ttl;
}route_entry;

route_entry routing_table[100]; //TODO: try not to hard code

struct neighbouring_routers neighbours[100];   //TODO: Try not to hard code

void print_neighbours(){
	int i;
	for(i=0;i<node_count;i++){
		printf("neighbour %d\t ip address %s\t is_neighbour %d\n",i, neighbours[i].ip_addr, neighbours[i].is_neighbour);
	}
}

void read_config_file(){
	FILE *fp;
	int i = 0;
	char ch;
	char *line, *ip_address, *neighbour;

	fp = fopen(config_file_name,"r");
	if(fp == NULL)
	{
		printf("Could not open the Configuration file.\n");
		exit(0);
	}
	line = (char*)malloc(256);
  // strcpy(neighbours[0].ip_addr, "120.0.0.1");
  // neighbours[0].is_neighbour = -1;
	while(fgets(line, 256, fp) != NULL)
	{
		ip_address = strtok(line, " ");
		neighbour = strtok(NULL, "\0");
		strcpy(neighbours[node_count].ip_addr, ip_address);
		if(node_count == 0)
		{
			neighbours[node_count].is_neighbour = -1;
		}
		else
		{
			neighbours[node_count].is_neighbour = (strncmp(neighbour, "yes", 3) == 0 ? 1: 0);
		}
		printf("ip_address %s\nneighbour %s\n", ip_address, neighbour);
		node_count++;		
	}
	print_neighbours();
	printf("Total nodes in this network is %d\n",node_count);
}

void print_header(){
	int i;
	for(i=0;i<node_count;i++){
		printf("%d\t",i);
	}
	printf("\n");
}

void print_array(){
 int i;
 printf("Distance Array\n");
 print_header();
 for(i=0;i<node_count;i++){
 	printf("%d\t",dist[i]);
 }
 printf("\nPi Array\n");
 print_header();
 for(i=0;i<node_count;i++){
 	printf("%d\t",pi[i]);
 }
 printf("\n");
}

void print_graph(){
	int i,j;
	printf("\ngraph\n");
	for(i=0;i<node_count;i++){
  	for(j=0;j<node_count;j++){
  	 printf("%d\t",graph[i][j]);
    }
    printf("\n");
  }
  printf("\n\n");
  print_array();
}

void allocate(){
	int i, j;
	// graph = calloc(node_count, sizeof(int));
 //  for(i=0;i<node_count;i++){
 //  	graph[i] = calloc(node_count, sizeof(int));
 //  }

  for(i=0;i<node_count;i++){
  	for(j=0;j<node_count;j++){
  		graph[i][j]=infinity;
  	}
  }

  dist = (int*)calloc(node_count, sizeof(int));
  pi = (int*)calloc(node_count, sizeof(int));

  print_graph();
}

void print_routing_table(){
	int i;
  printf("\n\nRouting table\n\n");
	printf("Node\t\tNext Hop\tcost\tTTL\n");
	for(i=0;i<node_count;i++){
		printf("%s\t%s\t%d\t%d\n", routing_table[i].destination, routing_table[i].next_hop, routing_table[i].cost, routing_table[i].ttl);
	}
}

void update_routing_table(){
  int i;
  for(i=0;i<node_count;i++){
  	strcpy(routing_table[i].destination,neighbours[i].ip_addr);
  	if(pi[i]==-1){
  		strcpy(routing_table[i].next_hop,"Null          ");
  	}
  	else
  	{
  		strcpy(routing_table[i].next_hop, neighbours[pi[i]].ip_addr);
  	}
  	routing_table[i].cost = dist[i];
  	routing_table[i].ttl = ttl;
  }
  print_routing_table();
}

void bellman_ford(){
 int i,j,k;
 /* For loop to intialise the graph*/
 for(i=0;i<node_count;i++){
 	dist[i] = infinity;
 	pi[i] = -1;
 }
 dist[0] = 0;

 for(k=1;k<node_count;k++){
	  for(i=0;i<node_count;i++){
	  	for(j=0;j<node_count;j++){
	  		if(dist[j] > dist[i]+graph[i][j]){
	  			dist[j] = dist[i]+graph[i][j];
	  			pi[j] = i;
	  		}
	  	}
	  }
 }
 printf("\nbellman_ford algo has finished\n");
 print_graph();
 update_routing_table();
}

void intialise_graph(){
	int i,j;
	for(j=1;j<node_count;j++){
 		graph[0][j] = (neighbours[j].is_neighbour == 1 ? 1 : infinity);
	}
	graph[0][0] = 0;
	print_graph();
}

void prepare_advertisement( unsigned char* advertise_contents){
	int i, j = 0, k=0, x=0, y=0;
	unsigned char transform;
	long path_cost;
	unsigned int p;
	unsigned char temp_ip_addr[15], part[3];
	for(i=0;i<node_count;i++){
		memcpy(temp_ip_addr,routing_table[i].destination, strlen(routing_table[i].destination));
    // /printf("%s\n", temp_ip_addr);
    while((temp_ip_addr[j])!='\0'){
    	if((temp_ip_addr[j]) != '.'){
       part[k++] = temp_ip_addr[j];
       j++;
      }
      if(temp_ip_addr[j]=='\0' || temp_ip_addr[j] == '.')
      {
      	p = atoi(part);
      	advertise_contents[x++] = p;
      	//printf("part is %u\n", p);
      	j++;
      	bzero(part,3);
      	k=0;
      }
    }
    path_cost = routing_table[i].cost;
    // transform = path_cost & 4278190080;
    advertise_contents[x++] = (path_cost & 4278190080)>>24;
    //printf("%u\n", advertise_contents[x-1]);
    advertise_contents[x++] = (path_cost & 16711680)>>16;
    //printf("%u\n", advertise_contents[x-1]);
    advertise_contents[x++] = (path_cost & 65280)>>8;
    //printf("%u\n", advertise_contents[x-1]);
    advertise_contents[x++] = path_cost & 255;
    //printf("%u\n", advertise_contents[x-1]);
    bzero(temp_ip_addr, 15);
    j=0;
    k=0;
	}
}

int get_vertex_number(char *ip){
	int  i;
	for(i=0;i<node_count;i++){
		if(strcmp(ip,neighbours[i].ip_addr)==0){
      return i;
		}
	}
	return -1;
}

void interpret_advertisement(unsigned char *advertise_contents){
	int i=0, source_node, destination_node;
	unsigned char ip[15];
	unsigned long get_cost=0, temp=0;
	printf("Received data is\n");
	while(i<(node_count*8)){
		sprintf(ip,"%u.%u.%u.%u",advertise_contents[i],advertise_contents[i+1],advertise_contents[i+2],advertise_contents[i+3]);
		printf("ip is %s -->", ip);
    temp = advertise_contents[i+4];
    get_cost = (temp << 24);
    temp = advertise_contents[i+5];
    get_cost = (temp<<16) | get_cost;
    temp = advertise_contents[i+6];
    get_cost = (temp<<8) | get_cost;
    temp = advertise_contents[i+7];
    get_cost = (temp) | get_cost;
    printf("%lu\n",get_cost);
    if(i==0){
    	source_node = get_vertex_number(ip);
    	if(source_node == -1){
    		printf("**ERROR*get_vertex_number retuned -1\n");
    		exit(1);
    	}
    	graph[source_node][source_node] = 0;
    	printf("source node is %d\n", source_node);
    }
    else
    {
    	destination_node = get_vertex_number(ip);
    	if(source_node == -1){
    		printf("**ERROR*get_vertex_number retuned -1\n");
    		exit(1);
    	}
    	printf("destination_node is %d\n", destination_node);
    	graph[source_node][destination_node] = get_cost;
    }
		i+=8;
	}
	bellman_ford();
}

void send_advertisment(char *advertise_contents){
  int i, result;
  for(i=0;i<node_count;i++){
  	if(neighbours[i].is_neighbour == 1){
  		neighbour_addr.sin_family = PF_INET;
  		neighbour_addr.sin_addr.s_addr = inet_addr(neighbours[i].ip_addr);
  		neighbour_addr.sin_port = htons(65001);
  		sendto(sock, advertise_contents, 8*node_count, 0, (struct sockaddr*)&neighbour_addr, sizeof(neighbour_addr_length));
  		printf("sendto to ip address %s is %d\n", neighbours[i].ip_addr, result);
  	}
  }	
}

void receive_advertisement(){
	int i, result;
	char received_advertisement[node_count*8];	
	for(;;){
		recvfrom(sock, received_advertisement, node_count*8, 0, (struct sockaddr*)&neighbour_addr, &neighbour_addr_length);
		interpret_advertisement(received_advertisement);
	}
}

void socket_creation(){
 server_addr.sin_family = AF_INET;
 server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 server_addr.sin_port = htons(port_no);
 neighbour_addr_length = sizeof(neighbour_addr);
 create_socket();
 bind_socket();
}

void intialise(){
	unsigned char *advertise_contents;
  allocate();
  intialise_graph();
  bellman_ford();
  advertise_contents = (char*)calloc(node_count*8,sizeof(char));
  prepare_advertisement(advertise_contents);
  socket_creation();
  send_advertisment(advertise_contents);
  receive_advertisement();
  //interpret_advertisement(advertise_contents);
} 

int main(int argc, char* argv[]){
	if(argc!=8){
		printf("Invalid number of arguments\n. Please pass 'Config_file_name port_no TTL infinity period split_horizon'\n");
		exit(1);
	}
  strcpy(config_file_name, argv[1]);
  port_no = atoi(argv[2]);
  ttl = atoi(argv[3]);
  infinity = atoi(argv[4]);
  period = atoi(argv[5]);
  split_horizon = atoi(argv[6]);
  n_port_no = atoi(argv[7]);
  printf(" Config file name %s\n port no %d\nTTL %d\ninfinity %ld\nperiod %d\nsplit_horizon %d n_port_no %d\n", config_file_name, port_no, ttl, infinity, period, split_horizon, n_port_no);
  read_config_file();
  intialise();
}

void create_socket()
{
 sock = socket(PF_INET, SOCK_DGRAM, 0);
 check_result("socket creation", sock);
}

/**
 * Binds socket.
 * If bind is unsuccesful then program will exit
 **/
void bind_socket()
{
 int bind_result;
 bind_result = bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
 check_result("Socket binding", bind_result);
}

/**
 * helper function to check results of socket creation and bind
 **/
void check_result(char* msg, int result)
{
if(result<0)
 {
  printf("%s failed with value %d\n",msg, result);
  perror(msg);
  exit(0);
 }
 else
 {
  printf("%s successful with value %d\n", msg, result);
 }
}