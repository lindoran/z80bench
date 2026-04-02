#include <gtk/gtk.h>
#include <glib/gstdio.h>
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
    GtkWidget *search_box;
    GtkWidget *search_mode_addr;
    GtkWidget *search_mode_mnem;
    GtkWidget *search_mode_comment;
    GtkWidget *search_addr_refs_check;
    char       project_path[512];
    int        pending_listing_addr;
    int        pending_listing_retries;
    gboolean   ui_busy;
    gboolean   pending_segment_reload;
    gboolean   segment_save_idle_queued;
    gboolean   auto_open_last;
    GtkWidget *auto_open_check;
    gboolean   highlight_ascii_runs;
    GtkWidget *highlight_ascii_check;
    gboolean   highlight_highbit_runs;
    GtkWidget *highlight_highbit_check;
    gboolean   highlight_ptr_in_rom;
    GtkWidget *highlight_ptr_in_rom_check;
    gboolean   highlight_ptr_out_rom;
    GtkWidget *highlight_ptr_out_rom_check;
    gboolean   ptr_ignore_ascii_overlap;
    GtkWidget *ptr_ignore_ascii_overlap_check;
    char       last_project_path[512];
    int        recent_count;
    char       recent_paths[10][512];
    int        saved_main_split;
    int        saved_main_vsplit;
    int        saved_dock_split;
    gboolean   syncing_split_positions;
    gboolean   pending_listing_jump_confirmed;
    GtkWidget *about_dialog;
} App;

#define RESPONSIVE_NARROW_WIDTH 1100
#define DEFAULT_MAIN_SPLIT 1011
#define DEFAULT_DOCK_SPLIT 1011
#define DEFAULT_MAIN_VSPLIT 379

typedef struct {
    GtkWidget *paned;
    int        pos;
} PanedPosApply;

typedef struct {
    GtkWidget *window;
} WindowSizeUnlock;

typedef struct {
    App       *app;
    GtkWidget *dialog;
    char       path[512];
} RecentItemCtx;

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */

static void show_new_project_dialog(App *app, const char *bin_path);
static void on_open_project_clicked(GtkButton *button, gpointer user_data);
static void on_new_project_clicked(GtkButton *button, gpointer user_data);
static void on_open_recent_clicked(GtkButton *button, gpointer user_data);
static void on_auto_open_toggled(GtkCheckButton *button, gpointer user_data);
static void on_highlight_ascii_toggled(GtkCheckButton *button, gpointer user_data);
static void on_highlight_highbit_toggled(GtkCheckButton *button, gpointer user_data);
static void on_highlight_ptr_in_rom_toggled(GtkCheckButton *button, gpointer user_data);
static void on_highlight_ptr_out_rom_toggled(GtkCheckButton *button, gpointer user_data);
static void on_ptr_ignore_ascii_overlap_toggled(GtkCheckButton *button, gpointer user_data);
static void on_search_addr_refs_toggled(GtkCheckButton *button, gpointer user_data);
static void on_about_clicked(GtkButton *button, gpointer user_data);
static void on_about_dialog_destroy(GtkWidget *widget, gpointer user_data);

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
static void controller_render(App *app);  /* forward */
static void controller_reload_and_render(gpointer user_data);  /* forward */
static void controller_segment_command(gpointer data, const UISegmentCmd *cmd,
                                       UISegmentCmdResult *res);  /* forward */
static void controller_segment_save(gpointer data, const UISegmentSaveRequest *req);  /* forward */
static gboolean controller_segment_save_idle(gpointer user_data);  /* forward */
static gboolean controller_apply_paned_position_idle(gpointer user_data);  /* forward */
static gboolean controller_unlock_window_size_idle(gpointer user_data);  /* forward */
static void controller_listing_visible_changed(gpointer user_data, int addr);  /* forward */
static void controller_focus_search(gpointer user_data);  /* forward */
static void controller_apply_responsive_layout(App *app);  /* forward */
static void on_window_size_changed(GObject *obj, GParamSpec *pspec,
                                   gpointer user_data);  /* forward */

static void app_render_hex_dump(App *app);  /* forward */
static void app_sync_hex_to_addr(App *app, int addr);  /* forward */
static GtkWidget *build_bottom_dock(App *app);  /* forward */
static UISearchMode app_get_search_mode(App *app);  /* forward */
static void on_dock_search_activate(GtkEntry *entry, gpointer user_data);  /* forward */
static void on_dock_search_next(GtkButton *button, gpointer user_data);  /* forward */
static void on_main_paned_position_changed(GObject *obj, GParamSpec *pspec,
                                           gpointer user_data);  /* forward */
static gboolean app_open_project(App *app, const char *proj_dir, gboolean show_alert);
static void app_load_persistence(App *app);
static void app_load_ui_defaults(App *app);
static void app_save_config(App *app);
static void app_save_state(App *app);
static void app_add_recent_project(App *app, const char *proj_dir);

static int controller_pick_paned_position(App *app, int current_pos) {
    int win_w = gtk_widget_get_width(app->window);
    int min_pos = 300;
    int max_pos = (win_w > 700) ? (win_w - 260) : 9000;
    if (max_pos < min_pos) max_pos = min_pos;

    /* Default left/right split baseline. */
    int fallback = 965;
    if (fallback < min_pos) fallback = min_pos;
    if (fallback > max_pos) fallback = max_pos;

    if (current_pos <= 0) {
        if (app && app->saved_main_split > 0) {
            int saved = app->saved_main_split;
            if (saved < min_pos) saved = min_pos;
            if (saved > max_pos) saved = max_pos;
            return saved;
        }
        return fallback;
    }
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

    gboolean *highlight = NULL;
    gboolean *highlight_high = NULL;
    gboolean *highlight_ptr_in = NULL;
    gboolean *highlight_ptr_out = NULL;
    if (app->highlight_ascii_runs) {
        highlight = g_new0(gboolean, (gsize)app->project.rom_size);
        int min_run = 3;
        /* Keep runs text-like: printable bytes plus common text controls. */
        #define IS_TEXT_CTRL(b) ((b) == 0x00 || (b) == 0x09 || (b) == 0x0A || (b) == 0x0D)
        #define IS_TEXTLIKE(b)  (((b) >= 0x20 && (b) < 0x7F) || IS_TEXT_CTRL(b))
        for (int i = 0; i < app->project.rom_size; ) {
            unsigned char c = app->project.rom[i];
            if (!IS_TEXTLIKE(c)) {
                i++;
                continue;
            }
            int start = i;
            int printable_count = 0;
            while (i < app->project.rom_size) {
                c = app->project.rom[i];
                if (!IS_TEXTLIKE(c))
                    break;
                if (c >= 0x20 && c < 0x7F)
                    printable_count++;
                i++;
            }
            int len = i - start;
            if (len >= min_run && printable_count >= min_run) {
                for (int j = start; j < i; j++)
                    highlight[j] = TRUE;
            }
        }
        #undef IS_TEXT_CTRL
        #undef IS_TEXTLIKE
    }
    if (app->highlight_highbit_runs) {
        highlight_high = g_new0(gboolean, (gsize)app->project.rom_size);
        int min_run = 3;
        for (int i = 0; i < app->project.rom_size; ) {
            unsigned char c = app->project.rom[i];
            if (c < 0x80) {
                i++;
                continue;
            }
            int start = i;
            while (i < app->project.rom_size && app->project.rom[i] >= 0x80)
                i++;
            int len = i - start;
            if (len >= min_run) {
                for (int j = start; j < i; j++)
                    highlight_high[j] = TRUE;
            }
        }
    }
    if (app->highlight_ptr_in_rom || app->highlight_ptr_out_rom) {
        int rom_start = app->project.load_addr;
        int rom_end = rom_start + app->project.rom_size - 1;
        if (app->highlight_ptr_in_rom)
            highlight_ptr_in = g_new0(gboolean, (gsize)app->project.rom_size);
        if (app->highlight_ptr_out_rom)
            highlight_ptr_out = g_new0(gboolean, (gsize)app->project.rom_size);

        /* Refined: only highlight real parameter-word bytes that appear in
         * decoded listing lines (not every sliding byte pair in ROM). */
        for (int li = 0; li < app->project.nlines; li++) {
            const DisasmLine *dl = &app->project.lines[li];
            if (!dl) continue;
            if (dl->operand_addr < 0 || dl->operand_addr > 0xFFFF)
                continue;
            if (dl->offset < 0 || dl->byte_count < 2)
                continue;

            int v = dl->operand_addr & 0xFFFF;
            unsigned char lo = (unsigned char)(v & 0xFF);
            unsigned char hi = (unsigned char)((v >> 8) & 0xFF);
            gboolean in_rom = (v >= rom_start && v <= rom_end);

            for (int b = 0; b + 1 < dl->byte_count; b++) {
                int off0 = dl->offset + b;
                int off1 = off0 + 1;
                if (off0 < 0 || off1 >= app->project.rom_size)
                    continue;
                if (app->project.rom[off0] != lo || app->project.rom[off1] != hi)
                    continue;

                if (app->ptr_ignore_ascii_overlap && highlight &&
                    (highlight[off0] || highlight[off1]))
                    continue;

                if (in_rom && highlight_ptr_in) {
                    highlight_ptr_in[off0] = TRUE;
                    highlight_ptr_in[off1] = TRUE;
                } else if (!in_rom && highlight_ptr_out) {
                    highlight_ptr_out[off0] = TRUE;
                    highlight_ptr_out[off1] = TRUE;
                }
            }
        }
    }

    GtkTextTagTable *tt = gtk_text_buffer_get_tag_table(buf);
    GtkTextTag *tag = gtk_text_tag_table_lookup(tt, "ascii-run");
    if (!tag) {
        tag = gtk_text_buffer_create_tag(buf, "ascii-run",
                                         "foreground", "#E69F00",
                                         "weight", PANGO_WEIGHT_BOLD,
                                         NULL);
    }
    GtkTextTag *tag_high = gtk_text_tag_table_lookup(tt, "highbit-run");
    if (!tag_high) {
        tag_high = gtk_text_buffer_create_tag(buf, "highbit-run",
                                              "foreground", "#56B4E9",
                                              "weight", PANGO_WEIGHT_BOLD,
                                              NULL);
    }
    GtkTextTag *tag_ptr_in = gtk_text_tag_table_lookup(tt, "ptr-in-rom");
    if (!tag_ptr_in) {
        tag_ptr_in = gtk_text_buffer_create_tag(buf, "ptr-in-rom",
                                                "foreground", "#0072B2",
                                                "weight", PANGO_WEIGHT_BOLD,
                                                NULL);
    }
    GtkTextTag *tag_ptr_out = gtk_text_tag_table_lookup(tt, "ptr-out-rom");
    if (!tag_ptr_out) {
        tag_ptr_out = gtk_text_buffer_create_tag(buf, "ptr-out-rom",
                                                 "foreground", "#CC79A7",
                                                 "weight", PANGO_WEIGHT_BOLD,
                                                 NULL);
    }

    gtk_text_buffer_set_text(buf, "", 0);
    GtkTextIter it;
    gtk_text_buffer_get_start_iter(buf, &it);

    for (int off = 0; off < app->project.rom_size; off += 16) {
        int addr = app->project.load_addr + off;
        char head[16];
        snprintf(head, sizeof(head), "%04X: ", addr & 0xFFFF);
        gtk_text_buffer_insert(buf, &it, head, -1);

        for (int i = 0; i < 16; i++) {
            if (off + i < app->project.rom_size) {
                char hx[4];
                snprintf(hx, sizeof(hx), "%02X ", app->project.rom[off + i]);
                if (highlight_ptr_in && highlight_ptr_in[off + i])
                    gtk_text_buffer_insert_with_tags(buf, &it, hx, -1, tag_ptr_in, NULL);
                else if (highlight_ptr_out && highlight_ptr_out[off + i])
                    gtk_text_buffer_insert_with_tags(buf, &it, hx, -1, tag_ptr_out, NULL);
                else if (highlight && highlight[off + i])
                    gtk_text_buffer_insert_with_tags(buf, &it, hx, -1, tag, NULL);
                else if (highlight_high && highlight_high[off + i])
                    gtk_text_buffer_insert_with_tags(buf, &it, hx, -1, tag_high, NULL);
                else
                    gtk_text_buffer_insert(buf, &it, hx, -1);
            } else {
                gtk_text_buffer_insert(buf, &it, "   ", 3);
            }
        }

        gtk_text_buffer_insert(buf, &it, " ", 1);

        for (int i = 0; i < 16 && off + i < app->project.rom_size; i++) {
            unsigned char c = app->project.rom[off + i];
            char ch = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
            if (highlight_ptr_in && highlight_ptr_in[off + i])
                gtk_text_buffer_insert_with_tags(buf, &it, &ch, 1, tag_ptr_in, NULL);
            else if (highlight_ptr_out && highlight_ptr_out[off + i])
                gtk_text_buffer_insert_with_tags(buf, &it, &ch, 1, tag_ptr_out, NULL);
            else if (highlight && highlight[off + i])
                gtk_text_buffer_insert_with_tags(buf, &it, &ch, 1, tag, NULL);
            else if (highlight_high && highlight_high[off + i])
                gtk_text_buffer_insert_with_tags(buf, &it, &ch, 1, tag_high, NULL);
            else
                gtk_text_buffer_insert(buf, &it, &ch, 1);
        }

        gtk_text_buffer_insert(buf, &it, "\n", 1);
    }

    g_free(highlight);
    g_free(highlight_high);
    g_free(highlight_ptr_in);
    g_free(highlight_ptr_out);
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

static char *app_config_file_path(void) {
    return g_build_filename(g_get_user_config_dir(), "z80bench", "config.ini", NULL);
}

static char *app_state_file_path(void) {
    return g_build_filename(g_get_user_state_dir(), "z80bench", "state.ini", NULL);
}

static gboolean app_write_key_file(const char *path, GKeyFile *kf) {
    gsize len = 0;
    gchar *content = g_key_file_to_data(kf, &len, NULL);
    if (!content) return FALSE;

    char *dir = g_path_get_dirname(path);
    if (g_mkdir_with_parents(dir, 0755) != 0) {
        g_free(content);
        g_free(dir);
        return FALSE;
    }
    g_free(dir);

    gboolean ok = g_file_set_contents(path, content, (gssize)len, NULL);
    g_free(content);
    return ok;
}

static void app_load_persistence(App *app) {
    if (!app) return;
    app->auto_open_last = FALSE;
    app->recent_count = 0;
    app->last_project_path[0] = '\0';
    app->saved_main_split = -1;
    app->saved_main_vsplit = -1;
    app->saved_dock_split = -1;

    char *config_path = app_config_file_path();
    GKeyFile *config = g_key_file_new();
    if (g_key_file_load_from_file(config, config_path, G_KEY_FILE_NONE, NULL)) {
        if (g_key_file_has_key(config, "session", "auto_open_last", NULL))
            app->auto_open_last = g_key_file_get_boolean(config, "session", "auto_open_last", NULL);
    }
    g_key_file_unref(config);
    g_free(config_path);

    char *state_path = app_state_file_path();
    GKeyFile *state = g_key_file_new();
    if (g_key_file_load_from_file(state, state_path, G_KEY_FILE_NONE, NULL)) {
        gchar *last = g_key_file_get_string(state, "session", "last_project", NULL);
        if (last) {
            g_strlcpy(app->last_project_path, last, sizeof(app->last_project_path));
            g_free(last);
        }

        gsize nrecents = 0;
        gchar **recents = g_key_file_get_string_list(state, "recent", "items", &nrecents, NULL);
        if (recents) {
            for (gsize i = 0; i < nrecents && app->recent_count < 10; i++) {
                if (recents[i] && recents[i][0]) {
                    g_strlcpy(app->recent_paths[app->recent_count],
                              recents[i], sizeof(app->recent_paths[0]));
                    app->recent_count++;
                }
            }
            g_strfreev(recents);
        }
        if (g_key_file_has_key(state, "ui", "main_split", NULL))
            app->saved_main_split = g_key_file_get_integer(state, "ui", "main_split", NULL);
        if (g_key_file_has_key(state, "ui", "main_vsplit", NULL))
            app->saved_main_vsplit = g_key_file_get_integer(state, "ui", "main_vsplit", NULL);
        if (g_key_file_has_key(state, "ui", "dock_split", NULL))
            app->saved_dock_split = g_key_file_get_integer(state, "ui", "dock_split", NULL);
    }
    g_key_file_unref(state);
    g_free(state_path);
}

static void app_load_ui_defaults(App *app) {
    if (!app) return;

    /* Baseline defaults when UI state is missing or disabled by policy. */
    app->saved_main_split = DEFAULT_MAIN_SPLIT;
    app->saved_dock_split = DEFAULT_DOCK_SPLIT;
    app->saved_main_vsplit = DEFAULT_MAIN_VSPLIT;

    /* If auto-open-last is disabled, do not restore UI sizes from state.ini. */
    if (!app->auto_open_last)
        return;

    char *state_path = app_state_file_path();
    GKeyFile *state = g_key_file_new();
    if (g_key_file_load_from_file(state, state_path, G_KEY_FILE_NONE, NULL)) {
        if (g_key_file_has_key(state, "ui", "main_split", NULL))
            app->saved_main_split = g_key_file_get_integer(state, "ui", "main_split", NULL);
        if (g_key_file_has_key(state, "ui", "main_vsplit", NULL))
            app->saved_main_vsplit = g_key_file_get_integer(state, "ui", "main_vsplit", NULL);
        if (g_key_file_has_key(state, "ui", "dock_split", NULL))
            app->saved_dock_split = g_key_file_get_integer(state, "ui", "dock_split", NULL);
    }
    g_key_file_unref(state);
    g_free(state_path);
}

static void app_save_config(App *app) {
    if (!app) return;
    char *config_path = app_config_file_path();
    GKeyFile *config = g_key_file_new();
    g_key_file_set_boolean(config, "session", "auto_open_last", app->auto_open_last);
    app_write_key_file(config_path, config);
    g_key_file_unref(config);
    g_free(config_path);
}

static void app_save_state(App *app) {
    if (!app) return;
    char *state_path = app_state_file_path();
    GKeyFile *state = g_key_file_new();

    if (app->paned) {
        int pos = gtk_paned_get_position(GTK_PANED(app->paned));
        if (pos > 0)
            app->saved_main_split = pos;
    }
    if (app->main_vpaned) {
        int pos = gtk_paned_get_position(GTK_PANED(app->main_vpaned));
        if (pos > 0)
            app->saved_main_vsplit = pos;
    }
    if (app->dock_paned) {
        int pos = gtk_paned_get_position(GTK_PANED(app->dock_paned));
        if (pos > 0)
            app->saved_dock_split = pos;
    }

    g_key_file_set_string(state, "session", "last_project",
                          app->last_project_path[0] ? app->last_project_path : "");
    if (app->recent_count > 0) {
        const gchar *vals[10];
        for (int i = 0; i < app->recent_count; i++)
            vals[i] = app->recent_paths[i];
        g_key_file_set_string_list(state, "recent", "items", vals, (gsize)app->recent_count);
    } else {
        g_key_file_set_string_list(state, "recent", "items", NULL, 0);
    }
    if (app->saved_main_split > 0)
        g_key_file_set_integer(state, "ui", "main_split", app->saved_main_split);
    if (app->saved_main_vsplit > 0)
        g_key_file_set_integer(state, "ui", "main_vsplit", app->saved_main_vsplit);
    if (app->saved_dock_split > 0)
        g_key_file_set_integer(state, "ui", "dock_split", app->saved_dock_split);

    app_write_key_file(state_path, state);
    g_key_file_unref(state);
    g_free(state_path);
}

static void app_add_recent_project(App *app, const char *proj_dir) {
    if (!app || !proj_dir || !proj_dir[0]) return;

    int existing = -1;
    for (int i = 0; i < app->recent_count; i++) {
        if (strcmp(app->recent_paths[i], proj_dir) == 0) {
            existing = i;
            break;
        }
    }

    if (existing == 0) {
        g_strlcpy(app->last_project_path, proj_dir, sizeof(app->last_project_path));
        app_save_state(app);
        return;
    }

    if (existing > 0) {
        for (int i = existing; i > 0; i--)
            g_strlcpy(app->recent_paths[i], app->recent_paths[i - 1], sizeof(app->recent_paths[0]));
    } else {
        if (app->recent_count < 10)
            app->recent_count++;
        for (int i = app->recent_count - 1; i > 0; i--)
            g_strlcpy(app->recent_paths[i], app->recent_paths[i - 1], sizeof(app->recent_paths[0]));
    }
    g_strlcpy(app->recent_paths[0], proj_dir, sizeof(app->recent_paths[0]));
    g_strlcpy(app->last_project_path, proj_dir, sizeof(app->last_project_path));
    app_save_state(app);
}

static gboolean app_open_project(App *app, const char *proj_dir, gboolean show_alert) {
    if (!app || !proj_dir || !proj_dir[0]) return FALSE;

    project_close(&app->project);
    app->project_path[0] = '\0';
    if (project_open(&app->project, proj_dir) != 0) {
        if (show_alert && app->window) {
            GtkAlertDialog *alert = gtk_alert_dialog_new("Could not open project.");
            gtk_alert_dialog_show(alert, GTK_WINDOW(app->window));
            g_object_unref(alert);
        }
        return FALSE;
    }

    g_strlcpy(app->project_path, proj_dir, sizeof(app->project_path));
    app_load_ui_defaults(app);
    if (app->paned)
        gtk_paned_set_position(GTK_PANED(app->paned), 0);
    if (app->dock_paned)
        gtk_paned_set_position(GTK_PANED(app->dock_paned), 0);
    if (app->main_vpaned && app->saved_main_vsplit > 0)
        gtk_paned_set_position(GTK_PANED(app->main_vpaned), app->saved_main_vsplit);
    app->pending_listing_addr = -1;
    app->pending_listing_retries = 0;
    controller_render(app);
    app_add_recent_project(app, proj_dir);
    return TRUE;
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

static void controller_apply_responsive_layout(App *app) {
    if (!app || !app->window || !app->paned)
        return;

    int win_w = gtk_widget_get_width(app->window);
    int win_h = gtk_widget_get_height(app->window);
    gboolean narrow = (win_w > 0 && win_w < RESPONSIVE_NARROW_WIDTH);
    GtkOrientation orient = narrow ? GTK_ORIENTATION_VERTICAL
                                   : GTK_ORIENTATION_HORIZONTAL;

    gtk_orientable_set_orientation(GTK_ORIENTABLE(app->paned), orient);
    gtk_paned_set_resize_start_child(GTK_PANED(app->paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(app->paned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(app->paned), narrow ? TRUE : FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(app->paned), TRUE);

    if (app->dock_paned) {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(app->dock_paned), orient);
        gtk_paned_set_resize_start_child(GTK_PANED(app->dock_paned), TRUE);
        gtk_paned_set_resize_end_child(GTK_PANED(app->dock_paned), narrow ? TRUE : FALSE);
        gtk_paned_set_shrink_start_child(GTK_PANED(app->dock_paned), TRUE);
        gtk_paned_set_shrink_end_child(GTK_PANED(app->dock_paned), TRUE);
    }

    if (app->panels_outer) {
        if (narrow) {
            gtk_widget_set_size_request(app->panels_outer, -1, 280);
            gtk_widget_set_hexpand(app->panels_outer, TRUE);
            gtk_widget_set_vexpand(app->panels_outer, TRUE);
        } else {
            gtk_widget_set_size_request(app->panels_outer, 320, -1);
            gtk_widget_set_hexpand(app->panels_outer, FALSE);
            gtk_widget_set_vexpand(app->panels_outer, TRUE);
        }
    }

    if (app->search_box) {
        gtk_widget_set_size_request(app->search_box, narrow ? -1 : 260, -1);
        gtk_widget_set_hexpand(app->search_box, narrow ? TRUE : FALSE);
    }

    int main_pos = narrow
        ? ((win_h > 220) ? (int)(win_h * 0.58) : 420)
        : controller_pick_paned_position(app, gtk_paned_get_position(GTK_PANED(app->paned)));
    gtk_paned_set_position(GTK_PANED(app->paned), main_pos);
    if (app->dock_paned)
        gtk_paned_set_position(GTK_PANED(app->dock_paned), main_pos);
}

static void on_window_size_changed(GObject *obj, GParamSpec *pspec, gpointer user_data) {
    (void)obj;
    (void)pspec;
    controller_apply_responsive_layout((App *)user_data);
}

static GtkWidget *build_bottom_dock(App *app) {
    GtkWidget *dock_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    int main_pos = app && app->paned ? gtk_paned_get_position(GTK_PANED(app->paned)) : 630;
    if (main_pos <= 0) {
        if (app && app->saved_dock_split > 0)
            main_pos = app->saved_dock_split;
        else if (app && app->saved_main_split > 0)
            main_pos = app->saved_main_split;
        else
            main_pos = 630;
    }
    gtk_paned_set_position(GTK_PANED(dock_split), main_pos);
    gtk_paned_set_resize_start_child(GTK_PANED(dock_split), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(dock_split), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(dock_split), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(dock_split), FALSE);

    GtkWidget *hex_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hex_header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(hex_header_row, 8);
    gtk_widget_set_margin_end(hex_header_row, 8);
    gtk_widget_set_margin_top(hex_header_row, 4);
    gtk_widget_set_margin_bottom(hex_header_row, 2);
    gtk_box_append(GTK_BOX(hex_container), hex_header_row);

    GtkWidget *hex_header = gtk_label_new(
        "ADDR  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  0123456789ABCDEF");
    gtk_widget_add_css_class(hex_header, "monospace");
    gtk_label_set_xalign(GTK_LABEL(hex_header), 0.0);
    gtk_widget_set_hexpand(hex_header, TRUE);
    gtk_box_append(GTK_BOX(hex_header_row), hex_header);

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

    app->search_addr_refs_check =
        gtk_check_button_new_with_label("Addr mode: include operand refs");
    g_signal_connect(app->search_addr_refs_check, "toggled",
                     G_CALLBACK(on_search_addr_refs_toggled), app);
    gtk_box_append(GTK_BOX(search_box), app->search_addr_refs_check);

    GtkWidget *entry = gtk_entry_new();
    gtk_widget_add_css_class(entry, "monospace");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Search value");
    gtk_box_append(GTK_BOX(search_box), entry);

    GtkWidget *find_next = gtk_button_new_with_label("Find Next");
    gtk_box_append(GTK_BOX(search_box), find_next);

    GtkWidget *filter_title = gtk_label_new(NULL);
    char *fm = g_markup_printf_escaped(
        "<span size='small' color='#888'><b>HEX FILTERS</b></span>");
    gtk_label_set_markup(GTK_LABEL(filter_title), fm);
    g_free(fm);
    gtk_label_set_xalign(GTK_LABEL(filter_title), 0.0f);
    gtk_widget_set_margin_top(filter_title, 2);
    gtk_box_append(GTK_BOX(search_box), filter_title);

    GtkWidget *filters_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(filters_grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(filters_grid), 2);
    gtk_box_append(GTK_BOX(search_box), filters_grid);

    app->highlight_ascii_check = gtk_check_button_new_with_label("Text runs >=3");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(app->highlight_ascii_check),
                                app->highlight_ascii_runs);
    g_signal_connect(app->highlight_ascii_check, "toggled",
                     G_CALLBACK(on_highlight_ascii_toggled), app);
    gtk_grid_attach(GTK_GRID(filters_grid), app->highlight_ascii_check, 0, 0, 1, 1);

    app->highlight_highbit_check = gtk_check_button_new_with_label("High-bit >=3");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(app->highlight_highbit_check),
                                app->highlight_highbit_runs);
    g_signal_connect(app->highlight_highbit_check, "toggled",
                     G_CALLBACK(on_highlight_highbit_toggled), app);
    gtk_grid_attach(GTK_GRID(filters_grid), app->highlight_highbit_check, 1, 0, 1, 1);

    app->highlight_ptr_in_rom_check = gtk_check_button_new_with_label("Addr in ROM");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(app->highlight_ptr_in_rom_check),
                                app->highlight_ptr_in_rom);
    g_signal_connect(app->highlight_ptr_in_rom_check, "toggled",
                     G_CALLBACK(on_highlight_ptr_in_rom_toggled), app);
    gtk_grid_attach(GTK_GRID(filters_grid), app->highlight_ptr_in_rom_check, 0, 1, 1, 1);

    app->highlight_ptr_out_rom_check = gtk_check_button_new_with_label("Addr out ROM");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(app->highlight_ptr_out_rom_check),
                                app->highlight_ptr_out_rom);
    g_signal_connect(app->highlight_ptr_out_rom_check, "toggled",
                     G_CALLBACK(on_highlight_ptr_out_rom_toggled), app);
    gtk_grid_attach(GTK_GRID(filters_grid), app->highlight_ptr_out_rom_check, 1, 1, 1, 1);

    app->ptr_ignore_ascii_overlap_check =
        gtk_check_button_new_with_label("Skip text overlap");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(app->ptr_ignore_ascii_overlap_check),
                                app->ptr_ignore_ascii_overlap);
    g_signal_connect(app->ptr_ignore_ascii_overlap_check, "toggled",
                     G_CALLBACK(on_ptr_ignore_ascii_overlap_toggled), app);
    gtk_grid_attach(GTK_GRID(filters_grid), app->ptr_ignore_ascii_overlap_check, 0, 2, 2, 1);

    gtk_box_append(GTK_BOX(search_box), gtk_label_new(""));
    gtk_widget_set_vexpand(gtk_widget_get_last_child(search_box), TRUE);

    app->search_entry = entry;
    app->search_box = search_box;
    app->search_mode_addr = mode_addr;
    app->search_mode_mnem = mode_mnem;
    app->search_mode_comment = mode_comment;

    g_signal_connect(entry, "activate", G_CALLBACK(on_dock_search_activate), app);
    g_signal_connect(find_next, "clicked", G_CALLBACK(on_dock_search_next), app);

    gtk_paned_set_start_child(GTK_PANED(dock_split), hex_container);
    gtk_paned_set_end_child(GTK_PANED(dock_split), search_box);
    app->dock_paned = dock_split;
    g_signal_connect(app->dock_paned, "notify::position",
                     G_CALLBACK(on_main_paned_position_changed), app);
    return dock_split;
}

static void on_main_paned_position_changed(GObject *obj, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    App *app = user_data;
    if (!app) return;
    if (app->syncing_split_positions) return;

    GtkWidget *source = GTK_WIDGET(obj);
    if (source == app->main_vpaned) {
        int pos = gtk_paned_get_position(GTK_PANED(app->main_vpaned));
        if (pos > 0)
            app->saved_main_vsplit = pos;
        return;
    }

    if (!app->paned || !app->dock_paned) return;

    int pos = gtk_paned_get_position(GTK_PANED(source));
    if (pos <= 0) return;

    app->syncing_split_positions = TRUE;
    if (source == app->paned)
        gtk_paned_set_position(GTK_PANED(app->dock_paned), pos);
    else if (source == app->dock_paned)
        gtk_paned_set_position(GTK_PANED(app->paned), pos);
    app->syncing_split_positions = FALSE;

    app->saved_main_split = gtk_paned_get_position(GTK_PANED(app->paned));
    app->saved_dock_split = gtk_paned_get_position(GTK_PANED(app->dock_paned));
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
    snprintf(title, sizeof(title), "%s %s — %s",
             Z80BENCH_APP_NAME, Z80BENCH_VERSION_LABEL, app->project.name);
    gtk_window_set_title(GTK_WINDOW(app->window), title);

    GtkWidget *listing = ui_listing_new(&app->project, NULL);
    GtkWidget *panels  = ui_panels_new(&app->project, app->window,
                                       app->project_path, listing);
    ui_listing_set_panels(listing, panels);
    ui_listing_set_visible_addr_cb(listing, controller_listing_visible_changed, app);
    ui_listing_set_search_focus_cb(listing, controller_focus_search, app);
    ui_listing_set_addr_search_include_operand_refs(
        listing,
        app->search_addr_refs_check
            ? gtk_check_button_get_active(GTK_CHECK_BUTTON(app->search_addr_refs_check))
            : FALSE);
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
    controller_apply_responsive_layout(app);

    app_render_hex_dump(app);
    if (app->project.nlines > 0)
        app_sync_hex_to_addr(app, app->project.lines[0].addr);

    PanedPosApply *pp = g_new0(PanedPosApply, 1);
    pp->paned = app->paned;
    pp->pos = gtk_paned_get_position(GTK_PANED(app->paned));
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
    app->pending_listing_retries = 45;
    app->pending_listing_jump_confirmed = FALSE;
    app_sync_hex_to_addr(app, addr);
    GUI_LOG(app, "jump request 0x%04X", addr & 0xFFFF);
}

static gboolean controller_apply_pending_jump(gpointer user_data) {
    App *app = user_data;
    if (app->pending_listing_addr >= 0 && app->listing_outer) {
        (void)ui_listing_select_address(app->listing_outer, app->pending_listing_addr);
        if (app->pending_listing_retries > 0) {
            app->pending_listing_retries--;
            return G_SOURCE_CONTINUE;
        }
        app->pending_listing_addr = -1;
        app->pending_listing_retries = 0;
        app->pending_listing_jump_confirmed = FALSE;
        return G_SOURCE_REMOVE;
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
        app_open_project(app, proj_dir, TRUE);
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

    app_open_project(app, proj_dir, TRUE);
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

static void on_recent_item_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    RecentItemCtx *ctx = user_data;
    if (!ctx || !ctx->app) return;
    app_open_project(ctx->app, ctx->path, TRUE);
    if (ctx->dialog)
        gtk_window_destroy(GTK_WINDOW(ctx->dialog));
}

static void on_recent_dialog_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    GList *children = (GList *)user_data;
    for (GList *it = children; it; it = it->next)
        g_free(it->data);
    g_list_free(children);
}

static void on_open_recent_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    App *app = user_data;
    if (!app) return;

    if (app->recent_count <= 0) {
        GtkAlertDialog *alert = gtk_alert_dialog_new("No recent projects yet.");
        gtk_alert_dialog_show(alert, GTK_WINDOW(app->window));
        g_object_unref(alert);
        return;
    }

    GtkWidget *dlg = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg), "Open Recent");
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(app->window));
    gtk_window_set_default_size(GTK_WINDOW(dlg), 680, -1);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_window_set_child(GTK_WINDOW(dlg), box);

    GList *ctx_list = NULL;
    for (int i = 0; i < app->recent_count; i++) {
        if (!app->recent_paths[i][0]) continue;
        GtkWidget *btn = gtk_button_new_with_label(app->recent_paths[i]);
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        gtk_widget_set_hexpand(btn, TRUE);

        RecentItemCtx *ctx = g_new0(RecentItemCtx, 1);
        ctx->app = app;
        ctx->dialog = dlg;
        g_strlcpy(ctx->path, app->recent_paths[i], sizeof(ctx->path));
        ctx_list = g_list_prepend(ctx_list, ctx);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_recent_item_clicked), ctx);
        gtk_box_append(GTK_BOX(box), btn);
    }

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_destroy), dlg);
    gtk_box_append(GTK_BOX(box), close_btn);

    g_signal_connect(dlg, "destroy", G_CALLBACK(on_recent_dialog_destroy), ctx_list);
    gtk_window_present(GTK_WINDOW(dlg));
}

static void on_auto_open_toggled(GtkCheckButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app) return;
    app->auto_open_last = gtk_check_button_get_active(button);
    app_save_config(app);
}

static void on_highlight_ascii_toggled(GtkCheckButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app) return;
    app->highlight_ascii_runs = gtk_check_button_get_active(button);
    app_render_hex_dump(app);
}

static void on_highlight_highbit_toggled(GtkCheckButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app) return;
    app->highlight_highbit_runs = gtk_check_button_get_active(button);
    app_render_hex_dump(app);
}

static void on_highlight_ptr_in_rom_toggled(GtkCheckButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app) return;
    app->highlight_ptr_in_rom = gtk_check_button_get_active(button);
    app_render_hex_dump(app);
}

static void on_highlight_ptr_out_rom_toggled(GtkCheckButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app) return;
    app->highlight_ptr_out_rom = gtk_check_button_get_active(button);
    app_render_hex_dump(app);
}

static void on_ptr_ignore_ascii_overlap_toggled(GtkCheckButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app) return;
    app->ptr_ignore_ascii_overlap = gtk_check_button_get_active(button);
    app_render_hex_dump(app);
}

static void on_search_addr_refs_toggled(GtkCheckButton *button, gpointer user_data) {
    App *app = user_data;
    if (!app || !app->listing_outer) return;
    ui_listing_set_addr_search_include_operand_refs(
        app->listing_outer,
        gtk_check_button_get_active(button));
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

static void on_about_dialog_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    App *app = user_data;
    if (!app) return;
    app->about_dialog = NULL;
}

static void on_about_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    App *app = user_data;
    if (!app || !app->window) return;

    if (app->about_dialog) {
        gtk_window_present(GTK_WINDOW(app->about_dialog));
        return;
    }

    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "The Z80 Workbench");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), Z80BENCH_VERSION_LABEL);
    gtk_about_dialog_set_comments(
        GTK_ABOUT_DIALOG(dialog),
        "z80bench\nGTK4 Z80 ROM annotation and export workbench.");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_MIT_X11);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), Z80BENCH_GITHUB_URL);
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "GitHub Repository");
    gtk_about_dialog_set_copyright(
        GTK_ABOUT_DIALOG(dialog),
        "Copyright (c) 2026 Dave Collins");
    gtk_window_set_title(GTK_WINDOW(dialog), "About z80bench");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(app->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    g_signal_connect(dialog, "destroy", G_CALLBACK(on_about_dialog_destroy), app);
    app->about_dialog = dialog;
    gtk_window_present(GTK_WINDOW(dialog));
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
    gtk_window_set_title(GTK_WINDOW(app->window), Z80BENCH_APP_NAME " " Z80BENCH_VERSION_LABEL);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1180, 900);
    gtk_window_maximize(GTK_WINDOW(app->window));
    g_signal_connect(app->window, "notify::width",
                     G_CALLBACK(on_window_size_changed), app);
    g_signal_connect(app->window, "notify::height",
                     G_CALLBACK(on_window_size_changed), app);

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

    GtkWidget *btn_recent = gtk_button_new_with_label("Open Recent");
    g_signal_connect(btn_recent, "clicked", G_CALLBACK(on_open_recent_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_recent);

    GtkWidget *btn_new = gtk_button_new_with_label("New Project");
    g_signal_connect(btn_new, "clicked", G_CALLBACK(on_new_project_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_new);

    GtkWidget *btn_save = gtk_button_new_with_label("Save");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_save);

    GtkWidget *toolbar_spacer = gtk_label_new("");
    gtk_widget_set_hexpand(toolbar_spacer, TRUE);
    gtk_box_append(GTK_BOX(toolbar), toolbar_spacer);

    app->auto_open_check = gtk_check_button_new_with_label("Auto-open last");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(app->auto_open_check), app->auto_open_last);
    g_signal_connect(app->auto_open_check, "toggled",
                     G_CALLBACK(on_auto_open_toggled), app);
    gtk_box_append(GTK_BOX(toolbar), app->auto_open_check);

    GtkWidget *btn_lst = gtk_button_new_with_label("Export .LST");
    g_signal_connect(btn_lst, "clicked", G_CALLBACK(on_export_lst_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_lst);

    GtkWidget *btn_asm = gtk_button_new_with_label("Export .ASM");
    g_signal_connect(btn_asm, "clicked", G_CALLBACK(on_export_asm_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_asm);

    GtkWidget *btn_about = gtk_button_new_with_label("About");
    g_signal_connect(btn_about, "clicked", G_CALLBACK(on_about_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_about);

    /* Main content area (listing + side panels) */
    app->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    app->pending_listing_addr = -1;
    app->pending_listing_retries = 0;
    gtk_paned_set_position(GTK_PANED(app->paned), 0);
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
    gtk_paned_set_position(
        GTK_PANED(app->main_vpaned),
        (app->saved_main_vsplit > 0) ? app->saved_main_vsplit : 375);
    gtk_paned_set_resize_start_child(GTK_PANED(app->main_vpaned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(app->main_vpaned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(app->main_vpaned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(app->main_vpaned), TRUE);
    gtk_widget_set_vexpand(app->main_vpaned, TRUE);
    g_signal_connect(app->main_vpaned, "notify::position",
                     G_CALLBACK(on_main_paned_position_changed), app);

    gtk_paned_set_start_child(GTK_PANED(app->main_vpaned), app->paned);
    gtk_paned_set_end_child(GTK_PANED(app->main_vpaned), build_bottom_dock(app));
    gtk_box_append(GTK_BOX(main_box), app->main_vpaned);

    app_render_hex_dump(app);

    gtk_window_present(GTK_WINDOW(app->window));

    if (app->auto_open_last && app->last_project_path[0] &&
        g_file_test(app->last_project_path, G_FILE_TEST_IS_DIR)) {
        app_open_project(app, app->last_project_path, FALSE);
    }
}

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
    App app;
    memset(&app, 0, sizeof(App));
    app.ui_busy = FALSE;
    app.highlight_ascii_runs = TRUE;
    app.highlight_highbit_runs = FALSE;
    app.highlight_ptr_in_rom = FALSE;
    app.highlight_ptr_out_rom = FALSE;
    app.ptr_ignore_ascii_overlap = FALSE;
    app_load_persistence(&app);

    GtkApplication *gtk_app =
        gtk_application_new("io.github.z80bench", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), &app);
    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);

    app_save_config(&app);
    app_save_state(&app);
    project_close(&app.project);
    g_object_unref(gtk_app);
    return status;
}
