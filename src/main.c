#include <gtk/gtk.h>
#include <string.h>
#include "z80bench.h"

#define GUI_LOG(app, fmt, ...) \
    G_STMT_START { \
        if ((app) && g_getenv("Z80BENCH_GUI_DEBUG")) \
            g_printerr("[z80bench-gui] " fmt "\n", ##__VA_ARGS__); \
    } G_STMT_END

typedef struct {
    Project   project;
    GtkWidget *window;
    GtkWidget *main_vpaned;
    GtkWidget *paned;
    GtkWidget *dock_paned;
    GtkWidget *listing_outer;
    GtkWidget *panels_outer;
    GtkWidget *hex_view;
    GtkWidget *search_entry;
    GtkWidget *search_mode_addr;
    GtkWidget *search_mode_mnem;
    GtkWidget *search_mode_comment;
    char       project_path[512];
    int        pending_listing_addr;
    int        pending_listing_retries;
    gboolean   ui_busy;
    gboolean   pending_segment_reload;
    gboolean   segment_save_idle_queued;
} App;

typedef struct {
    GtkWidget *paned;
    int        pos;
} PanedPosApply;

typedef struct {
    GtkWidget *window;
} WindowSizeUnlock;

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */

static void show_new_project_dialog(App *app, const char *bin_path);
static void on_open_project_clicked(GtkButton *button, gpointer user_data);
static void on_new_project_clicked(GtkButton *button, gpointer user_data);

/* --------------------------------------------------------------------------
 * ImportCtx — lives for the duration of the new-project dialog
 * -------------------------------------------------------------------------- */

typedef struct {
    App       *app;
    char       bin_path[512];
    char       proj_parent[512]; /* directory the project folder will be created in */
    GtkWidget *dialog;
    GtkWidget *folder_label;
    GtkWidget *origin_entry;
} ImportCtx;

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

static gboolean controller_apply_pending_jump(gpointer user_data);  /* forward */
static void controller_request_jump(gpointer user_data, int addr);  /* forward */
static void controller_reload_and_render(gpointer user_data);  /* forward */
static void controller_segment_command(gpointer data, const UISegmentCmd *cmd,
                                       UISegmentCmdResult *res);  /* forward */
static void controller_segment_save(gpointer data, const UISegmentSaveRequest *req);  /* forward */
static gboolean controller_segment_save_idle(gpointer user_data);  /* forward */
static gboolean controller_apply_paned_position_idle(gpointer user_data);  /* forward */
static gboolean controller_unlock_window_size_idle(gpointer user_data);  /* forward */
static void controller_listing_visible_changed(gpointer user_data, int addr);  /* forward */
static void controller_focus_search(gpointer user_data);  /* forward */

static void app_render_hex_dump(App *app);  /* forward */
static void app_sync_hex_to_addr(App *app, int addr);  /* forward */
static GtkWidget *build_bottom_dock(App *app);  /* forward */
static UISearchMode app_get_search_mode(App *app);  /* forward */
static void on_dock_search_activate(GtkEntry *entry, gpointer user_data);  /* forward */
static void on_dock_search_next(GtkButton *button, gpointer user_data);  /* forward */
static void on_main_paned_position_changed(GObject *obj, GParamSpec *pspec,
                                           gpointer user_data);  /* forward */

static int controller_pick_paned_position(App *app, int current_pos) {
    int win_w = gtk_widget_get_width(app->window);
    int min_pos = 300;
    int max_pos = (win_w > 700) ? (win_w - 260) : 9000;
    if (max_pos < min_pos) max_pos = min_pos;

    int fallback = (win_w > 0) ? (int)(win_w * 0.62) : 630;
    if (fallback < min_pos) fallback = min_pos;
    if (fallback > max_pos) fallback = max_pos;

    if (current_pos <= 0)
        return fallback;
    if (current_pos < min_pos)
        return min_pos;
    if (current_pos > max_pos)
        return max_pos;
    return current_pos;
}

static gboolean controller_apply_paned_position_idle(gpointer user_data) {
    PanedPosApply *pp = user_data;
    if (!pp || !pp->paned) {
        g_free(pp);
        return G_SOURCE_REMOVE;
    }
    gtk_paned_set_position(GTK_PANED(pp->paned), pp->pos);
    g_free(pp);
    return G_SOURCE_REMOVE;
}

static gboolean controller_unlock_window_size_idle(gpointer user_data) {
    WindowSizeUnlock *u = user_data;
    if (!u) return G_SOURCE_REMOVE;
    if (u->window)
        gtk_widget_set_size_request(u->window, -1, -1);
    g_free(u);
    return G_SOURCE_REMOVE;
}

static void app_render_hex_dump(App *app) {
    if (!app || !app->hex_view) return;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->hex_view));
    if (!buf) return;

    if (!app->project.rom || app->project.rom_size <= 0) {
        gtk_text_buffer_set_text(buf, "No ROM loaded.", -1);
        return;
    }

    GString *s = g_string_sized_new((gsize)app->project.rom_size * 6);
    for (int off = 0; off < app->project.rom_size; off += 16) {
        int addr = app->project.load_addr + off;
        g_string_append_printf(s, "%08X: ", addr & 0xFFFFFFFF);
        for (int i = 0; i < 16; i++) {
            if (off + i < app->project.rom_size)
                g_string_append_printf(s, "%02X ", app->project.rom[off + i]);
            else
                g_string_append(s, "   ");
        }
        g_string_append(s, " ");
        for (int i = 0; i < 16 && off + i < app->project.rom_size; i++) {
            unsigned char c = app->project.rom[off + i];
            g_string_append_c(s, (c >= 0x20 && c < 0x7F) ? (char)c : '.');
        }
        g_string_append_c(s, '\n');
    }
    gtk_text_buffer_set_text(buf, s->str, -1);
    g_string_free(s, TRUE);
}

static void app_sync_hex_to_addr(App *app, int addr) {
    if (!app || !app->hex_view || !app->project.rom || app->project.rom_size <= 0) return;
    int off = addr - app->project.load_addr;
    if (off < 0) off = 0;
    if (off >= app->project.rom_size) off = app->project.rom_size - 1;
    int line = off / 16;

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->hex_view));
    GtkTextIter it;
    gtk_text_buffer_get_iter_at_line(buf, &it, line);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->hex_view), &it, 0.2, TRUE, 0.0, 0.1);
}

static UISearchMode app_get_search_mode(App *app) {
    if (!app) return UI_SEARCH_ADDR;
    if (app->search_mode_mnem &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->search_mode_mnem)))
        return UI_SEARCH_MNEM;
    if (app->search_mode_comment &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->search_mode_comment)))
        return UI_SEARCH_COMMENT;
    return UI_SEARCH_ADDR;
}

static void on_dock_search_activate(GtkEntry *entry, gpointer user_data) {
    App *app = user_data;
    if (!app || !app->listing_outer) return;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!text || !text[0]) return;
    ui_listing_search_next(app->listing_outer, text, app_get_search_mode(app));
}

static void on_dock_search_next(GtkButton *button, gpointer user_data) {
    (void)button;
    App *app = user_data;
    if (!app || !app->search_entry || !app->listing_outer) return;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(app->search_entry));
    if (!text || !text[0]) return;
    ui_listing_search_next(app->listing_outer, text, app_get_search_mode(app));
}

static void controller_listing_visible_changed(gpointer user_data, int addr) {
    App *app = user_data;
    app_sync_hex_to_addr(app, addr);
}

static void controller_focus_search(gpointer user_data) {
    App *app = user_data;
    if (!app || !app->search_entry) return;
    gtk_widget_grab_focus(app->search_entry);
}

static GtkWidget *build_bottom_dock(App *app) {
    GtkWidget *dock_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    int main_pos = app && app->paned ? gtk_paned_get_position(GTK_PANED(app->paned)) : 630;
    if (main_pos <= 0) main_pos = 630;
    gtk_paned_set_position(GTK_PANED(dock_split), main_pos);
    gtk_paned_set_resize_start_child(GTK_PANED(dock_split), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(dock_split), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(dock_split), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(dock_split), FALSE);

    GtkWidget *hex_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hex_header = gtk_label_new("HEX DUMP  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII");
    gtk_widget_add_css_class(hex_header, "monospace");
    gtk_label_set_xalign(GTK_LABEL(hex_header), 0.0);
    gtk_widget_set_margin_start(hex_header, 8);
    gtk_widget_set_margin_top(hex_header, 4);
    gtk_widget_set_margin_bottom(hex_header, 2);
    gtk_box_append(GTK_BOX(hex_container), hex_header);

    GtkWidget *hex_scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(hex_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(hex_scrolled, TRUE);

    GtkWidget *hex_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(hex_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(hex_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(hex_view), GTK_WRAP_NONE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(hex_view), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(hex_view), 8);
    gtk_widget_add_css_class(hex_view, "monospace");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(hex_scrolled), hex_view);
    app->hex_view = hex_view;
    gtk_box_append(GTK_BOX(hex_container), hex_scrolled);

    GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(search_box, 8);
    gtk_widget_set_margin_end(search_box, 8);
    gtk_widget_set_margin_top(search_box, 6);
    gtk_widget_set_margin_bottom(search_box, 6);
    gtk_widget_set_size_request(search_box, 260, -1);

    GtkWidget *search_title = gtk_label_new(NULL);
    char *m = g_markup_printf_escaped(
        "<span size='small' color='#888'><b>SEARCH</b></span>");
    gtk_label_set_markup(GTK_LABEL(search_title), m);
    g_free(m);
    gtk_label_set_xalign(GTK_LABEL(search_title), 0.0f);
    gtk_box_append(GTK_BOX(search_box), search_title);

    GtkWidget *mode_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *mode_addr = gtk_toggle_button_new_with_label("Addr");
    GtkWidget *mode_mnem = gtk_toggle_button_new_with_label("Mnem");
    GtkWidget *mode_comment = gtk_toggle_button_new_with_label("Comment");
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(mode_mnem), GTK_TOGGLE_BUTTON(mode_addr));
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(mode_comment), GTK_TOGGLE_BUTTON(mode_addr));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mode_addr), TRUE);
    gtk_box_append(GTK_BOX(mode_row), mode_addr);
    gtk_box_append(GTK_BOX(mode_row), mode_mnem);
    gtk_box_append(GTK_BOX(mode_row), mode_comment);
    gtk_box_append(GTK_BOX(search_box), mode_row);

    GtkWidget *entry = gtk_entry_new();
    gtk_widget_add_css_class(entry, "monospace");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Search value");
    gtk_box_append(GTK_BOX(search_box), entry);

    GtkWidget *find_next = gtk_button_new_with_label("Find Next");
    gtk_box_append(GTK_BOX(search_box), find_next);

    GtkWidget *search_note = gtk_label_new(NULL);
    char *nm = g_markup_printf_escaped(
        "<span size='x-small' color='#666'>hex values in 0x0000 notation</span>");
    gtk_label_set_markup(GTK_LABEL(search_note), nm);
    g_free(nm);
    gtk_label_set_xalign(GTK_LABEL(search_note), 0.5f);
    gtk_box_append(GTK_BOX(search_box), search_note);

    gtk_box_append(GTK_BOX(search_box), gtk_label_new(""));
    gtk_widget_set_vexpand(gtk_widget_get_last_child(search_box), TRUE);

    app->search_entry = entry;
    app->search_mode_addr = mode_addr;
    app->search_mode_mnem = mode_mnem;
    app->search_mode_comment = mode_comment;

    g_signal_connect(entry, "activate", G_CALLBACK(on_dock_search_activate), app);
    g_signal_connect(find_next, "clicked", G_CALLBACK(on_dock_search_next), app);

    gtk_paned_set_start_child(GTK_PANED(dock_split), hex_container);
    gtk_paned_set_end_child(GTK_PANED(dock_split), search_box);
    app->dock_paned = dock_split;
    return dock_split;
}

static void on_main_paned_position_changed(GObject *obj, GParamSpec *pspec, gpointer user_data) {
    (void)obj;
    (void)pspec;
    App *app = user_data;
    if (!app || !app->paned || !app->dock_paned) return;
    int pos = gtk_paned_get_position(GTK_PANED(app->paned));
    if (pos > 0)
        gtk_paned_set_position(GTK_PANED(app->dock_paned), pos);
}

static void controller_render(App *app) {
    int paned_pos = controller_pick_paned_position(
        app, gtk_paned_get_position(GTK_PANED(app->paned)));
    int win_w = gtk_widget_get_width(app->window);
    int win_h = gtk_widget_get_height(app->window);
    GUI_LOG(app, "render begin map=%d pending=0x%04X retries=%d win=%dx%d paned=%d",
            memmap_count(app->project.map),
            app->pending_listing_addr >= 0 ? (app->pending_listing_addr & 0xFFFF) : 0xFFFF,
            app->pending_listing_retries, win_w, win_h, paned_pos);
    if (win_w > 100 && win_h > 100)
        gtk_widget_set_size_request(app->window, win_w, win_h);

    char title[512];
    snprintf(title, sizeof(title), "z80bench — %s", app->project.name);
    gtk_window_set_title(GTK_WINDOW(app->window), title);

    GtkWidget *listing = ui_listing_new(&app->project, NULL);
    GtkWidget *panels  = ui_panels_new(&app->project, app->window,
                                       app->project_path, listing);
    ui_listing_set_panels(listing, panels);
    ui_listing_set_visible_addr_cb(listing, controller_listing_visible_changed, app);
    ui_listing_set_search_focus_cb(listing, controller_focus_search, app);
    ui_panels_set_reload_cb(panels, controller_reload_and_render, app);
    ui_panels_set_jump_cb(panels, controller_request_jump, app);
    ui_panels_set_segment_command_cb(panels, controller_segment_command, app);
    ui_panels_set_segment_save_cb(panels, controller_segment_save, app);

    app->listing_outer = listing;
    app->panels_outer = panels;

    gtk_paned_set_start_child(GTK_PANED(app->paned), listing);
    gtk_paned_set_end_child(GTK_PANED(app->paned), panels);
    gtk_paned_set_resize_start_child(GTK_PANED(app->paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(app->paned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(app->paned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(app->paned), FALSE);
    gtk_paned_set_position(GTK_PANED(app->paned), paned_pos);
    if (app->dock_paned)
        gtk_paned_set_position(GTK_PANED(app->dock_paned), paned_pos);

    app_render_hex_dump(app);
    if (app->project.nlines > 0)
        app_sync_hex_to_addr(app, app->project.lines[0].addr);

    PanedPosApply *pp = g_new0(PanedPosApply, 1);
    pp->paned = app->paned;
    pp->pos = paned_pos;
    g_idle_add(controller_apply_paned_position_idle, pp);

    WindowSizeUnlock *u = g_new0(WindowSizeUnlock, 1);
    u->window = app->window;
    g_idle_add(controller_unlock_window_size_idle, u);
    GUI_LOG(app, "render end map=%d", memmap_count(app->project.map));
}

/* Controller: full reload flow for map/segment changes */
static void controller_reload_and_render(gpointer user_data) {
    App *app = user_data;
    if (!app->project.rom || !app->project_path[0] || app->ui_busy) return;
    GUI_LOG(app, "reload begin map=%d", memmap_count(app->project.map));
    app->ui_busy = TRUE;
    gtk_widget_set_sensitive(app->paned, FALSE);

    /* 1. Sync map entries → ann regions */
    project_sync_map_to_regions(&app->project);

    /* 2. Re-disassemble with the updated regions */
    if (app->project.lines) {
        int nregions;
        const Region *regions = annotate_get_regions(app->project.ann, &nregions);
        app->project.nlines = disasm_range(
            app->project.rom, app->project.rom_size,
            app->project.load_addr, 0, app->project.rom_size - 1,
            regions, nregions, NULL,
            app->project.lines, 100000);
        if (app->project.nlines > 0) {
            annotate_merge(app->project.lines, app->project.nlines, app->project.ann);
            symbols_resolve(app->project.lines, app->project.nlines, app->project.sym);
        }
    }

    /* 3. Save AFTER re-disasm so .mnm and .map contain the updated data */
    project_save(&app->project, app->project_path);

    /* 4. Rebuild the listing and panels widgets */
    controller_render(app);
    if (app->pending_listing_addr >= 0) {
        /* Try immediately first; if the listing has not realized yet, retry. */
        controller_apply_pending_jump(app);
        if (app->pending_listing_retries < 120)
            app->pending_listing_retries = 120;
        g_timeout_add(16, controller_apply_pending_jump, app);
    }
    gtk_widget_set_sensitive(app->paned, TRUE);
    app->ui_busy = FALSE;
    GUI_LOG(app, "reload end map=%d", memmap_count(app->project.map));
}

static void controller_request_jump(gpointer user_data, int addr) {
    App *app = user_data;
    app->pending_listing_addr = addr;
    app->pending_listing_retries = 120;
    app_sync_hex_to_addr(app, addr);
    GUI_LOG(app, "jump request 0x%04X", addr & 0xFFFF);
}

static gboolean controller_apply_pending_jump(gpointer user_data) {
    App *app = user_data;
    if (app->pending_listing_addr >= 0 && app->listing_outer) {
        gboolean done = ui_listing_select_address(app->listing_outer,
                                                  app->pending_listing_addr);
        if (done) {
            app->pending_listing_addr = -1;
            app->pending_listing_retries = 0;
            return G_SOURCE_REMOVE;
        }
        if (app->pending_listing_retries > 0) {
            app->pending_listing_retries--;
            return G_SOURCE_CONTINUE;
        }
        app->pending_listing_addr = -1;
        app->pending_listing_retries = 0;
    }
    return G_SOURCE_REMOVE;
}

static void controller_segment_command(gpointer data, const UISegmentCmd *cmd,
                                       UISegmentCmdResult *res) {
    App *app = data;
    if (!res) return;
    memset(res, 0, sizeof(*res));
    res->ok = FALSE;
    res->resulting_index = -1;

    if (!app || !cmd) {
        snprintf(res->error, sizeof(res->error), "Invalid segment command context.");
        return;
    }
    if (app->ui_busy) {
        snprintf(res->error, sizeof(res->error), "UI is busy reloading. Try again.");
        return;
    }

    if (cmd->type == UI_SEGMENT_CMD_ADD) {
        if (!app->project.map) {
            app->project.map = memmap_new();
            if (!app->project.map) {
                snprintf(res->error, sizeof(res->error), "Out of memory creating segment map.");
                return;
            }
        }
        if (memmap_add(app->project.map, &cmd->entry) != 0) {
            snprintf(res->error, sizeof(res->error), "Failed to add segment.");
            return;
        }
        res->ok = TRUE;
        res->resulting_index = memmap_count(app->project.map) - 1;
        GUI_LOG(app, "segment add [%04X-%04X] type=%d map=%d",
                cmd->entry.start & 0xFFFF, cmd->entry.end & 0xFFFF,
                (int)cmd->entry.type, memmap_count(app->project.map));
        return;
    }

    if (cmd->type == UI_SEGMENT_CMD_UPDATE) {
        if (!app->project.map || memmap_replace(app->project.map, cmd->index, &cmd->entry) != 0) {
            snprintf(res->error, sizeof(res->error), "Failed to update segment.");
            return;
        }
        res->ok = TRUE;
        res->resulting_index = cmd->index;
        GUI_LOG(app, "segment update idx=%d [%04X-%04X] type=%d map=%d",
                cmd->index, cmd->entry.start & 0xFFFF, cmd->entry.end & 0xFFFF,
                (int)cmd->entry.type, memmap_count(app->project.map));
        return;
    }

    if (cmd->type == UI_SEGMENT_CMD_REMOVE) {
        if (!app->project.map || memmap_remove(app->project.map, cmd->index) != 0) {
            snprintf(res->error, sizeof(res->error), "Failed to remove segment.");
            return;
        }
        res->ok = TRUE;
        {
            int n = memmap_count(app->project.map);
            res->resulting_index = (cmd->index < n) ? cmd->index : (n - 1);
        }
        GUI_LOG(app, "segment remove idx=%d map=%d", cmd->index, memmap_count(app->project.map));
        return;
    }

    snprintf(res->error, sizeof(res->error), "Unknown segment command.");
}

static void controller_segment_save(gpointer data, const UISegmentSaveRequest *req) {
    App *app = data;
    if (!app || !req) return;
    GUI_LOG(app, "segment save needs_reload=%d have_jump=%d jump=0x%04X map=%d",
            req->needs_reload ? 1 : 0, req->have_jump ? 1 : 0,
            req->have_jump ? (req->jump_addr & 0xFFFF) : 0xFFFF,
            memmap_count(app->project.map));
    if (req->have_jump)
        controller_request_jump(app, req->jump_addr);
    if (req->needs_reload)
        app->pending_segment_reload = TRUE;
    if (!app->segment_save_idle_queued) {
        app->segment_save_idle_queued = TRUE;
        g_idle_add(controller_segment_save_idle, app);
    }
}

static gboolean controller_segment_save_idle(gpointer user_data) {
    App *app = user_data;
    if (!app) return G_SOURCE_REMOVE;
    app->segment_save_idle_queued = FALSE;
    if (app->pending_segment_reload) {
        app->pending_segment_reload = FALSE;
        controller_reload_and_render(app);
    }
    return G_SOURCE_REMOVE;
}

/* --------------------------------------------------------------------------
 * Browse-folder callback (used inside the new-project dialog)
 * -------------------------------------------------------------------------- */

static void on_browse_response(GObject *source, GAsyncResult *res, gpointer user_data) {
    ImportCtx *ctx = user_data;
    GFile *file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), res, NULL);
    if (!file) return;
    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (!path) return;
    strncpy(ctx->proj_parent, path, sizeof(ctx->proj_parent) - 1);
    gtk_label_set_text(GTK_LABEL(ctx->folder_label), path);
    g_free(path);
}

static void on_browse_clicked(GtkButton *button, gpointer user_data) {
    ImportCtx *ctx = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Choose project location");
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(ctx->dialog), NULL,
                                  on_browse_response, ctx);
    g_object_unref(dialog);
}

/* --------------------------------------------------------------------------
 * New-project dialog buttons
 * -------------------------------------------------------------------------- */

static void on_import_cancel(GtkButton *button, gpointer user_data) {
    ImportCtx *ctx = user_data;
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    g_free(ctx);
}

static void on_import_create(GtkButton *button, gpointer user_data) {
    ImportCtx *ctx = user_data;

    /* Parse origin address — accept plain decimal or 0x hex */
    const char *origin_text = gtk_editable_get_text(GTK_EDITABLE(ctx->origin_entry));
    int load_addr = (int)strtol(origin_text, NULL, 0);

    /* Build project dir: proj_parent / basename-without-extension */
    const char *basename = strrchr(ctx->bin_path, '/');
    basename = basename ? basename + 1 : ctx->bin_path;
    char name[256];
    strncpy(name, basename, sizeof(name) - 1);
    char *dot = strrchr(name, '.');
    if (dot && dot != name) *dot = '\0';

    char proj_dir[1024];
    snprintf(proj_dir, sizeof(proj_dir), "%s/%s", ctx->proj_parent, name);

    App *app = ctx->app;
    char bin_path_copy[512];
    strncpy(bin_path_copy, ctx->bin_path, sizeof(bin_path_copy) - 1);
    bin_path_copy[sizeof(bin_path_copy) - 1] = '\0';

    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    g_free(ctx);

    project_close(&app->project);
    app->project_path[0] = '\0';

    if (project_import(&app->project, bin_path_copy, proj_dir, load_addr) == 0) {
        strncpy(app->project_path, proj_dir, sizeof(app->project_path) - 1);
        app->pending_listing_addr = -1;
        app->pending_listing_retries = 0;
        controller_render(app);
    } else {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Could not create project.");
        gtk_alert_dialog_show(alert, GTK_WINDOW(app->window));
        g_object_unref(alert);
    }
}

/* --------------------------------------------------------------------------
 * New-project dialog — shown when the chosen .bin has no project folder yet
 * -------------------------------------------------------------------------- */

static void show_new_project_dialog(App *app, const char *bin_path) {
    ImportCtx *ctx = g_new0(ImportCtx, 1);
    ctx->app = app;
    strncpy(ctx->bin_path, bin_path, sizeof(ctx->bin_path) - 1);

    /* Default project parent = directory containing the .bin */
    strncpy(ctx->proj_parent, bin_path, sizeof(ctx->proj_parent) - 1);
    char *slash = strrchr(ctx->proj_parent, '/');
    if (slash) *slash = '\0';
    else strcpy(ctx->proj_parent, ".");

    /* Build the dialog window */
    GtkWidget *dlg = gtk_window_new();
    ctx->dialog = dlg;
    gtk_window_set_title(GTK_WINDOW(dlg), "Create Project");
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(app->window));
    gtk_window_set_default_size(GTK_WINDOW(dlg), 460, -1);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_window_set_child(GTK_WINDOW(dlg), box);

    /* ROM filename label */
    const char *base = strrchr(bin_path, '/');
    base = base ? base + 1 : bin_path;
    char desc[512];
    snprintf(desc, sizeof(desc), "Importing: %s", base);
    GtkWidget *desc_lbl = gtk_label_new(desc);
    gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0f);
    gtk_box_append(GTK_BOX(box), desc_lbl);

    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Project folder row */
    GtkWidget *folder_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(box), folder_row);

    GtkWidget *folder_lbl = gtk_label_new("Save to:");
    gtk_box_append(GTK_BOX(folder_row), folder_lbl);

    ctx->folder_label = gtk_label_new(ctx->proj_parent);
    gtk_label_set_xalign(GTK_LABEL(ctx->folder_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(ctx->folder_label), PANGO_ELLIPSIZE_START);
    gtk_widget_set_hexpand(ctx->folder_label, TRUE);
    gtk_box_append(GTK_BOX(folder_row), ctx->folder_label);

    GtkWidget *browse_btn = gtk_button_new_with_label("Browse…");
    g_signal_connect(browse_btn, "clicked", G_CALLBACK(on_browse_clicked), ctx);
    gtk_box_append(GTK_BOX(folder_row), browse_btn);

    /* Origin address row */
    GtkWidget *origin_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(box), origin_row);

    GtkWidget *origin_lbl = gtk_label_new("Origin address:");
    gtk_box_append(GTK_BOX(origin_row), origin_lbl);

    ctx->origin_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->origin_entry), "0x0000");
    gtk_widget_set_hexpand(ctx->origin_entry, TRUE);
    gtk_box_append(GTK_BOX(origin_row), ctx->origin_entry);

    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Cancel / Create buttons */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(box), btn_row);

    /* Push buttons to the right */
    gtk_box_append(GTK_BOX(btn_row), gtk_label_new(""));
    gtk_widget_set_hexpand(gtk_widget_get_last_child(btn_row), TRUE);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_import_cancel), ctx);
    gtk_box_append(GTK_BOX(btn_row), cancel_btn);

    GtkWidget *create_btn = gtk_button_new_with_label("Create");
    g_signal_connect(create_btn, "clicked", G_CALLBACK(on_import_create), ctx);
    gtk_box_append(GTK_BOX(btn_row), create_btn);

    gtk_window_present(GTK_WINDOW(dlg));
}

/* --------------------------------------------------------------------------
 * Open project folder — select the project directory directly
 * -------------------------------------------------------------------------- */

static void on_open_project_response(GObject *source, GAsyncResult *res, gpointer user_data) {
    App *app = user_data;
    GFile *file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), res, NULL);
    if (!file) return;

    char *proj_dir = g_file_get_path(file);
    g_object_unref(file);
    if (!proj_dir) return;

    project_close(&app->project);
    app->project_path[0] = '\0';
    if (project_open(&app->project, proj_dir) == 0) {
        strncpy(app->project_path, proj_dir, sizeof(app->project_path) - 1);
        controller_render(app);
    } else {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Could not open project.");
        gtk_alert_dialog_show(alert, GTK_WINDOW(app->window));
        g_object_unref(alert);
    }
    g_free(proj_dir);
}

static void on_open_project_clicked(GtkButton *button, gpointer user_data) {
    App *app = user_data;

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Project Folder");
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(app->window), NULL,
                                  on_open_project_response, app);
    g_object_unref(dialog);
}

static void on_new_project_response(GObject *source, GAsyncResult *res, gpointer user_data) {
    App *app = user_data;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), res, NULL);
    if (!file) return;

    char *bin_path = g_file_get_path(file);
    g_object_unref(file);
    if (!bin_path) return;

    show_new_project_dialog(app, bin_path);
    g_free(bin_path);
}

static void on_new_project_clicked(GtkButton *button, gpointer user_data) {
    App *app = user_data;

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Create Project from Binary");

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "ROM binaries (*.bin)");
    gtk_file_filter_add_pattern(filter, "*.bin");

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
    g_object_unref(filter);

    gtk_file_dialog_open(dialog, GTK_WINDOW(app->window), NULL,
                         on_new_project_response, app);
    g_object_unref(dialog);
}

/* --------------------------------------------------------------------------
 * Save / Export
 * -------------------------------------------------------------------------- */

static void on_save_clicked(GtkButton *button, gpointer user_data) {
    App *app = user_data;
    if (app->project.rom && app->project_path[0])
        project_save(&app->project, app->project_path);
}

static void on_export_lst_clicked(GtkButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app->project.rom || !app->project_path[0]) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/export.lst", app->project_path);
    export_lst(&app->project, path);
}

static void on_export_asm_clicked(GtkButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app->project.rom || !app->project_path[0]) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/export.asm", app->project_path);
    export_asm(&app->project, path);
}

/* --------------------------------------------------------------------------
 * App activation
 * -------------------------------------------------------------------------- */

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    App *app = user_data;

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        ".monospace { font-family: monospace; font-size: 12px; }\n"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "z80bench");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1180, 900);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(app->window), main_box);

    /* Toolbar */
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_top(toolbar, 5);
    gtk_widget_set_margin_bottom(toolbar, 5);
    gtk_widget_set_margin_start(toolbar, 5);
    gtk_widget_set_margin_end(toolbar, 5);
    gtk_box_append(GTK_BOX(main_box), toolbar);

    GtkWidget *btn_open = gtk_button_new_with_label("Open Project");
    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open_project_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_open);

    GtkWidget *btn_new = gtk_button_new_with_label("New Project");
    g_signal_connect(btn_new, "clicked", G_CALLBACK(on_new_project_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_new);

    GtkWidget *btn_save = gtk_button_new_with_label("Save");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_save);

    GtkWidget *toolbar_spacer = gtk_label_new("");
    gtk_widget_set_hexpand(toolbar_spacer, TRUE);
    gtk_box_append(GTK_BOX(toolbar), toolbar_spacer);

    GtkWidget *btn_lst = gtk_button_new_with_label("Export .LST");
    g_signal_connect(btn_lst, "clicked", G_CALLBACK(on_export_lst_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_lst);

    GtkWidget *btn_asm = gtk_button_new_with_label("Export .ASM");
    g_signal_connect(btn_asm, "clicked", G_CALLBACK(on_export_asm_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_asm);

    /* Main content area (listing + side panels) */
    app->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    app->pending_listing_addr = -1;
    app->pending_listing_retries = 0;
    gtk_paned_set_position(GTK_PANED(app->paned), 700);
    gtk_paned_set_resize_start_child(GTK_PANED(app->paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(app->paned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(app->paned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(app->paned), FALSE);
    g_signal_connect(app->paned, "notify::position",
                     G_CALLBACK(on_main_paned_position_changed), app);

    gtk_paned_set_start_child(GTK_PANED(app->paned),
        gtk_label_new("Open a .bin ROM file to begin."));
    gtk_paned_set_end_child(GTK_PANED(app->paned), gtk_label_new(""));

    /* Vertical split: main content over bottom dock (hex + search) */
    app->main_vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_position(GTK_PANED(app->main_vpaned), 580);
    gtk_paned_set_resize_start_child(GTK_PANED(app->main_vpaned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(app->main_vpaned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(app->main_vpaned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(app->main_vpaned), FALSE);
    gtk_widget_set_vexpand(app->main_vpaned, TRUE);

    gtk_paned_set_start_child(GTK_PANED(app->main_vpaned), app->paned);
    gtk_paned_set_end_child(GTK_PANED(app->main_vpaned), build_bottom_dock(app));
    gtk_box_append(GTK_BOX(main_box), app->main_vpaned);

    app_render_hex_dump(app);

    gtk_window_present(GTK_WINDOW(app->window));
}

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
    App app;
    memset(&app, 0, sizeof(App));
    app.ui_busy = FALSE;

    GtkApplication *gtk_app =
        gtk_application_new("io.github.z80bench", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), &app);
    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);

    project_close(&app.project);
    g_object_unref(gtk_app);
    return status;
}
