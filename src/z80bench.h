#ifndef Z80BENCH_H
#define Z80BENCH_H

#include "z80bench_core.h"
#include <gtk/gtk.h>

typedef enum {
    UI_SEGMENT_CMD_ADD = 0,
    UI_SEGMENT_CMD_UPDATE = 1,
    UI_SEGMENT_CMD_REMOVE = 2
} UISegmentCmdType;

typedef struct {
    UISegmentCmdType type;
    int              index;
    MapEntry         entry;
} UISegmentCmd;

typedef struct {
    gboolean ok;
    int      resulting_index;
    char     error[DISASM_COMMENT_MAX];
} UISegmentCmdResult;

typedef void (*UISegmentCommandFn)(gpointer data,
                                   const UISegmentCmd *cmd,
                                   UISegmentCmdResult *res);

typedef struct {
    gboolean needs_reload;
    gboolean have_jump;
    int      jump_addr;
} UISegmentSaveRequest;

typedef void (*UISegmentSaveFn)(gpointer data, const UISegmentSaveRequest *req);

typedef enum {
    UI_SEARCH_ADDR = 0,
    UI_SEARCH_MNEM = 1,
    UI_SEARCH_COMMENT = 2
} UISearchMode;

typedef void (*UIListingVisibleAddrFn)(gpointer data, int addr);
typedef void (*UIListingFocusSearchFn)(gpointer data);

/* --------------------------------------------------------------------------
 * Module: UI Listing (ui_listing.c)
 * -------------------------------------------------------------------------- */
GtkWidget *ui_listing_new(Project *p, GtkWidget *panels);
void       ui_listing_refresh_line(GtkWidget *listing_outer, int line_index);
void       ui_listing_set_panels(GtkWidget *listing_outer, GtkWidget *panels);
gboolean   ui_listing_select_address(GtkWidget *listing_outer, int addr);
gboolean   ui_listing_search_next(GtkWidget *listing_outer, const char *text, UISearchMode mode);
void       ui_listing_set_visible_addr_cb(GtkWidget *listing_outer,
                                          UIListingVisibleAddrFn cb,
                                          gpointer data);
void       ui_listing_set_search_focus_cb(GtkWidget *listing_outer,
                                          UIListingFocusSearchFn cb,
                                          gpointer data);

/* --------------------------------------------------------------------------
 * Module: UI Panels (ui_panels.c)
 * -------------------------------------------------------------------------- */
GtkWidget *ui_panels_new(Project *p, GtkWidget *window, const char *project_path,
                         GtkWidget *listing_outer);
void ui_panels_update_selection(GtkWidget *panels, DisasmLine *dl, int line_index);
void ui_panels_refresh_symbols(GtkWidget *panels);
void ui_panels_clear_selection(GtkWidget *panels);
void ui_panels_set_reload_cb(GtkWidget *panels, void (*cb)(gpointer), gpointer data);
void ui_panels_set_jump_cb(GtkWidget *panels, void (*cb)(gpointer, int), gpointer data);
void ui_panels_set_segment_command_cb(GtkWidget *panels, UISegmentCommandFn cb, gpointer data);
void ui_panels_set_segment_save_cb(GtkWidget *panels, UISegmentSaveFn cb, gpointer data);
void ui_panels_open_segment_editor(GtkWidget *panels, int start_offset, int end_offset);
void ui_panels_open_segment_editor_prefill(GtkWidget *panels, int start_offset,
                                           int end_offset, const char *name,
                                           MapType type);
void ui_panels_open_segment_editor_with_details(GtkWidget *panels, int start_offset,
                                                int end_offset, const char *name,
                                                const char *notes, MapType type);

#endif /* Z80BENCH_H */
