#ifndef __VALA_PANEL_CSS_H__
#define __VALA_PANEL_CSS_H__

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <stdbool.h>

void css_apply_with_class(GtkWidget *widget, const char *css, const char *klass, bool remove);
char *css_generate_background(const char *filename, GdkRGBA color, bool no_image);
char *css_generate_font_color(GdkRGBA color);
char *css_generate_font_size(gint size);
char *css_generate_font_label(gfloat size, bool is_bold);
char *css_apply_from_file(GtkWidget *widget, const char *file);
char *css_apply_from_resource(GtkWidget *widget, const char *file, const char *klass);
char *css_apply_from_file_to_app(const char *file);
// inline char* css_generate_flat_button(GtkWidget* widget,ValaPanel* panel);

#endif