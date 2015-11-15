#include<stdio.h>
#include<stdlib.h>
#include<string.h>

char config_file_name[100];
int ttl, port_no, period, split_horizon, node_count=1, graph[100][100], *dist, *pi;     //split horizon can be either 1 or 0
long infinity;

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
  strcpy(neighbours[0].ip_addr, "120.0.0.1");
  neighbours[0].is_neighbour = -1;
	while(fgets(line, 256, fp) != NULL)
	{
		ip_address = strtok(line, " ");
		neighbour = strtok(NULL, "\0");
		strcpy(neighbours[node_count].ip_addr, ip_address);
		neighbours[node_count].is_neighbour = (strncmp(neighbour, "yes", 3) == 0 ? 1: 0);
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

void intialise(){
  allocate();
  intialise_graph();
  bellman_ford();
} 

int main(int argc, char* argv[]){
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
}