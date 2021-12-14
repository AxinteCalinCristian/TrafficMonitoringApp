#include <gtk/gtk.h>

#define LOGIN_UI "ClientUI/login_screen.ui"
#define MAIN_UI "ClientUI/main_screen.ui"

struct Login_Screen {
	GtkBuilder * 	builder;
	GObject * 	window;
	GObject * 	user_entry;
	GObject * 	register_button;
	GObject * 	login_button;
	GtkLabel * 	warning_label;
};

struct Main_Screen {
	GtkBuilder * 	builder;
	GObject * 	window;
	GObject * 	logout_btn;
	GtkLabel * 	current_user;
	GtkLabel * 	current_location;
	GObject * 	news_subscription;
	GObject * 	event_location;
	GObject * 	event_details;
	GObject * 	submit_event;
	GtkWidget * 	event_list;
};
