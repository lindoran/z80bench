#ifndef Z80BENCH_H
#define Z80BENCH_H

#include "z80bench_core.h"
#include <gtk/gtk.h>

/* --------------------------------------------------------------------------
 * Module: UI Listing (ui_listing.c)
 * -------------------------------------------------------------------------- */
GtkWidget *ui_listing_new(Project *p, GtkWidget *panels);
void       ui_listing_refresh_line(GtkWidget *listing_outer, int line_index);
void       ui_listing_set_panels(GtkWidget *listing_outer, GtkWidget *panels);
gboolean   ui_listing_select_address(GtkWidget *listing_outer, int addr);

/* --------------------------------------------------------------------------
 * Module: UI Panels (ui_panels.c)
 * -------------------------------------------------------------------------- */
GtkWidget *ui_panels_new(Project *p, GtkWidget *window, const char *project_path,
                         GtkWidget *listing_outer);
void ui_panels_update_selection(GtkWidget *panels, DisasmLine *dl, int line_index);
void ui_panels_refresh_symbols(GtkWidget *panels);
void ui_panels_set_reload_cb(GtkWidget *panels, void (*cb)(gpointer), gpointer data);
void ui_panels_set_jump_cb(GtkWidget *panels, void (*cb)(gpointer, int), gpointer data);
void ui_panels_open_segment_editor(GtkWidget *panels, int start_offset, int end_offset);
void ui_panels_open_segment_editor_prefill(GtkWidget *panels, int start_offset,
                                           int end_offset, const char *name,
                                           MapType type);
void ui_panels_open_segment_editor_with_details(GtkWidget *panels, int start_offset,
                                                int end_offset, const char *name,
                                                const char *notes, MapType type);

#endif /* Z80BENCH_H */
