#include "runner.h"
#include "lib/c-lib/css.h"
#include "lib/c-lib/launcher.h"
#include "lib/definitions.h"
#include <gio/gdesktopappinfo.h>
#include <stdbool.h>
#include <string.h>

struct _ValaPanelRunner
{
	GtkDialog __parent__;
	GtkSettings *settings;
	GtkCssProvider *css_provider;
	char *current_theme_uri;
	GtkRevealer *bottom_revealer;
	GtkListBox *app_box;
	GtkSearchEntry *main_entry;
	GtkWidget *bootstrap_row;
	GtkLabel *bootstrap_label;
	GtkToggleButton *terminal_button;
};

G_DEFINE_TYPE(ValaPanelRunner, vala_panel_runner, GTK_TYPE_DIALOG);
#define BUTTON_QUARK g_quark_from_static_string("button-id")
#define g_app_launcher_button_get_app_info(btn)                                                    \
	G_APP_INFO(g_object_get_qdata(G_OBJECT(btn), BUTTON_QUARK))
#define g_app_launcher_button_set_app_info(btn, info)                                              \
	g_object_set_qdata_full(G_OBJECT(btn), BUTTON_QUARK, (gpointer)info, g_object_unref)
GtkWidget *create_widget_func(GAppInfo *info);
void add_application(GAppInfo *app_info, ValaPanelRunner *self);
void create_bootstrap(ValaPanelRunner *self);
static GDesktopAppInfo *match_app_by_exec(const char *exec)
{
	GDesktopAppInfo *ret       = NULL;
	g_autofree char *exec_path = g_find_program_in_path(exec);
	const char *pexec;
	int path_len, exec_len, len;

	if (!exec_path)
		return NULL;

	path_len                  = strlen(exec_path);
	exec_len                  = strlen(exec);
	g_autoptr(GList) app_list = g_app_info_get_all();
	for (GList *l = app_list; l; l = l->next)
	{
		GAppInfo *app        = G_APP_INFO(l->data);
		const char *app_exec = g_app_info_get_executable(app);
		if (!app_exec)
			continue;
		if (g_path_is_absolute(app_exec))
		{
			pexec = exec_path;
			len   = path_len;
		}
		else
		{
			pexec = exec;
			len   = exec_len;
		}

		if (strncmp(app_exec, pexec, len) == 0)
		{
			/* exact match has the highest priority */
			if (app_exec[len] == '\0')
			{
				ret = G_DESKTOP_APP_INFO(app);
				break;
			}
		}
	}

	/* if this is a symlink */
	if (!ret && g_file_test(exec_path, G_FILE_TEST_IS_SYMLINK))
	{
		char target[512]; /* FIXME: is this enough? */
		len = readlink(exec_path, target, sizeof(target) - 1);
		if (len > 0)
		{
			target[len] = '\0';
			ret         = match_app_by_exec(target);
			if (!ret)
			{
				/* FIXME: Actually, target could be relative paths.
				 *        So, actually path resolution is needed here. */
				g_autofree char *basename = g_path_get_basename(target);
				g_autofree char *locate   = g_find_program_in_path(basename);
				if (locate && strcmp(locate, target) == 0)
					ret = match_app_by_exec(basename);
			}
		}
	}
	return ret;
}
static void vala_panel_runner_response(GtkDialog *dlg, gint response)
{
	ValaPanelRunner *self = VALA_PANEL_RUNNER(dlg);
	if (G_LIKELY(response == GTK_RESPONSE_ACCEPT))
	{
		GtkWidget *active_row =
		    gtk_bin_get_child(GTK_BIN(gtk_list_box_get_selected_row(self->app_box)));
		g_autoptr(GAppInfo) app_info = NULL;
		if (active_row == self->bootstrap_row)
		{
			GError *err = NULL;
			app_info    = g_app_info_create_from_commandline(
			    gtk_entry_get_text(GTK_ENTRY(self->main_entry)),
			    NULL,
			    gtk_toggle_button_get_active(self->terminal_button)
			        ? G_APP_INFO_CREATE_NEEDS_TERMINAL
			        : G_APP_INFO_CREATE_NONE,
			    &err);
			if (err)
			{
				g_error_free(err);
				g_signal_stop_emission_by_name(dlg, "response");
				return;
			}
		}
		else
			app_info = g_app_info_dup(g_app_launcher_button_get_app_info(active_row));
		bool launch =
		    vala_panel_launch(G_DESKTOP_APP_INFO(app_info), NULL, GTK_WIDGET(dlg));
		if (!launch)
		{
			g_signal_stop_emission_by_name(dlg, "response");
			return;
		}
	}
	gtk_widget_destroy((GtkWidget *)dlg);
}

/**
 * Filter the list
 */
bool on_filter(GtkListBoxRow *row, ValaPanelRunner *self)
{
	GAppInfo *info = g_app_launcher_button_get_app_info(gtk_bin_get_child(GTK_BIN(row)));
	//        g_autofree char* disp_name = g_utf8_strdown(g_app_info_get_display_name(info),-1);
	const char *search_text = gtk_entry_get_text(GTK_ENTRY(self->main_entry));
	GAppInfo *match         = G_APP_INFO(match_app_by_exec(search_text));
	if (!strcmp(search_text, ""))
		return false;
	else if (!(match) && (gtk_bin_get_child(GTK_BIN(row)) == self->bootstrap_row))
		return true;
	else if (!(match))
		return false;
	else if (g_app_info_equal(info, match))
		return true;
	return false;
}

void on_entry_changed(GtkSearchEntry *ent, ValaPanelRunner *self)
{
	gtk_list_box_invalidate_filter(self->app_box);
	GtkWidget *active_row = NULL;
	GList *chl            = gtk_container_get_children(GTK_CONTAINER(self->app_box));
	for (GList *l = chl; l != NULL; l = g_list_next(l))
	{
		GtkWidget *row = GTK_WIDGET(l->data);
		if (gtk_widget_get_visible(row) && gtk_widget_get_child_visible(row))
		{
			active_row = row;
			break;
		}
	}

	if (active_row == NULL)
	{
		gtk_revealer_set_transition_type(self->bottom_revealer,
		                                 GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
		gtk_revealer_set_reveal_child(self->bottom_revealer, false);
	}
	else
	{
		gtk_revealer_set_transition_type(self->bottom_revealer,
		                                 GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
		gtk_revealer_set_reveal_child(self->bottom_revealer, true);
		gtk_list_box_select_row(self->app_box, GTK_LIST_BOX_ROW(active_row));
	}
}

void build_app_box(ValaPanelRunner *self)
{
	create_bootstrap(self);
	gtk_container_add(GTK_CONTAINER(self->app_box), self->bootstrap_row);
	g_autoptr(GList) apps = g_app_info_get_all();
	g_list_foreach(apps, G_CALLBACK(add_application), self);
	gtk_widget_show_all(GTK_WIDGET(self->app_box));
}

void add_application(GAppInfo *app_info, ValaPanelRunner *self)
{
	GDesktopAppInfo *dinfo = G_DESKTOP_APP_INFO(app_info);
	if (g_desktop_app_info_get_nodisplay(dinfo))
		return;
	GtkWidget *button = create_widget_func(g_app_info_dup(app_info));
	gtk_container_add(GTK_CONTAINER(self->app_box), button);
	gtk_widget_show_all(button);
}

/**
 * Handle click/<enter> activation on the main list
 */
void on_row_activated(GtkListBoxRow *row, ValaPanelRunner *self)
{
	gtk_dialog_response(GTK_DIALOG(self), GTK_RESPONSE_ACCEPT);
}

/**
 * Handle click/<enter> activation on the entry
 */
void on_entry_activated(GtkEntry *row, ValaPanelRunner *self)
{
	gtk_dialog_response(GTK_DIALOG(self), GTK_RESPONSE_ACCEPT);
}

static void vala_panel_runner_finalize(GObject *obj)
{
	ValaPanelRunner *self =
	    G_TYPE_CHECK_INSTANCE_CAST(obj, vala_panel_runner_get_type(), ValaPanelRunner);
	gtk_window_set_application((GtkWindow *)self, NULL);
	g_object_unref0(self->bootstrap_row);
	g_object_unref0(self->bootstrap_label);
	g_object_unref0(self->main_entry);
	g_object_unref0(self->bottom_revealer);
	g_object_unref0(self->app_box);
	g_object_unref0(self->terminal_button);
	G_OBJECT_CLASS(vala_panel_runner_parent_class)->finalize(obj);
}

void vala_panel_runner_init(ValaPanelRunner *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));
	css_apply_from_resource(GTK_WIDGET(self),
	                        "/org/vala-panel/runner/style.css",
	                        "-panel-run-dialog");
	build_app_box(self);
	gtk_list_box_set_filter_func(self->app_box, (GtkListBoxFilterFunc)on_filter, self, NULL);
}

void vala_panel_runner_class_init(ValaPanelRunnerClass *klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
	                                            "/org/vala-panel/runner/app-runner.ui");
	vala_panel_runner_parent_class  = g_type_class_peek_parent(klass);
	G_OBJECT_CLASS(klass)->finalize = vala_panel_runner_finalize;
	gtk_widget_class_bind_template_child_full(GTK_WIDGET_CLASS(klass),
	                                          "main-entry",
	                                          false,
	                                          G_STRUCT_OFFSET(ValaPanelRunner, main_entry));
	gtk_widget_class_bind_template_child_full(GTK_WIDGET_CLASS(klass),
	                                          "search-box",
	                                          false,
	                                          G_STRUCT_OFFSET(ValaPanelRunner, app_box));
	gtk_widget_class_bind_template_child_full(GTK_WIDGET_CLASS(klass),
	                                          "terminal-button",
	                                          false,
	                                          G_STRUCT_OFFSET(ValaPanelRunner,
	                                                          terminal_button));
	gtk_widget_class_bind_template_child_full(GTK_WIDGET_CLASS(klass),
	                                          "revealer",
	                                          true,
	                                          G_STRUCT_OFFSET(ValaPanelRunner,
	                                                          bottom_revealer));
	gtk_widget_class_bind_template_callback_full(GTK_WIDGET_CLASS(klass),
	                                             "on_search_changed",
	                                             G_CALLBACK(on_entry_changed));
	gtk_widget_class_bind_template_callback_full(GTK_WIDGET_CLASS(klass),
	                                             "on_search_activated",
	                                             G_CALLBACK(on_entry_activated));
	gtk_widget_class_bind_template_callback_full(GTK_WIDGET_CLASS(klass),
	                                             "vala_panel_runner_response",
	                                             G_CALLBACK(vala_panel_runner_response));
	gtk_widget_class_bind_template_callback_full(GTK_WIDGET_CLASS(klass),
	                                             "on_row_activated",
	                                             G_CALLBACK(on_row_activated));
}

ValaPanelRunner *vala_panel_runner_new(GtkApplication *app)
{
	return VALA_PANEL_RUNNER(
	    g_object_new(vala_panel_runner_get_type(), "application", app, NULL));
}
void create_bootstrap(ValaPanelRunner *self)
{
	GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2));
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(box)),
	                            "launcher-button");
	GtkImage *image =
	    GTK_IMAGE(gtk_image_new_from_icon_name("system-run-symbolic", GTK_ICON_SIZE_DIALOG));
	gtk_image_set_pixel_size(image, 48);
	gtk_widget_set_margin_start(GTK_WIDGET(image), 8);
	gtk_box_pack_start(box, GTK_WIDGET(image), false, false, 0);
	self->bootstrap_row   = GTK_WIDGET(box);
	self->bootstrap_label = GTK_LABEL(gtk_label_new(""));
	gtk_box_pack_start(box, GTK_WIDGET(self->bootstrap_label), false, false, 0);
	g_object_bind_property(self->main_entry,
	                       "text",
	                       self->bootstrap_label,
	                       "label",
	                       G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

GtkWidget *create_widget_func(GAppInfo *info)
{
	GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2));
	g_app_launcher_button_set_app_info(box, info);
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(box)),
	                            "launcher-button");
	GtkImage *image =
	    GTK_IMAGE(gtk_image_new_from_gicon(g_app_info_get_icon(info), GTK_ICON_SIZE_DIALOG));
	gtk_image_set_pixel_size(image, 48);
	gtk_widget_set_margin_start(GTK_WIDGET(image), 8);
	gtk_box_pack_start(box, GTK_WIDGET(image), false, false, 0);

	g_autofree char *nom =
	    g_markup_escape_text(g_app_info_get_name(info), strlen(g_app_info_get_name(info)));
	const char *sdesc =
	    g_app_info_get_description(info) ? g_app_info_get_description(info) : "";
	g_autofree char *desc   = g_markup_escape_text(sdesc, strlen(sdesc));
	g_autofree char *markup = g_strdup_printf("<big>%s</big>\n<small>%s</small>", nom, desc);
	GtkLabel *label         = GTK_LABEL(gtk_label_new(markup));
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(label)), "dim-label");
	gtk_label_set_line_wrap(label, true);
	g_object_set(label, "xalign", 0.0, NULL);
	gtk_label_set_use_markup(label, true);
	gtk_widget_set_margin_start(GTK_WIDGET(label), 12);
	gtk_label_set_max_width_chars(label, 60);
	gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
	gtk_box_pack_start(box, GTK_WIDGET(label), false, false, 0);

	gtk_widget_set_hexpand(GTK_WIDGET(box), false);
	gtk_widget_set_vexpand(GTK_WIDGET(box), false);
	gtk_widget_set_halign(GTK_WIDGET(box), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(box), GTK_ALIGN_START);
	gtk_widget_set_tooltip_text(GTK_WIDGET(box), g_app_info_get_name(info));
	gtk_widget_set_margin_top(GTK_WIDGET(box), 3);
	gtk_widget_set_margin_bottom(GTK_WIDGET(box), 3);
	return GTK_WIDGET(box);
}

void gtk_run(ValaPanelRunner *self)
{
	gtk_widget_show_all(GTK_WIDGET(self));
	gtk_widget_grab_focus(GTK_WIDGET(self->main_entry));
	gtk_window_present_with_time(GTK_WINDOW(self), gtk_get_current_event_time());
}