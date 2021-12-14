#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <utmp.h>
#include <paths.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <pthread.h>
#include <X11/Xlib.h>
 
#include "ClientUI/client_ui.h"
#include "utils.h"

// global user vars
int sockfd;
int close_client 	= 0;
int logged_in 		= 0;
int wants_updates 	= 0;
char user_name		[INFO_CHUNK / 8];
int user_speed		= 0;
char curr_location     [INFO_CHUNK / 4];

// ui structs

int show_gui 		= 1;
GtkApplication	* 	app_ui;
struct Login_Screen 	login_screen;
struct Main_Screen 	main_screen;

// ui code

void add_new_event_to_list(char event[]) {
	if(show_gui == 0) {
		return;
	}
	
	gtk_grid_insert_row(GTK_GRID(main_screen.event_list), 0);
	
	GtkLabel* event_label = GTK_LABEL(gtk_label_new(event));
	gtk_label_set_justify(event_label, GTK_JUSTIFY_LEFT);
	gtk_label_set_xalign(event_label, 0.0);
	
	char attrs[INFO_CHUNK * 2];
	memset(attrs, 0, strlen(attrs));
	sprintf(attrs, "<span font_desc=\"Sawasdee Bold 12\">%s</span>", event);
	gtk_label_set_markup(event_label, attrs);
	gtk_label_set_line_wrap(event_label, TRUE);
	gtk_widget_set_margin_top(GTK_WIDGET(event_label), 3);
	gtk_widget_set_margin_start(GTK_WIDGET(event_label), 4);
	gtk_widget_set_margin_end(GTK_WIDGET(event_label), 4);
	gtk_widget_set_margin_bottom(GTK_WIDGET(event_label), 3);
	
	gtk_grid_attach(GTK_GRID(main_screen.event_list), GTK_WIDGET(event_label), 1, 0, 1, 1);
	gtk_widget_show_all(GTK_WIDGET(main_screen.event_list));
}

void toggle_news_subscription(GtkWidget *widget, gpointer data) {
	struct Info_Message to_send;
	memset(&to_send, 0, sizeof(to_send));
	
	if(gtk_switch_get_state(GTK_SWITCH(widget)) == FALSE) {
		to_send.msg_type = USER_SUBSCRIBE;
		strcpy(to_send.user_name, user_name);
		send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
	} else {
		to_send.msg_type = USER_UNSUBSCRIBE;
		strcpy(to_send.user_name, user_name);
		send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
	}
}

void click_login(GtkWidget *widget, gpointer data)
{	
	const char * input;
	input = gtk_entry_get_text(GTK_ENTRY(data));
	
	struct Info_Message to_send;
	memset(&to_send, 0, sizeof(to_send));
	to_send.msg_type = USER_LOGIN;
	memset(to_send.user_name, 0, strlen(to_send.user_name));
	strcpy(to_send.user_name, input);
	
	send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
}

void click_register(GtkWidget *widget, gpointer data)
{	
	const char * input;
	input = gtk_entry_get_text(GTK_ENTRY(data));
	
	struct Info_Message to_send;
	memset(&to_send, 0, sizeof(to_send));
	to_send.msg_type = USER_REGISTER;
	memset(to_send.user_name, 0, strlen(to_send.user_name));
	strcpy(to_send.user_name, input);
	
	send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
}

void click_logout(GtkWidget *widget, gpointer data)
{	
	struct Info_Message to_send;
	memset(&to_send, 0, sizeof(to_send));
	to_send.msg_type = USER_DISCONNECT;
	strcpy(to_send.user_name, user_name);
	send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
}

void close_gapp(GtkWidget *widget, gpointer data) {

	if(login_screen.window != NULL)
		gtk_window_close(GTK_WINDOW(login_screen.window));
	if(main_screen.window != NULL)
		gtk_window_close(GTK_WINDOW(main_screen.window));
	
	g_application_quit(G_APPLICATION(data));
	
}

void click_submit_event(GtkWidget *widget, gpointer data) {	
	if(strlen(gtk_entry_get_text(GTK_ENTRY(main_screen.event_location))) == 0 || 
	strlen(gtk_entry_get_text(GTK_ENTRY(main_screen.event_details))) == 0 ) {
		return;
	}
	
	struct Info_Message to_send;
	memset(&to_send, 0, sizeof(to_send));
	
	to_send.msg_type = TRAFFIC_INCIDENT;
	strcpy(to_send.user_name, user_name);
	strcpy(to_send.event_location, gtk_entry_get_text(GTK_ENTRY(main_screen.event_location)));
	strcpy(to_send.event_details, gtk_entry_get_text(GTK_ENTRY(main_screen.event_details)));
	
	send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
	
	char event[INFO_CHUNK * 2];
	memset(event, 0, strlen(event));
	sprintf(event, "%s (YOU): Traffic incident on: %s. Summary: %s", to_send.user_name, 
		to_send.event_location, to_send.event_details);
		
	add_new_event_to_list(event);
	
	gtk_entry_set_text(GTK_ENTRY(main_screen.event_location), "");
	gtk_entry_set_text(GTK_ENTRY(main_screen.event_details), "");
	
}

void activate (GtkApplication* app, gpointer user_data)
{	
	// build login_screen
	login_screen.builder = gtk_builder_new();
	gtk_builder_add_from_file(login_screen.builder, LOGIN_UI, NULL);
	login_screen.window = gtk_builder_get_object(login_screen.builder, "main_window");
	g_signal_connect(G_OBJECT(login_screen.window), "destroy", G_CALLBACK(close_gapp), app);
	
	login_screen.user_entry = gtk_builder_get_object(login_screen.builder, "login_entry");
	
	login_screen.login_button = gtk_builder_get_object(login_screen.builder, "login_btn");
	g_signal_connect (login_screen.login_button, "clicked", G_CALLBACK(click_login), login_screen.user_entry);
	
	login_screen.register_button = gtk_builder_get_object(login_screen.builder, "register_btn");
	g_signal_connect (login_screen.register_button, "clicked", G_CALLBACK(click_register), login_screen.user_entry);
	
	login_screen.warning_label = GTK_LABEL(gtk_builder_get_object(login_screen.builder, "signin_warning"));
	
	gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(login_screen.window));
	
	if(login_screen.window != NULL && GTK_IS_WIDGET(login_screen.window)) {
		gtk_widget_show(GTK_WIDGET(login_screen.window));
	}
	
	g_object_unref(login_screen.builder);
	
	// build main_screen
	main_screen.builder = gtk_builder_new();
	gtk_builder_add_from_file(main_screen.builder, MAIN_UI, NULL);
	main_screen.window = gtk_builder_get_object(main_screen.builder, "main_window");
	g_signal_connect(G_OBJECT(main_screen.window), "destroy", G_CALLBACK(close_gapp), app);
	
	main_screen.logout_btn = gtk_builder_get_object(main_screen.builder, "logout_btn");
	g_signal_connect(G_OBJECT(main_screen.logout_btn), "activate", G_CALLBACK(click_logout), NULL);
	
	main_screen.news_subscription = gtk_builder_get_object(main_screen.builder, "subscribe_btn");
	
	main_screen.current_user = GTK_LABEL(gtk_builder_get_object(main_screen.builder, "current_user"));
	main_screen.current_location = GTK_LABEL(gtk_builder_get_object(main_screen.builder, "current_location"));
	
	main_screen.event_location = gtk_builder_get_object(main_screen.builder, "submit_location");
	main_screen.event_details = gtk_builder_get_object(main_screen.builder, "submit_details");
	
	main_screen.submit_event = gtk_builder_get_object(main_screen.builder, "submit_event");
	g_signal_connect(main_screen.submit_event, "clicked", G_CALLBACK(click_submit_event), NULL);
	main_screen.event_list = GTK_WIDGET(gtk_builder_get_object(main_screen.builder, "event_list_grid"));
	
	gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(main_screen.window));
	
}

// code

void fail(const char* msg, int terminate, int sockfd) {
  perror(msg);
  if(terminate) {
  	close(sockfd);
  	exit(-1);
  }
}

void get_user_input(struct User_Input * user_in) {
	long unsigned int BUFFER_SIZE = INFO_CHUNK;
	char * buffer = (char*) malloc(BUFFER_SIZE);

	getline(&buffer, &BUFFER_SIZE, stdin);
	
	if(strlen(buffer) == 0)
	{
		return;
	}
	
	buffer[strlen(buffer) - 1] = 0;
	struct User_Input tmp = userInputParser(buffer);
	memset(user_in, 0, sizeof(struct User_Input));

	strcpy(user_in->type, tmp.type);
	strcpy(user_in->user_name, tmp.user_name);
	strcpy(user_in->location, tmp.location);
	strcpy(user_in->summary, tmp.summary);
}

void convert_user_input_to_packet(struct User_Input * user_in , int sockfd) {
	struct Info_Message to_send;
	memset(&to_send, 0, sizeof(to_send));
	
	if(logged_in == 1) {
		if(strcmp(user_in->type, "report_incident") == 0) {
			to_send.msg_type = TRAFFIC_INCIDENT;
			strcpy(to_send.user_name, user_name);
			strcpy(to_send.event_location, user_in->location);
			strcpy(to_send.event_details, user_in->summary);
			
			send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
		}
		else if(strcmp(user_in->type, "subscribe") == 0) {
			to_send.msg_type = USER_SUBSCRIBE;
			strcpy(to_send.user_name, user_name);
			send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
		}
		else if(strcmp(user_in->type, "unsubscribe") == 0) {
			to_send.msg_type = USER_UNSUBSCRIBE;
			strcpy(to_send.user_name, user_name);
			send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
		}
		else if(strcmp(user_in->type, "logout") == 0) {
			to_send.msg_type = USER_DISCONNECT;
			strcpy(to_send.user_name, user_name);
			send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
		}
		
	} else {
		if(strcmp(user_in->type, "register") == 0) {
			to_send.msg_type = USER_REGISTER;
			memset(to_send.user_name, 0, strlen(to_send.user_name));
			strcpy(to_send.user_name, user_in->user_name);
			
			send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
		}
		
		if(strcmp(user_in->type, "login") == 0) {
			to_send.msg_type = USER_LOGIN;
			memset(to_send.user_name, 0, strlen(to_send.user_name));
			strcpy(to_send.user_name, user_in->user_name);
			
			send(sockfd, (struct Info_Message*) &to_send, sizeof(to_send), 0);
		}
	}
}

void send_msg_handler() {
	struct User_Input user_in;

	while(1) {
		get_user_input(&user_in);
		
		if (strcmp(user_in.type, "quit") == 0) {
			memset(user_in.type, 0, strlen(user_in.type));
			strcpy(user_in.type, "logout");
			convert_user_input_to_packet(&user_in, sockfd);
			break;
		} else {
			convert_user_input_to_packet(&user_in, sockfd);
		}
	}
  
  close_client = 1;
}

void check_login_resp(struct Info_Message info_msg) {
	char gui_resp[INFO_CHUNK / 4];
	memset(gui_resp, 0, strlen(gui_resp));
	
	if(info_msg.action_response == ACTION_REJECTED) {
		printf("Username %s not found. Login failed.\n", info_msg.user_name);
		
		if(show_gui) {
			sprintf(gui_resp, "Username %s not found", info_msg.user_name);
			gtk_label_set_label(login_screen.warning_label, gui_resp);
		
		}
		return;
	}
	
	printf("Logged in as %s. Wants updates: %d\n", info_msg.user_name, info_msg.wants_updates);
	
	if(show_gui) {
		sprintf(gui_resp, "Logged in as %s", info_msg.user_name);
		gtk_label_set_label(login_screen.warning_label, gui_resp);
		
		if(info_msg.wants_updates) {
			gtk_switch_set_state(GTK_SWITCH(main_screen.news_subscription), TRUE);
		} else {
			gtk_switch_set_state(GTK_SWITCH(main_screen.news_subscription), FALSE);
		}
		
		g_signal_connect(main_screen.news_subscription, "state_set", G_CALLBACK(toggle_news_subscription), NULL);
		
		if(main_screen.window != NULL && login_screen.window != NULL &&
		   GTK_IS_WIDGET(login_screen.window) && GTK_IS_WIDGET(main_screen.window)) {
			gtk_widget_hide(GTK_WIDGET(login_screen.window));
			
			gtk_label_set_label(main_screen.current_user, info_msg.user_name);
			gtk_widget_show(GTK_WIDGET(main_screen.window));
		}
	}
	
	logged_in = 1;
	wants_updates = 1;
	memset(user_name, 0, strlen(user_name));
	strcpy(user_name, info_msg.user_name);
}

void check_logout_attempt(struct Info_Message info_msg) {
	if(info_msg.action_response == ACTION_REJECTED) {
		printf("Could not logout user %s\n", info_msg.user_name);
		return;
	}
	printf("User %s logged out.\n", user_name);
	
	if(show_gui) {
		if(main_screen.window != NULL && login_screen.window != NULL && 
		GTK_IS_WIDGET(login_screen.window) && GTK_IS_WIDGET(main_screen.window)) {
			gtk_widget_hide(GTK_WIDGET(main_screen.window));
			
			gtk_label_set_label(login_screen.warning_label, "");
			gtk_widget_show(GTK_WIDGET(login_screen.window));
		}
	}
	
	logged_in = 0;
	wants_updates = 0;
	memset(user_name, 0, strlen(user_name));
}

void check_subscribe_attempt(struct Info_Message info_msg) {
	if(info_msg.action_response == ACTION_FAILED) {
		printf("Error subscribing for updates\n");
		return;
	}
	else if(info_msg.action_response == ACTION_REJECTED) {
		printf("Already subscribed for updates\n");
		return;
	}
	printf("User %s now subscribed for updates.\n", info_msg.user_name);
	wants_updates = 1;
	
}

void check_unsubscribe_attempt(struct Info_Message info_msg) {
	if(info_msg.action_response == ACTION_FAILED) {
		printf("Error unsubscribing from updates\n");
		return;
	}
	else if(info_msg.action_response == ACTION_REJECTED) {
		printf("Not currently subscribed for updates\n");
		return;
	}
	printf("User %s now unsubscribed from updates.\n", info_msg.user_name);
	wants_updates = 0;
}

void check_traffic_report_attempt(struct Info_Message info_msg) {
	if(info_msg.action_response == ACTION_FAILED) {
		printf("Error submitting report\n");
		return;
	}
	else if(info_msg.action_response == ACTION_ACCEPTED) {
		printf("Traffic incident report successfully submitted\n");
		return;
	}
	else if(info_msg.action_response == NO_ACTION) {
		char input[INFO_CHUNK * 2];
		memset(input, 0, strlen(input));
		sprintf(input, "%s: Traffic incident on: %s. Summary: %s", info_msg.user_name, 
		info_msg.event_location, info_msg.event_details);
		add_new_event_to_list(input);
		
		printf("%s: Traffic incident on: %s. Summary: %s\n", info_msg.user_name, 
		info_msg.event_location, info_msg.event_details);
		return;
	}
}

void check_speed_restriction_query_attempt(struct Info_Message info_msg) {
	char gui_resp[INFO_CHUNK];
	memset(gui_resp, 0, strlen(gui_resp));
	
	if(info_msg.action_response == ACTION_FAILED) {
		printf("Error retrieving speed restriction for %s\n", info_msg.event_location);
		
		if(show_gui) {
			sprintf(gui_resp, "%s\nSpeed Restriction: Error", info_msg.event_location);
			gtk_label_set_label(main_screen.current_location, gui_resp);
		}
		
		return;
	}
	
	printf("Current location: %s. Speed restriction: %s\n", info_msg.event_location, info_msg.speed_value);
	
	if(show_gui) {
		sprintf(gui_resp, "%s\nSpeed Restriction: %s", info_msg.event_location, info_msg.speed_value);
		gtk_label_set_label(main_screen.current_location, gui_resp);
	}
}

void check_register_attempt(struct Info_Message info_msg) {
	char gui_resp[INFO_CHUNK];
	memset(gui_resp, 0, strlen(gui_resp));
	
	if(info_msg.action_response == ACTION_FAILED) {
		printf("Error registering user %s\n", info_msg.user_name);
		
		if(show_gui) {
			sprintf(gui_resp, "Error registering\n user %s", info_msg.user_name);
			gtk_label_set_label(login_screen.warning_label, gui_resp);
		}
		
		return;
	}
	else if(info_msg.action_response == ACTION_REJECTED) {
		printf("User %s already registered\n", info_msg.user_name);
		
		if(show_gui) {
			sprintf(gui_resp, "User %s\n already registered", info_msg.user_name);
			gtk_label_set_label(login_screen.warning_label, gui_resp);
		}
		
		return;
	}
	else if(info_msg.action_response == ACTION_ACCEPTED) {
		printf("User %s successfully registered\n", info_msg.user_name);
		
		if(show_gui) {
			sprintf(gui_resp, "User %s\n successfully registered", info_msg.user_name);
			gtk_label_set_label(login_screen.warning_label, gui_resp);
		}
		
		return;
	}
}

void check_traffic_event(struct Info_Message info_msg) {
	char input[INFO_CHUNK * 2];
	memset(input, 0, strlen(input));
	sprintf(input, "[New Event] %s", info_msg.event_details);
	printf("%s\n", input);
	add_new_event_to_list(input);
}

void receive_msg_handler() {
	struct Info_Message info_msg;
	int bytes_read;
	while(1) {
		memset(&info_msg, 0, sizeof(info_msg));
		bytes_read = read(sockfd, (struct Info_Message*) &info_msg, sizeof(info_msg));
		if(bytes_read == 0) {
			close_client = 1;
			break;
		}
		else if(bytes_read < 0) {
			fail("Error on read function", 1, sockfd);
		}
		else {
			/*
			printf("Type: %d, Event location: %s, Event details: %s, Username: %s\n", 
			info_msg.msg_type, info_msg.event_location, info_msg.event_details, info_msg.user_name);
			*/
			
			if(info_msg.msg_type == USER_LOGIN) {
				check_login_resp(info_msg);
			}
			else if(info_msg.msg_type == USER_DISCONNECT) {
				check_logout_attempt(info_msg);
			}
			else if(info_msg.msg_type == USER_SUBSCRIBE) {
				check_subscribe_attempt(info_msg);
			}
			else if(info_msg.msg_type == USER_UNSUBSCRIBE) {
				check_unsubscribe_attempt(info_msg);
			}
			else if(info_msg.msg_type == TRAFFIC_INCIDENT) {
				check_traffic_report_attempt(info_msg);
			}
			else if(info_msg.msg_type == SPEED_RESTRICTION) {
				check_speed_restriction_query_attempt(info_msg);
			}
			else if(info_msg.msg_type == USER_REGISTER) {
				check_register_attempt(info_msg);
			}
			else if(info_msg.msg_type == TRAFFIC_EVENT) {
				check_traffic_event(info_msg);
			}
		}
	}
}

void send_user_speed_handler() {
	struct Info_Message user_info;
	
	user_info.msg_type = CAR_SPEED_VALUE;
	char tmp_speed[INFO_CHUNK / 16];
	
	while(1) {
		if(close_client){
			break;
    		}
    		if(logged_in == 1) {
    			memset(user_info.user_name, 0, strlen(user_info.user_name));
    			strcpy(user_info.user_name, user_name);
    			memset(tmp_speed, 0, strlen(tmp_speed));
    			sprintf(user_info.speed_value, "%d", user_speed);
    			send(sockfd, (struct Info_Message*) &user_info, sizeof(user_info), 0);
    			sleep(60);
    		}
	}
}


void curr_user_speed_handler() {

	while(1) {
		if(close_client){
			break;
    		}
    		user_speed = rand() % 40 + 35;
    		sleep(1);
	}
}

void get_location() {
	generate_rand_location(curr_location);
}

void curr_user_location_handler() {
	struct Info_Message user_location;
	user_location.msg_type = SPEED_RESTRICTION;
	
	while(1) {
		if(close_client){
			break;
    		}
    		if(logged_in == 1) {
	    		get_location();
	    		memset(user_location.event_location, 0, strlen(user_location.event_location));
	    		strcpy(user_location.event_location, curr_location);
	    		memset(user_location.user_name, 0, strlen(user_location.user_name));
	    		strcpy(user_location.user_name, user_name);
	    		send(sockfd, (struct Info_Message*) &user_location, sizeof(user_location), 0);
	    		sleep(20);
    		}
    		
	}
}

void gui_app_handler(char **argv) {
	GtkApplication *app;
  	int status;
  	pid_t pid = getpid();
  	char app_id[INFO_CHUNK/4];
  	memset(app_id, 0, strlen(app_id));
  	sprintf(app_id, "app.traffic_monitoring%d", pid);
  	
	app = gtk_application_new (app_id, G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), 2, argv);
	g_object_unref(app);
	g_object_unref(main_screen.builder);
	
	struct User_Input user_in;
	memset(user_in.type, 0, strlen(user_in.type));
	strcpy(user_in.type, "logout");
	convert_user_input_to_packet(&user_in, sockfd);
	close_client = 1;
}

int main(int argc, char **argv){
	srand(time(NULL));

	struct sockaddr_in server_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(LOCALHOST);
	server_addr.sin_port = htons(SERVER_PORT);
	
  	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		fail("Error on connect function", 1, sockfd);
	}
	
	pthread_t send_msg_thread, receive_msg_thread, send_user_speed_thread, curr_user_speed_thread,
	curr_user_location_thread, gui_app_thread;
	
	XInitThreads();
	
  	if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		fail("Error on send_msg pthread_create function", 1, sockfd);
	}

  	if(pthread_create(&receive_msg_thread, NULL, (void *) receive_msg_handler, NULL) != 0){
		fail("Error on receive_msg pthread_create function", 1, sockfd);
	}
	
	if(pthread_create(&send_user_speed_thread, NULL, (void *) send_user_speed_handler, NULL) != 0){
		fail("Error on send_user_speed pthread_create function", 1, sockfd);
	}
	
	if(pthread_create(&curr_user_speed_thread, NULL, (void *) curr_user_speed_handler, NULL) != 0){
		fail("Error on curr_user_speed pthread_create function", 1, sockfd);
	}
	
	if(pthread_create(&curr_user_location_thread, NULL, (void *) curr_user_location_handler, NULL) != 0){
		fail("Error on curr_user_location pthread_create function", 1, sockfd);
	}
	
	if(argc > 1 && strcmp(argv[1], "no_gui") == 0) {
		show_gui = 0;
	}
	
	if(show_gui) {
		if(pthread_create(&gui_app_thread, NULL, (void *) gui_app_handler, (void*)argv) != 0){
			fail("Error on gui_app pthread_create function", 1, sockfd);
		}
	}
	
	while (1){
		if(close_client && logged_in == 0){
			printf("Shutting down client\n");
			break;
    		}
	}

	close(sockfd);
	
	return 0;
}
