#include <gtk/gtk.h>
#include <string.h>
#include "z80bench.h"

typedef struct {
    Project   project;
    GtkWidget *window;
    GtkWidget *paned;
    GtkWidget *listing_outer;
    GtkWidget *panels_outer;
    char       project_path[512];
    int        pending_listing_addr;
    int        pending_listing_retries;
} App;

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

static void reload_listing(gpointer user_data);  /* forward */
static void queue_listing_jump(gpointer user_data, int addr);  /* forward */
static gboolean apply_pending_listing_jump(gpointer user_data);  /* forward */

static void load_project_ui(App *app) {
    char title[512];
    snprintf(title, sizeof(title), "z80bench — %s", app->project.name);
    gtk_window_set_title(GTK_WINDOW(app->window), title);

    GtkWidget *listing = ui_listing_new(&app->project, NULL);
    GtkWidget *panels  = ui_panels_new(&app->project, app->window,
                                       app->project_path, listing);
    ui_listing_set_panels(listing, panels);
    ui_panels_set_reload_cb(panels, reload_listing, app);
    ui_panels_set_jump_cb(panels, queue_listing_jump, app);

    app->listing_outer = listing;
    app->panels_outer = panels;

    gtk_paned_set_start_child(GTK_PANED(app->paned), listing);
    gtk_paned_set_end_child(GTK_PANED(app->paned), panels);
}

/* Reload callback — fired when a Direct Byte / Define Message map range changes */
static void reload_listing(gpointer user_data) {
    App *app = user_data;
    if (!app->project.rom || !app->project_path[0]) return;

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
    load_project_ui(app);
    if (app->pending_listing_addr >= 0) {
        app->pending_listing_retries = 8;
        g_timeout_add(16, apply_pending_listing_jump, app);
    }
}

static void queue_listing_jump(gpointer user_data, int addr) {
    App *app = user_data;
    app->pending_listing_addr = addr;
    app->pending_listing_retries = 8;
}

static gboolean apply_pending_listing_jump(gpointer user_data) {
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
        load_project_ui(app);
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
        load_project_ui(app);
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

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "z80bench");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1024, 600);

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

    gtk_box_append(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    GtkWidget *search = gtk_search_entry_new();
    gtk_widget_set_hexpand(search, TRUE);
    gtk_box_append(GTK_BOX(toolbar), search);

    gtk_box_append(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    GtkWidget *btn_lst = gtk_button_new_with_label("Export .LST");
    g_signal_connect(btn_lst, "clicked", G_CALLBACK(on_export_lst_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_lst);

    GtkWidget *btn_asm = gtk_button_new_with_label("Export .ASM");
    g_signal_connect(btn_asm, "clicked", G_CALLBACK(on_export_asm_clicked), app);
    gtk_box_append(GTK_BOX(toolbar), btn_asm);

    /* Content area */
    app->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    app->pending_listing_addr = -1;
    app->pending_listing_retries = 0;
    gtk_paned_set_position(GTK_PANED(app->paned), 630);
    gtk_widget_set_vexpand(app->paned, TRUE);
    gtk_box_append(GTK_BOX(main_box), app->paned);

    gtk_paned_set_start_child(GTK_PANED(app->paned),
        gtk_label_new("Open a .bin ROM file to begin."));
    gtk_paned_set_end_child(GTK_PANED(app->paned), gtk_label_new(""));

    gtk_window_present(GTK_WINDOW(app->window));
}

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
    App app;
    memset(&app, 0, sizeof(App));

    GtkApplication *gtk_app =
        gtk_application_new("io.github.z80bench", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), &app);
    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);

    project_close(&app.project);
    g_object_unref(gtk_app);
    return status;
}
