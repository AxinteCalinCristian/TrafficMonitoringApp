#include<json-c/json.h>

// defines and consts
#define LOCALHOST 		"127.0.0.1"
#define SERVER_PORT 		8080
#define MAX_NO_OF_CLIENTS 	10
#define INFO_CHUNK 		1024
#define SQLITE_DB 		"sqlite_db.db"
#define RAND_INPUT_JSON_PATH 	"generator_input.json"

// enums
enum MSG_TYPE {
	USER_DISCONNECT,
	USER_LOGIN,
	USER_REGISTER,
	USER_SUBSCRIBE,
	USER_UNSUBSCRIBE,
	CAR_SPEED_VALUE,
	TRAFFIC_INCIDENT,
	SPEED_RESTRICTION,
	TRAFFIC_EVENT
};

char * MSG_TYPES[9] = {"USER_DISCONNECT", "USER_LOGIN", "USER_REGISTER", "USER_SUBSCRIBE", "USER_UNSUBSCRIBE",
		     "CAR_SPEED_VALUE", "TRAFFIC_INCIDENT", "SPEED_RESTRICTION", "TRAFFIC_EVENT"};

enum ACTION_RESPONSE {
	ACTION_ACCEPTED,
	ACTION_REJECTED,
	ACTION_FAILED,
	NO_ACTION
};

char * ACTION_RESPONSES[4] = {"ACTION_ACCEPTED", "ACTION_REJECTED", "ACTION_FAILED", "NO_ACTION"};

// structs 
struct Info_Message {
	enum MSG_TYPE 		msg_type;
	char 			speed_value	[INFO_CHUNK / 16];
	char 			event_location	[INFO_CHUNK / 4];
	char 			event_details	[INFO_CHUNK];
	char 			user_name	[INFO_CHUNK / 8];
	int 			wants_updates;
	enum ACTION_RESPONSE 	action_response;
};

struct User_Input {
	char 		type 		[INFO_CHUNK / 4];
	char 		user_name 	[INFO_CHUNK / 8];
	char 		location 	[INFO_CHUNK];
	char 		summary 	[INFO_CHUNK];
};

struct Client {
	int 		fd;
	char 		user_name	[INFO_CHUNK / 8];
	int 		send_updates;
};

// parsers

struct User_Input userInputParser(char buffer[]) {
	int idx = 0, len = strlen(buffer);
	while(idx < len && buffer[idx] == ' ') idx++;
	
	char word[INFO_CHUNK];
	memset(word, 0, INFO_CHUNK);
	int word_idx = 0;
	
	while(idx < len && buffer[idx] != ' ') word[word_idx] = buffer[idx], word_idx++, idx++;
	word[word_idx] = 0;
	
	struct User_Input user_in;
	bzero(&user_in, sizeof(user_in));
	
	if(strcmp(word, "login") == 0 || strcmp(word, "register") == 0) {
		strcpy(user_in.type, word);
		while(idx < len && buffer[idx] == ' ') idx++;
		
		memset(word, 0, INFO_CHUNK);
		word_idx = 0;
		
		while(idx < len && buffer[idx] != ' ') word[word_idx] = buffer[idx], word_idx++, idx++;
		word[word_idx] = 0;
		
		strcpy(user_in.user_name, word);
	} else if (strcmp(word, "subscribe") == 0){
		strcpy(user_in.type, word);
	} else if (strcmp(word, "unsubscribe") == 0){
		strcpy(user_in.type, word);
	} else if (strcmp(word, "report_incident") == 0) {
		strcpy(user_in.type, word);
		while(idx < len && buffer[idx] == ' ') idx++;
		idx++;
		
		memset(word, 0, INFO_CHUNK);
		word_idx = 0;
		
		while(idx < len && buffer[idx] != '\"') word[word_idx] = buffer[idx], word_idx++, idx++;
		word[word_idx] = 0;
		idx++;
		
		strcpy(user_in.location, word);
		
		while(idx < len && buffer[idx] == ' ') idx++;
		idx++;
		
		memset(word, 0, INFO_CHUNK);
		word_idx = 0;
		
		while(idx < len && buffer[idx] != '\"') word[word_idx] = buffer[idx], word_idx++, idx++;
		word[word_idx] = 0;
		idx++;
		
		strcpy(user_in.summary, word);
	} else if (strcmp(word, "logout") == 0) {
		strcpy(user_in.type, word);
	} else if (strcmp(word, "quit") == 0) {
		strcpy(user_in.type, word);
	}
	
	return user_in;
}

void format_and_print_message(int type, struct Info_Message * info_msg) {
	char output[INFO_CHUNK * 4];
	memset(output, 0, strlen(output));
	if(type == 1) {
		strcpy(output, "[RECEIVED] ");
	}
	else if(type == -1) {
		strcpy(output, "[SENT]     ");
	}
	else{
		return;
	}
	
	strcat(output, "Type: ");
	strcat(output, MSG_TYPES[info_msg->msg_type]);
	
	if(strlen(info_msg->speed_value) > 0) {
		strcat(output, ", Speed value: ");
		strcat(output, info_msg->speed_value);
	}
	
	if(strlen(info_msg->event_location) > 0) {
		strcat(output, ", Event location: ");
		strcat(output, info_msg->event_location);
	}
	
	if(strlen(info_msg->event_details) > 0) {
		strcat(output, ", Event details: ");
		strcat(output, info_msg->event_details);
	}
	
	if(strlen(info_msg->user_name) > 0) {
		strcat(output, ", Username: ");
		strcat(output, info_msg->user_name);
	}
	
	strcat(output, ", Action response: ");
	strcat(output, ACTION_RESPONSES[info_msg->action_response]);
	
	printf("%s\n", output);
}


// random data generation
struct json_object * parsed_json;
struct json_object * road_names;
struct json_object * city_names;
struct json_object * event_types;
struct json_object * sports_events;
struct json_object * weather_events;
struct json_object * football_teams; 
struct json_object * sports_events;

void parse_rand_input_json() {
	if(parsed_json != NULL) {
		return;
	}
	srand(time(NULL));
	FILE* fp;
	char buffer[INFO_CHUNK * 8];
	fp = fopen(RAND_INPUT_JSON_PATH, "r");
	fread(buffer, INFO_CHUNK * 8, 1, fp);
	fclose(fp);
	
	parsed_json = json_tokener_parse(buffer);
	json_object_object_get_ex(parsed_json, "road_names", &road_names);
	json_object_object_get_ex(parsed_json, "city_names", &city_names);
	json_object_object_get_ex(parsed_json, "event_types", &event_types);
	json_object_object_get_ex(parsed_json, "sports_events", &sports_events);
	json_object_object_get_ex(parsed_json, "weather_events", &weather_events);
	json_object_object_get_ex(parsed_json, "sports_events", &sports_events);
	
	json_object_object_get_ex(sports_events, "football", &football_teams);
}

void generate_rand_location(char location[]) {
	parse_rand_input_json();
	
	if(road_names == NULL || city_names == NULL) {
		printf("Error parsing rand city/road names\n");
		return;
	}
	
	memset(location, 0, strlen(location));
	
	int num = rand() % 80 + 1;
	
	size_t no_of_roads = json_object_array_length(road_names);
	size_t no_of_cities = json_object_array_length(city_names);
	
	int road_idx = rand() % no_of_roads, city_idx = rand() % no_of_cities;
	
	sprintf(location, "Strada %s %d, %s", json_object_get_string(json_object_array_get_idx(road_names, road_idx)),
	num, json_object_get_string(json_object_array_get_idx(city_names, city_idx)));
}

void generate_rand_event(char event_info[]) {
	parse_rand_input_json();
	
	if(event_types == NULL || sports_events == NULL || weather_events == NULL) {
		printf("Error parsing rand events info\n");
		return;
	}
	memset(event_info, 0, strlen(event_info));
	
	size_t no_of_events = json_object_array_length(event_types);
	int event_type_idx = rand() % no_of_events;
	char type[INFO_CHUNK / 8];
	memset(type, 0, strlen(type));
	strcpy(type, json_object_get_string(json_object_array_get_idx(event_types, event_type_idx)));
	
	if(strcmp(type, "Pret benzina") == 0) {
		int a = rand() % 3 + 4, b = rand() % 10, c = rand() % 10;
		char location[INFO_CHUNK / 4];
		generate_rand_location(location);
		sprintf(event_info, "Update pret combustibil pentru benzinaria aflata pe %s: %d.%d%d RON/L", location, a, b, c);
		return;
	}
	else if(strcmp(type, "Accident") == 0) {
		char location[INFO_CHUNK / 4];
		generate_rand_location(location);
		sprintf(event_info, "Accident rutier pe %s", location);
		return;
	}
	else if(strcmp(type, "Drum inchis") == 0) {
		char location[INFO_CHUNK / 4];
		generate_rand_location(location);
		sprintf(event_info, "Drum temporar inchis: %s", location);
		return;
	}
	else if(strcmp(type, "Restrictie temporara de viteza") == 0) {
		char location[INFO_CHUNK / 4];
		generate_rand_location(location);
		int max_speed = rand() % 30 + 25;
		sprintf(event_info, "Restrictie temporara de viteza (%d) pentru %s", max_speed, location);
		return;
	}
	else if(strcmp(type, "Update Meteo") == 0) {
		char location[INFO_CHUNK / 4];
		size_t no_of_cities = json_object_array_length(city_names);
		int city_idx = rand() % no_of_cities;
		size_t no_of_we = json_object_array_length(weather_events);
		int weather_event_idx = rand() % no_of_we;
		
		sprintf(event_info, "Updated meteo: %s in zona %s",
		json_object_get_string(json_object_array_get_idx(weather_events, weather_event_idx)),
		json_object_get_string(json_object_array_get_idx(city_names, city_idx)));
		return;
	}
	else if(strcmp(type, "Eveniment Sportiv") == 0) {
		size_t no_of_teams = json_object_array_length(football_teams);
		int team_a = rand() % no_of_teams, team_b = rand() % no_of_teams, score_a = rand() % 5, score_b = rand() % 5;
		
		while(team_a == team_b) {
			team_b = rand() % no_of_teams;
		}
		
		sprintf(event_info, "Eveniment sportiv: Fotbal: %s %d : %d %s",
		json_object_get_string(json_object_array_get_idx(football_teams, team_a)), score_a,
		score_b, json_object_get_string(json_object_array_get_idx(football_teams, team_b)));
		return;
	}
}
