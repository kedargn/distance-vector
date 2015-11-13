#include<stdio.h>
#include<stdlib.h>
#include<string.h>

char config_file_name[100];
int ttl, port_no, period, split_horizon;     //split horizon can be either 1 or 0
long infinity;





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
	line = (char*)malloc(25);
	while((ch = getc(fp))!=EOF){
		if(ch != '\n'){
      line[i++] = ch;
		} 
		else 
		{
			//printf("line is %s\n", line);
			ip_address = strtok(line, " ");
			neighbour = strtok(NULL, " ");
			printf("ip_address %s & neighbour %s\n", ip_address, neighbour);
			free(line);
			line = (char*)malloc(25);
			i = 0;
		}
	}
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
}