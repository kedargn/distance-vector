#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>

char config_file_name[100], my_ip[50];
int ttl, source_node=-1, port_no, period, split_horizon=0, node_count=1, graph[100][100], *dist, *pi, is_routing_table_changed = 0, is_initialised=0;     //split horizon can be either 1 or 0
long infinity;
unsigned char *advertise_contents;
pthread_mutex_t bellman_mutex, update_mutex, adv_mutex;

struct sockaddr_in server_addr;
struct sockaddr_in neighbour_addr;
int sock, port;
int neighbour_addr_length;

void create_socket();
void bind_socket();
void check_result(char*, int);
void send_advertisment(int);

// structure for all routers in network
struct neighbouring_routers{
 char ip_addr[15];
 int is_neighbour;
};

//routing table structure
typedef struct{
	char destination[15], next_hop[15];
	int cost, ttl, from;
}route_entry;

route_entry routing_table[200]; 

struct neighbouring_routers neighbours[200];

//prints the neighbours
void print_neighbours(){
	int i;
	for(i=0;i<node_count;i++){
		printf("neighbour %d\t ip address %s\t is_neighbour %d\n",i, neighbours[i].ip_addr, neighbours[i].is_neighbour);
	}
}

/** 
* gets ip address of the local machine
**/
void get_ip_address(){
  struct addrinfo hints, *res;
  int errcode;
  char hostname[50];
  struct sockaddr_in *addr;

  if(gethostname(hostname, 50)!=0){
   printf("error in getting hostname, errno ");
  }
  printf("hostname is %s\n", hostname);

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype = SOCK_DGRAM;

  if((getaddrinfo(hostname, NULL, &hints, &res))!=0){
    perror("getaddrinfo error: ");
  }
  while(res!=NULL){
   inet_ntop(AF_INET, res->ai_addr->sa_data, my_ip, INET_ADDRSTRLEN);
  
  addr = (struct sockaddr_in *)res->ai_addr;
  strcpy(my_ip, inet_ntoa((struct in_addr)addr->sin_addr)); 

  printf("My ip adress is %s\n",my_ip);
   res=res->ai_next;
  }
}

/** reads config file and detetermines the ip address of all routers 
* and identifies the neighbouring routers
**/
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
  get_ip_address();
  strcpy(neighbours[0].ip_addr, my_ip);
  neighbours[0].is_neighbour = -1;
	while((fgets(line, 256, fp) != NULL)&&(line[0]!='\n'))
	{
		ip_address = strtok(line, " ");
    if(ip_address == NULL){
     break;
    }
		neighbour = strtok(NULL, "\0");
		strcpy(neighbours[node_count].ip_addr, ip_address);
		if(node_count == 0)
		{
			//neighbours[node_count].is_neighbour = -1;
		}
		else
		{
			neighbours[node_count].is_neighbour = (((strncmp(neighbour, "yes", 3) == 0) || (strncmp(neighbour, "YES", 3) == 0)) ? 1: 0);
		}
		printf("ip_address %s\nneighbour %s\n", ip_address, neighbour);
		node_count++;		
	}
	print_neighbours();
	printf("Total nodes in this network is %d\n",node_count);
}

/**
* prints the header
**/
void print_header(){
	int i;
	for(i=0;i<node_count;i++){
		printf("%d\t",i);
	}
	printf("\n");
}

/** 
* prints distance and pi(predecessor) array
**/
void print_array(){
 int i;
 printf("Distance Array\n");
 print_header();
 for(i=0;i<node_count;i++){
 	printf("%d\t",dist[i]);
 }
 printf("\n\nPi Array\n");
 print_header();
 for(i=0;i<node_count;i++){
 	printf("%d\t",pi[i]);
 }
 printf("\n");
}

/** 
* prints the graph
**/
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


/** 
* allocates and intialises graph with infinity
**/
void allocate(){
	int i, j;

  for(i=0;i<node_count;i++){
  	for(j=0;j<node_count;j++){
  		graph[i][j]=infinity;
  	}
  }

  dist = (int*)calloc(node_count, sizeof(int));
  pi = (int*)calloc(node_count, sizeof(int));

  print_graph();
}

/**
* prints the routing table
**/
void print_routing_table(){
	int i;
  printf("\nRouting table\n\n");
	printf("Node\t\tNext Hop\tcost\tTTL\tSN\n");
	for(i=0;i<node_count;i++){
		printf("%s\t%s\t%d\t%d\t%d\n", routing_table[i].destination, routing_table[i].next_hop, routing_table[i].cost, routing_table[i].ttl, routing_table[i].from);
	}
}

/** updates routing table after bellman ford is run
* this function is thread safe
**/
void update_routing_table(){
  pthread_mutex_lock(&update_mutex);
  int i;
  for(i=0;i<node_count;i++){
  	strcpy(routing_table[i].destination,neighbours[i].ip_addr);

  	if(((routing_table[i].cost != dist[i]) || ((strcmp(routing_table[i].next_hop, neighbours[pi[i]].ip_addr))!=0))&&(i!=0)){
  		routing_table[i].from = source_node;
  	}

    if(((strcmp(routing_table[i].next_hop, neighbours[pi[i]].ip_addr)!=0)||(routing_table[i].cost != dist[i]))&& (pi[i]!=-1))
	{
      is_routing_table_changed=1;
	}

  	if(pi[i]==-1){
  		strcpy(routing_table[i].next_hop,"Null          ");
  	}
  	else
  	{
  		strcpy(routing_table[i].next_hop, neighbours[pi[i]].ip_addr);
  	}
  	routing_table[i].cost = dist[i];

  	if(is_initialised==0){
  		routing_table[i].from = -1;
  		routing_table[i].ttl = ttl;
  	}
  }
  is_initialised = 1;
  //print_routing_table(); Tempororily comment this.
  pthread_mutex_unlock(&update_mutex);
}

/** This function implements bellman ford and populates
* distance pi array
**/
void bellman_ford(){
 pthread_mutex_lock(&bellman_mutex);
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
 pthread_mutex_unlock(&bellman_mutex);
}

/** 
* intialises the graph when router startsup
**/
void intialise_graph(){
	int i,j;
	for(j=1;j<node_count;j++){
 		graph[0][j] = (neighbours[j].is_neighbour == 1 ? 1 : infinity);
	}
	graph[0][0] = 0;
	print_graph();
}

/**
* prepares the advertisement 
* if split horizon is on,then required action in update message will be taken
**/
void prepare_advertisement(){
  pthread_mutex_lock(&adv_mutex);
	int i,node_no=1, j = 0, k=0, x=0, y=0;
	unsigned char transform;
	long path_cost;
	unsigned int p;
	unsigned char temp_ip_addr[15], part[3];
 for(node_no=1;node_no<node_count;node_no++){
 x = y = k = 0;
 if(neighbours[node_no].is_neighbour==1){
  advertise_contents = (char*)calloc(node_count*8, sizeof(char));
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
    if((routing_table[i].from == node_no) && (split_horizon==1))
     path_cost = infinity;
    else
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
   send_advertisment(node_no);
  }
 }
 pthread_mutex_unlock(&adv_mutex);
}

/** 
* Helper function to get vertex number in 2D matrix
**/
int get_vertex_number(char *ip){
	int  i;
	for(i=0;i<node_count;i++){
		if(strcmp(ip,neighbours[i].ip_addr)==0){
      return i;
		}
	}
	return -1;
}

/** 
* After router receives the update from the router, its ttl will be set to default value
**/
void set_ttl_to_default(int node){
	if(node>=node_count){
		printf("Invalid node number %d\n", node);
		exit(1);
	}
        graph[0][node]=1;
        //printf("Setting deault ttl to node %s\n",neighbours[node].ip_addr);
	routing_table[node].ttl = ttl;
}

/**
* Reduces ttl by period seconds
**/
void reduce_ttl(int node, int seconds){
 if(neighbours[node].is_neighbour==1 && routing_table[node].ttl > 0){
  routing_table[node].ttl = routing_table[node].ttl - period;
  if(routing_table[node].ttl <= 0){
	routing_table[node].ttl = 0; 
    graph[0][node] = infinity;
    bellman_ford();
    print_graph();  
  }
 } 
}

/**
* the received update message is interpretted here
**/
void interpret_advertisement(unsigned char *advertise_contents, int seconds){
  int i=0, destination_node;
  unsigned char ip[15];
  unsigned long get_cost=0, temp=0;
  printf("\n***UPDATE RECEIVED***\n");
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
    		printf("get_vertex_number returned -1\n");
    		exit(1);
    	}
    	set_ttl_to_default(source_node);
        graph[source_node][0] = 1;
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
    	//reduce_ttl(destination_node, seconds);
    	printf("destination_node is %d\n", destination_node);
    	graph[source_node][destination_node] = ((get_cost > infinity) ? infinity : get_cost);
    }
		i+=8;
	}
	bellman_ford();
}


/**
* sends advertisement to neighbours
**/
void send_advertisment(int i){
  int  result;
  //for(i=0;i<node_count;i++){
  	if(neighbours[i].is_neighbour == 1){
  		neighbour_addr.sin_family = PF_INET;
  		neighbour_addr.sin_addr.s_addr = inet_addr(neighbours[i].ip_addr);
  		neighbour_addr.sin_port = htons(port_no);
  		result = sendto(sock, advertise_contents, 8*node_count, 0, (struct sockaddr*)&neighbour_addr, sizeof(neighbour_addr));
  		printf("sendto to ip address %s & port is %d is %d\n", neighbours[i].ip_addr, port_no, result);
      free(advertise_contents);
  	}
  //}
}

/**
* receives advertisement from neighbours
**/
void receive_advertisement(){
	int i, result;
	char received_advertisement[node_count*8];	
	printf("Waiting to receive update\n");
	recvfrom(sock, received_advertisement, node_count*8, 0, (struct sockaddr*)&neighbour_addr, &neighbour_addr_length);
	printf("***UPDATE RECEIVED***\n");
	interpret_advertisement(received_advertisement, 0);  //TODO: Look into this again
}

/**
* creates & binds socket
**/
void socket_creation(){
 server_addr.sin_family = AF_INET;
 server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 server_addr.sin_port = htons(port_no);
 neighbour_addr_length = sizeof(neighbour_addr);
 create_socket();
 bind_socket();
}

/** 
* intialisation step
**/
void intialise(){
  allocate();
  intialise_graph();
  bellman_ford();
  socket_creation();
  prepare_advertisement(); 
} 

/** 
*function which waits for period seconds and sends the periodic update
**/
void *periodic_update_function()
{
	int i;
	while(1)
	{
		sleep(period);
		for(i=1;i<node_count;i++)
		{
			reduce_ttl(i, period);
		}
		prepare_advertisement();
        //send_advertisment();
        printf("\nSENDING FOLLOWING TABLE\n");
        print_routing_table();
		
	}
}

/**
* function to intialise the mutexes
**/
void init_mutexes(){
 if(pthread_mutex_init(&update_mutex, NULL)!=0){
  printf("mutex init failed for update\n");
  exit(1);
 }
 if(pthread_mutex_init(&bellman_mutex, NULL)!=0){
  printf("mutex init failed for bellman\n");
  exit(1);
 }
 if(pthread_mutex_init(&adv_mutex, NULL)!=0){
  printf("mutex init failed for advertisement\n");
  exit(1);
 }
}

/**
* function to destroy mutexes
**/
void destroy_mutexes(){
 pthread_mutex_destroy(&bellman_mutex);
 pthread_mutex_destroy(&update_mutex);
 pthread_mutex_destroy(&adv_mutex);
}

int main(int argc, char* argv[]){
	pthread_t periodic_thread, triggered_thread;
	int result,th_ret1;
	char *received_advertisement = (char*) malloc(100*sizeof(char));
	if(argc!=7){
		printf("Invalid number of arguments\n. Please pass 'Config_file_name port_no TTL infinity period split_horizon'\n");
		exit(1);
	}
  strcpy(config_file_name, argv[1]);
  port_no = atoi(argv[2]);
  ttl = atoi(argv[3]);
  infinity = atoi(argv[4]);
  period = atoi(argv[5]);
  split_horizon = atoi(argv[6]);
  printf(" Config file name %s\n port no %d\nTTL %d\ninfinity %ld\nperiod %d\nsplit_horizon %d\n", config_file_name, port_no, ttl, infinity, period, split_horizon);
  read_config_file();
  intialise();
  received_advertisement = realloc(received_advertisement, (node_count * 8));
  th_ret1 = pthread_create( &periodic_thread, NULL, periodic_update_function, NULL);
  while(1)
  {
	  result = recvfrom(sock, received_advertisement, node_count*8, 0, (struct sockaddr*)&neighbour_addr, &neighbour_addr_length);
	  
	  if(result<0)
	  {
		  printf("Nothing Received\n");
		  continue;
	  }
	  interpret_advertisement(received_advertisement, period);
      
	  if(is_routing_table_changed == 1)
	  {
		  printf("***********Routing table has changed************\n");
		  prepare_advertisement();
		  is_routing_table_changed=0;
		  print_routing_table();
	  }
    else
    {
     printf("***ROUTING TABLE NOT CHANGED\n");
     print_routing_table();
    }
  }
  //update();
  return 0;
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
