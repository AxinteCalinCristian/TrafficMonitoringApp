#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <utmp.h>
#include <paths.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include <sqlite3.h>
#include <pthread.h>

#include "utils.h"

extern int errno;
sqlite3 * sqlite_db;

struct Client clients[MAX_NO_OF_CLIENTS];

void fail(const char* msg, int terminate, int sockfd) {
  perror(msg);
  if(terminate) {
  	close(sockfd);
  	exit(-1);
  }
}

void road_max_speed(struct Info_Message * resp, struct Info_Message * info_msg) {
	char input[INFO_CHUNK / 2];
	sprintf(input, "select * from roads where name='%s';", info_msg->event_location);
	struct sqlite3_stmt* select_road;
	
	resp->msg_type = SPEED_RESTRICTION;
	resp->action_response = ACTION_ACCEPTED;
	strcpy(resp->event_location, info_msg->event_location);
	strcpy(resp->user_name, info_msg->user_name);
	
	int res = sqlite3_prepare_v2(sqlite_db, input, -1, &select_road, NULL);
	int speed_restr = -1;
	
	if(res == SQLITE_OK)
	{
		if (sqlite3_step(select_road) == SQLITE_ROW) {
			speed_restr = (int)sqlite3_column_int(select_road, 2);
		}
	}
	
	if(speed_restr != -1) {
		sprintf(resp->speed_value, "%d", speed_restr);
		return;
	}
	
	speed_restr = rand() % 60 + 35;
	
	memset(input,0 , strlen(input));
	sprintf(input, "insert into roads (name, max_speed) values ('%s', %d);", info_msg->event_location, speed_restr);
	res = sqlite3_exec(sqlite_db, input, NULL, 0, NULL);
	
	if(res != SQLITE_OK) {
		speed_restr = -1;
	}
	sprintf(resp->speed_value, "%d", speed_restr);
	
	if(speed_restr == -1) {
		resp->action_response = ACTION_FAILED;
	}
}

void update_user_speed(struct Info_Message * resp, struct Info_Message * info_msg) {
	char input[INFO_CHUNK / 4];
	sprintf(input, "update users set last_recorded_speed='%s' where username='%s';", info_msg->speed_value, info_msg->user_name);
	sqlite3_exec(sqlite_db, input, NULL, 0, NULL);
	resp->msg_type = CAR_SPEED_VALUE;
	resp->action_response = ACTION_ACCEPTED;
}

void subscription_change(int type, struct Info_Message * resp, struct Client * client) {
	char input[INFO_CHUNK / 4];
	sprintf(input, "select * from users where username='%s';", client->user_name);
	struct sqlite3_stmt* select_user;
	strcpy(resp->user_name, client->user_name);
	if(type == 1) {
		resp->msg_type =  USER_SUBSCRIBE;
		
	}else if( type == -1) {
		resp->msg_type =  USER_UNSUBSCRIBE;
	}
	int rez = 0;
	int res = sqlite3_prepare_v2(sqlite_db, input, -1, &select_user, NULL);
	char wants_updates;
	if(res == SQLITE_OK)
	{
		if (sqlite3_step(select_user) == SQLITE_ROW) {
			wants_updates = sqlite3_column_text(select_user, 2)[0];
		} else {
			rez = -1;
		}
	}
	sqlite3_finalize(select_user);
	memset(input, 0, strlen(input));
	memset(select_user, 0, sizeof(select_user));
	
	if(type == 1 && wants_updates == 'F') {
		sprintf(input, "update users set wants_updates='T' where username='%s';", client->user_name);
		res = sqlite3_exec(sqlite_db, input, NULL, 0, NULL);
		if(res == SQLITE_OK) {
			rez = 1;
			client->send_updates = 1;
		}
		else {
			rez = -1;
		}
	}
	else if(type == -1 && wants_updates == 'T') {
		sprintf(input, "update users set wants_updates='F' where username='%s';", client->user_name);
		res = sqlite3_exec(sqlite_db, input, NULL, 0, NULL);
		if(res == SQLITE_OK) {
			rez = 1;
			client->send_updates = 0;
		}
		else {
			rez = -1;
		}
	}
	
	if(rez == 1) {
		resp->action_response = ACTION_ACCEPTED;
	}
	else if(rez == 0){
		resp->action_response = ACTION_REJECTED;
	}
	else {
		resp->action_response = ACTION_FAILED;
	}
}

void login_user_attempt(struct Info_Message * resp, struct Info_Message * info_msg, struct Client * client) {
	char input[INFO_CHUNK / 4];
	sprintf(input, "select * from users where username='%s';", info_msg->user_name);
	struct sqlite3_stmt* select_user;
	
	int res = sqlite3_prepare_v2(sqlite_db, input, -1, &select_user, NULL);
	if(res == SQLITE_OK)
	{
		if (sqlite3_step(select_user) == SQLITE_ROW)
		{	
			// user found
			resp->msg_type = USER_LOGIN;
			strcpy(resp->user_name, info_msg->user_name);
			resp->action_response = ACTION_ACCEPTED;
			memset(client->user_name, 0, strlen(client->user_name));
			strcpy(client->user_name, info_msg->user_name);
			char wants_updates = sqlite3_column_text(select_user, 2)[0];
			if(wants_updates == 'T') {
				client->send_updates = 1;
				resp->wants_updates = 1;
			}
		}
		else
		{	
			// user not found
		 	resp->msg_type = USER_LOGIN;
			strcpy(resp->user_name, info_msg->user_name);
			resp->action_response = ACTION_REJECTED;
		}
	}
	sqlite3_finalize(select_user);
}

int register_user(char user_name[]) {
	int resp = 0;
	
	char input[INFO_CHUNK / 4];
	sprintf(input, "insert into users (username, wants_updates) values ('%s', 'F');", user_name);
	struct sqlite3_stmt* insert_user;
	
	if(sqlite3_exec(sqlite_db, input, NULL, 0, NULL) == SQLITE_OK) {
	 resp = 1;
	}
	
	sqlite3_finalize(insert_user);
	return resp;
}

void register_user_attempt(struct Info_Message * resp, struct Info_Message * info_msg, struct Client * client) {
	char input[INFO_CHUNK / 4];
	sprintf(input, "select * from users where username='%s';", info_msg->user_name);
	struct sqlite3_stmt* select_user;

	int res = sqlite3_prepare_v2(sqlite_db, input, -1, &select_user, NULL);
	if(res == SQLITE_OK)
	{
		if (sqlite3_step(select_user) == SQLITE_ROW)
		{	
			// user found
			resp->msg_type = USER_REGISTER;
			strcpy(resp->user_name, info_msg->user_name);
			resp->action_response = ACTION_REJECTED;
		}
		else
		{	
			// user not found
			resp->msg_type = USER_REGISTER;
			strcpy(resp->user_name, info_msg->user_name);
			int reg = register_user(info_msg->user_name);
			if(reg == 1){
				resp->action_response = ACTION_ACCEPTED;
			}
			else {
				resp->action_response = ACTION_FAILED;
			}
		 	
		}
		
	}
	else
	{	
		// user not found
		resp->msg_type = USER_REGISTER;
		strcpy(resp->user_name, info_msg->user_name);
		int reg = register_user(info_msg->user_name);
		if(reg == 1){
			resp->action_response = ACTION_ACCEPTED;
		}
		else {
			resp->action_response = ACTION_FAILED;
		}
	 	
	}

	sqlite3_finalize(select_user);
}

void logout_user(struct Info_Message * resp, struct Info_Message * info_msg, struct Client * client) {
	resp->msg_type = USER_DISCONNECT;
	resp->action_response = ACTION_ACCEPTED;
	strcpy(resp->user_name, client->user_name);
	
	memset(client->user_name, 0, strlen(client->user_name));
	client->send_updates = 0;
}

void submit_traffic_incident(struct Info_Message * resp, struct Info_Message * info_msg) {
	resp->msg_type = TRAFFIC_INCIDENT;
	resp->action_response = ACTION_ACCEPTED;
	strcpy(resp->user_name, info_msg->user_name);
	
	struct Info_Message report;
	report.msg_type = TRAFFIC_INCIDENT;
	report.action_response = NO_ACTION;
	memset(report.user_name, 0, strlen(report.user_name));
	strcpy(report.user_name, info_msg->user_name);
	memset(report.event_location, 0, strlen(report.event_location));
	strcpy(report.event_location, info_msg->event_location);
	memset(report.event_details, 0, strlen(report.event_details));
	strcpy(report.event_details, info_msg->event_details);
	
	format_and_print_message(-1, &report);
	
	for(int i = 0; i < MAX_NO_OF_CLIENTS; i++) {
		if(strcmp(info_msg->user_name, clients[i].user_name) !=0) {
			send(clients[i].fd, (struct Info_Message*) &report, sizeof(report), 0);
		}
	}
}

void parseMessage(struct Info_Message info_msg, struct Client * client) {
	int client_fd = client->fd;
	struct Info_Message resp;
	memset(&resp, 0, sizeof(resp));
	
	format_and_print_message(1, &info_msg);
	
	if(info_msg.msg_type == USER_LOGIN) {
		login_user_attempt(&resp, &info_msg, client);
	}
	else if(info_msg.msg_type == USER_REGISTER) {
		register_user_attempt(&resp, &info_msg, client);
	}
	else if(info_msg.msg_type == USER_SUBSCRIBE) {
		subscription_change(1, &resp, client);
	}
	else if(info_msg.msg_type == USER_UNSUBSCRIBE) {
		subscription_change(-1, &resp, client);
	}
	else if(info_msg.msg_type == CAR_SPEED_VALUE) {
		update_user_speed(&resp, &info_msg);
	}
	else if(info_msg.msg_type == USER_DISCONNECT) {
		logout_user(&resp, &info_msg, client);
	}
	else if(info_msg.msg_type == TRAFFIC_INCIDENT) {
		submit_traffic_incident(&resp, &info_msg);
	}
	else if(info_msg.msg_type == SPEED_RESTRICTION) {
		road_max_speed(&resp, &info_msg);
	}
	
	format_and_print_message(-1, &resp);
	send(client_fd, (struct Info_Message*) &resp, sizeof(resp), 0);
}

void send_news_update() {
	struct Info_Message report;
	report.msg_type = TRAFFIC_EVENT;
	
	char event_info[INFO_CHUNK];
	generate_rand_event(event_info);	
	memset(report.event_details, 0, strlen(report.event_details));
	memset(report.user_name, 0, strlen(report.user_name));
	strcpy(report.event_details, event_info);
	
	int should_send = rand() % 100 + 1;
	if(rand() % 2 == 0) {
		format_and_print_message(-1, &report);
		for(int i = 0; i < MAX_NO_OF_CLIENTS; i++) {
			if(clients[i].send_updates == 1) {
				send(clients[i].fd, (struct Info_Message*) &report, sizeof(report), 0);
			}
		}
	}
	
}

void send_news_handler() {
	while(1) {
		send_news_update();
		sleep(2);
	}
}

int main() {
	srand(time(NULL));

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);           
	if (sockfd < 0) fail("Error on socket function", 1, sockfd); 

	struct sockaddr_in server_sockaddr;
	struct sockaddr_in client_sockaddr;
	memset(&server_sockaddr, 0, sizeof(server_sockaddr));
	memset(&client_sockaddr, 0, sizeof(client_sockaddr));

	server_sockaddr.sin_family = AF_INET;   
	server_sockaddr.sin_addr.s_addr = INADDR_ANY;
	server_sockaddr.sin_port = htons(SERVER_PORT);

	if (bind(sockfd, (struct sockaddr *) &server_sockaddr, sizeof(server_sockaddr)) == -1) {
		fail("Error on bind function", 1, sockfd);
	}

	if (listen(sockfd, MAX_NO_OF_CLIENTS) == -1){ 
		fail("Error listening for clients", 1, sockfd);
	}
	
	int max_fd = 0, new_sock, client;
	memset(clients, 0, sizeof(clients));
	
	fd_set readfds;
	
	struct Info_Message info_msg;
	
	if(sqlite3_open(SQLITE_DB, &sqlite_db) !=0){
		sqlite3_close(sqlite_db);
		fail("Error opening the database", 1, sockfd);
	}
	
	pthread_t send_news_thread;
	
	if(pthread_create(&send_news_thread, NULL, (void *) send_news_handler, NULL) != 0){
		fail("Error on send_news pthread_create function", 1, sockfd);
	}
	
  	while(1) {
  		FD_ZERO (&readfds);	
  		FD_SET (sockfd, &readfds);	
  		max_fd = sockfd;
  		for(int i=0;i<MAX_NO_OF_CLIENTS;i++){
  			if(clients[i].fd > 0)
  				FD_SET(clients[i].fd, &readfds);

  			if(clients[i].fd > max_fd)
  				max_fd = clients[i].fd;
  		}
  		
  		if((client = select(max_fd + 1, &readfds, NULL, NULL, NULL)) < 0){
  			printf("Select function error!\n");
  		}
  		
  		if(FD_ISSET(sockfd, &readfds)) {
  			if((new_sock = accept(sockfd, (struct sockaddr *)&client_sockaddr, (socklen_t*)&client_sockaddr)) < 0){
  				fail("Error on accept function", 1, sockfd);
  			}
  			
  			for (int i = 0; i < MAX_NO_OF_CLIENTS; i++) {  
				if(clients[i].fd == 0 ){  
				    clients[i].fd = new_sock;  
				    break;  
				}  
			}  
  		}
  		
  		for (int i = 0; i < MAX_NO_OF_CLIENTS; i++)  
		{   	 
		    if(FD_ISSET(clients[i].fd , &readfds))  
		    {  
		    	int bytes_read = read(clients[i].fd, (struct Info_Message*) &info_msg, sizeof(info_msg));
			if (bytes_read == 0) {  
				// client disconnected

				close(clients[i].fd);  
				clients[i].fd = 0;
				clients[i].send_updates = 0;
				memset(clients[i].user_name, 0, strlen(clients[i].user_name));
			}  
			else if (bytes_read > 0)
			{  	
				parseMessage(info_msg, &(clients[i]));
			}
		    }  
		}
  		
  	}
	
	printf("Shutting down server\n");
	close(sockfd);
	sqlite3_close(sqlite_db);
	return 0;

}
