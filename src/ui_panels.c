#include <gtk/gtk.h>
#include "z80bench.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* ==========================================================================
 * Type/colour tables
 * ========================================================================== */

static const char * const map_type_names[] =
    { "ROM", "RAM", "VRAM", "IO", "SYSVARS", "UNMAPPED",
      "Direct Byte Range", "Define Message Range", NULL };
static const MapType map_type_values[] =
    { MAP_ROM, MAP_RAM, MAP_VRAM, MAP_IO, MAP_SYSVARS, MAP_UNMAPPED,
      MAP_DIRECT_BYTE, MAP_DEFINE_MSG };

static const char * const sym_type_names[] =
    { "ROM_CALL", "VECTOR", "JUMP_LABEL", "WRITABLE", "PORT", "CONSTANT", NULL };
static const SymbolType sym_type_values[] =
    { SYM_ROM_CALL, SYM_VECTOR, SYM_JUMP_LABEL, SYM_WRITABLE, SYM_PORT, SYM_CONSTANT };

static const char *map_type_colour(MapType t) {
    switch (t) {
        case MAP_ROM:         return "#85B7EB";
        case MAP_RAM:         return "#639922";
        case MAP_VRAM:        return "#D485EB";
        case MAP_IO:          return "#EF9F27";
        case MAP_SYSVARS:     return "#5DCAA5";
        case MAP_DIRECT_BYTE: return "#A0804A";
        case MAP_DEFINE_MSG:  return "#5DCAA5";
        default:              return "#888";
    }
}

static const char *sym_type_colour(SymbolType t) {
    switch (t) {
        case SYM_ROM_CALL:   return "#F0997B";
        case SYM_VECTOR:     return "#EF9F27";
        case SYM_JUMP_LABEL: return "#639922";
        case SYM_WRITABLE:   return "#85B7EB";
        case SYM_PORT:       return "#D485EB";
        default:             return "#888";
    }
}

/* ==========================================================================
 * Shared helpers
 * ========================================================================== */

static GtkWidget *section_header_new(const char *title) {
    GtkWidget *lbl = gtk_label_new(NULL);
    char *m = g_markup_printf_escaped(
        "<span size='small' color='#888'><b>%s</b></span>", title);
    gtk_label_set_markup(GTK_LABEL(lbl), m);
    g_free(m);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    return lbl;
}

static GtkWidget *panel_header_row_new(const char *title, const char *btn_label,
                                       GCallback cb, gpointer cb_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_top(row, 8);
    gtk_widget_set_margin_bottom(row, 4);

    gtk_box_append(GTK_BOX(row), section_header_new(title));

    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(row), spacer);

    GtkWidget *btn = gtk_button_new_with_label(btn_label);
    gtk_widget_add_css_class(btn, "flat");
    if (cb) g_signal_connect(btn, "clicked", cb, cb_data);
    gtk_box_append(GTK_BOX(row), btn);

    return row;
}

/* ==========================================================================
 * Segments panel (unchanged from before)
 * ========================================================================== */

typedef struct {
    Project   *project;
    GtkWidget *window;
    GtkWidget *listing_outer;
    GtkWidget *panel_list;
    void     (*reload_cb)(gpointer);
    gpointer   reload_data;
    void     (*jump_cb)(gpointer, int);
    gpointer   jump_data;
} MapPanelCtx;

typedef struct {
    Project      *project;
    GtkWidget    *panel_list;
    GtkWidget    *dialog;
    GtkWidget    *dialog_list;
    GtkWidget    *name_entry;
    GtkWidget    *start_entry;
    GtkWidget    *end_entry;
    GtkDropDown  *type_drop;
    GtkWidget    *notes_entry;
    GtkWidget    *update_btn;
    GtkWidget    *save_btn;
    GtkWidget    *close_btn;
    void        (*reload_cb)(gpointer);
    gpointer      reload_data;
    gboolean      needs_reload;  /* set when a DIRECT_BYTE/DEFINE_MSG is added/removed */
    int           selected_index;
    int           prefill_start;
    int           prefill_end;
    gboolean      have_prefill;
    char          prefill_name[DISASM_LABEL_MAX];
    gboolean      have_name;
    char          prefill_notes[DISASM_COMMENT_MAX];
    gboolean      have_notes;
    MapType       prefill_type;
    gboolean      have_type;
    int           pending_jump_addr;
    gboolean      have_pending_jump;
    void        (*jump_cb)(gpointer, int);
    gpointer      jump_data;
    int           focus_index;
    gboolean      updating_form;
} MapEditCtx;

static void open_segment_dialog(MapPanelCtx *pctx, int start_offset, int end_offset,
                                const char *name, const char *notes,
                                gboolean set_type, MapType type);
static gboolean map_find_exact_segment(MapPanelCtx *pctx, int start_offset,
                                       int end_offset, int *entry_index);
static void map_dialog_set_entry(MapEditCtx *ctx, const MapEntry *e, int idx);
static gboolean map_entry_from_form(MapEditCtx *ctx, MapEntry *e);
static void segment_refresh_auto_name(MapEditCtx *ctx, gboolean force);
static void on_segment_form_changed(GtkEditable *editable, gpointer ud);

typedef struct {
    MapEditCtx *ctx;
    int         entry_index;
} MapRemoveConfirmCtx;

static gboolean map_entry_is_segment(MapType t) {
    return t == MAP_DIRECT_BYTE || t == MAP_DEFINE_MSG;
}

static gboolean map_entries_overlap(const MapEntry *a, const MapEntry *b) {
    return a->start <= b->end && b->start <= a->end;
}

static void segment_dialog_apply_prefill(MapEditCtx *ctx) {
    if (!ctx->have_prefill) return;
    ctx->updating_form = TRUE;
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%04X", ctx->prefill_start);
    gtk_editable_set_text(GTK_EDITABLE(ctx->start_entry), buf);
    snprintf(buf, sizeof(buf), "0x%04X", ctx->prefill_end);
    gtk_editable_set_text(GTK_EDITABLE(ctx->end_entry), buf);
    if (ctx->have_name)
        gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), ctx->prefill_name);
    if (ctx->have_notes)
        gtk_editable_set_text(GTK_EDITABLE(ctx->notes_entry), ctx->prefill_notes);
    if (ctx->have_type)
        gtk_drop_down_set_selected(ctx->type_drop, ctx->prefill_type);
    ctx->updating_form = FALSE;
}

static void segment_dialog_present(MapEditCtx *ctx) {
    gtk_window_present(GTK_WINDOW(ctx->dialog));
}

static void map_list_populate(GtkWidget *list, Project *p) {
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(list)))
        gtk_list_box_remove(GTK_LIST_BOX(list), c);

    int n = memmap_count(p->map);
    if (n == 0) {
        GtkWidget *lbl = gtk_label_new("No segments");
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        gtk_list_box_append(GTK_LIST_BOX(list), lbl);
        return;
    }
    for (int i = 0; i < n; i++) {
        const MapEntry *e = memmap_get(p->map, i);
        GtkWidget *rb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(rb, 6); gtk_widget_set_margin_end(rb, 6);
        gtk_widget_set_margin_top(rb, 2);   gtk_widget_set_margin_bottom(rb, 2);

        GtkWidget *nl = gtk_label_new(NULL);
        char *m = g_markup_printf_escaped(
            "<span color='%s' face='monospace'>%s</span>",
            map_type_colour(e->type), e->name);
        gtk_label_set_markup(GTK_LABEL(nl), m); g_free(m);
        gtk_label_set_xalign(GTK_LABEL(nl), 0.0);
        gtk_widget_set_hexpand(nl, TRUE);
        gtk_box_append(GTK_BOX(rb), nl);

        char range[24];
        snprintf(range, sizeof(range), "%04X-%04X", e->start, e->end);
        GtkWidget *rl = gtk_label_new(NULL);
        char *rm = g_markup_printf_escaped(
            "<span color='#888' face='monospace'>%s</span>", range);
        gtk_label_set_markup(GTK_LABEL(rl), rm); g_free(rm);
        gtk_box_append(GTK_BOX(rb), rl);

        GtkWidget *tl = gtk_label_new(NULL);
        char *tm = g_markup_printf_escaped(
            "<span color='%s' size='small'>%s</span>",
            map_type_colour(e->type), map_type_names[e->type]);
        gtk_label_set_markup(GTK_LABEL(tl), tm); g_free(tm);
        gtk_box_append(GTK_BOX(rb), tl);

        GtkListBoxRow *lbr = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
        gtk_list_box_row_set_child(lbr, rb);
        g_object_set_data(G_OBJECT(lbr), "entry-index", GINT_TO_POINTER(i));
        gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(lbr));
    }
}

static void map_dialog_populate(MapEditCtx *ctx) {
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(ctx->dialog_list)))
        gtk_list_box_remove(GTK_LIST_BOX(ctx->dialog_list), c);

    int n = memmap_count(ctx->project->map);
    if (n == 0) {
        gtk_list_box_append(GTK_LIST_BOX(ctx->dialog_list), gtk_label_new("No segments"));
        return;
    }
    for (int i = 0; i < n; i++) {
        const MapEntry *e = memmap_get(ctx->project->map, i);
        char text[96];
        snprintf(text, sizeof(text), "%-14s  %04X-%04X  %s",
                 e->name, e->start, e->end, map_type_names[e->type]);
        GtkWidget *lbl = gtk_label_new(NULL);
        char *m = g_markup_printf_escaped("<span face='monospace'>%s</span>", text);
        gtk_label_set_markup(GTK_LABEL(lbl), m); g_free(m);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
        gtk_widget_set_margin_start(lbl, 6);
        gtk_widget_set_margin_top(lbl, 2); gtk_widget_set_margin_bottom(lbl, 2);

        GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
        gtk_list_box_row_set_child(row, lbl);
        g_object_set_data(G_OBJECT(row), "entry-index", GINT_TO_POINTER(i));
        gtk_list_box_append(GTK_LIST_BOX(ctx->dialog_list), GTK_WIDGET(row));
    }
}

static gboolean map_find_exact_segment(MapPanelCtx *pctx, int start_offset,
                                       int end_offset, int *entry_index) {
    if (!pctx || !pctx->project || !pctx->project->map) return FALSE;
    for (int i = 0; i < memmap_count(pctx->project->map); i++) {
        const MapEntry *cur = memmap_get(pctx->project->map, i);
        if (!cur || !map_entry_is_segment(cur->type)) continue;
        if (cur->start == start_offset && cur->end == end_offset) {
            if (entry_index) *entry_index = i;
            return TRUE;
        }
    }
    return FALSE;
}

static void map_dialog_set_entry(MapEditCtx *ctx, const MapEntry *e, int idx) {
    if (!ctx || !e) return;
    ctx->updating_form = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), e->name);
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%04X", e->start);
    gtk_editable_set_text(GTK_EDITABLE(ctx->start_entry), buf);
    snprintf(buf, sizeof(buf), "0x%04X", e->end);
    gtk_editable_set_text(GTK_EDITABLE(ctx->end_entry), buf);
    gtk_drop_down_set_selected(ctx->type_drop, e->type);
    gtk_editable_set_text(GTK_EDITABLE(ctx->notes_entry), e->notes);
    ctx->updating_form = FALSE;
    ctx->selected_index = idx;
    if (ctx->update_btn)
        gtk_widget_set_sensitive(ctx->update_btn, TRUE);
}

static gboolean map_entry_from_form(MapEditCtx *ctx, MapEntry *e) {
    if (!ctx || !e) return FALSE;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry));
    if (!name[0]) return FALSE;

    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, DISASM_LABEL_MAX - 1);
    e->start = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->start_entry)), NULL, 0);
    e->end   = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->end_entry)),   NULL, 0);
    e->type  = map_type_values[gtk_drop_down_get_selected(ctx->type_drop)];
    strncpy(e->notes, gtk_editable_get_text(GTK_EDITABLE(ctx->notes_entry)),
            DISASM_COMMENT_MAX - 1);
    return TRUE;
}

static void segment_build_auto_name(MapEditCtx *ctx, char *buf, size_t buf_sz) {
    buf[0] = '\0';
    if (!ctx) return;

    MapType type = map_type_values[gtk_drop_down_get_selected(ctx->type_drop)];
    int start = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->start_entry)), NULL, 0);
    int end   = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->end_entry)),   NULL, 0);

    if (type == MAP_DIRECT_BYTE) {
        if (start == end)
            snprintf(buf, buf_sz, "direct bytes @ 0x%04X", start);
        else
            snprintf(buf, buf_sz, "direct bytes @ 0x%04X-0x%04X", start, end);
    } else if (type == MAP_DEFINE_MSG) {
        snprintf(buf, buf_sz, "message @ 0x%04X", start);
    }
}

static void segment_refresh_auto_name(MapEditCtx *ctx, gboolean force) {
    if (!ctx || ctx->updating_form) return;

    char generated[DISASM_LABEL_MAX];
    segment_build_auto_name(ctx, generated, sizeof(generated));
    if (!generated[0]) return;

    const char *cur = gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry));
    if (!cur) cur = "";
    if (!force && cur[0])
        return;

    ctx->updating_form = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), generated);
    ctx->updating_form = FALSE;
}

static void on_segment_form_changed(GtkEditable *editable, gpointer ud) {
    (void)editable;
    segment_refresh_auto_name((MapEditCtx *)ud, TRUE);
}

static void on_segment_type_changed(GObject *obj, GParamSpec *pspec, gpointer ud) {
    (void)obj;
    (void)pspec;
    segment_refresh_auto_name((MapEditCtx *)ud, TRUE);
}

static void on_map_save(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    gboolean needs = ctx->needs_reload;
    void (*cb)(gpointer) = ctx->reload_cb;
    gpointer data = ctx->reload_data;
    if (ctx->have_pending_jump) {
        void (*jump_cb)(gpointer, int) = ctx->jump_cb;
        gpointer jump_data = ctx->jump_data;
        int jump_addr = ctx->pending_jump_addr;
        if (jump_cb)
            jump_cb(jump_data, jump_addr);
    }
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    g_free(ctx);
    /* Fire reload AFTER dialog is destroyed */
    if (needs && cb) cb(data);
}

static void on_map_close(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    g_free(ctx);
}

static gboolean on_map_dialog_close_request(GtkWindow *window, gpointer ud) {
    (void)window;
    MapEditCtx *ctx = ud;
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    g_free(ctx);
    return TRUE;
}

static void on_map_dialog_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer ud) {
    (void)box;
    MapEditCtx *ctx = ud;
    if (!row) {
        ctx->selected_index = -1;
        if (ctx->update_btn)
            gtk_widget_set_sensitive(ctx->update_btn, FALSE);
        return;
    }
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    const MapEntry *e = memmap_get(ctx->project->map, idx);
    if (!e) return;
    map_dialog_set_entry(ctx, e, idx);
}

static gboolean map_entry_overlaps_other_segment(MapEditCtx *ctx, const MapEntry *e,
                                                 int skip_index, int *exact_idx) {
    if (exact_idx) *exact_idx = -1;
    if (!ctx || !ctx->project || !ctx->project->map || !e) return FALSE;

    for (int i = 0; i < memmap_count(ctx->project->map); i++) {
        const MapEntry *cur = memmap_get(ctx->project->map, i);
        if (!cur || !map_entry_is_segment(cur->type) || i == skip_index) continue;
        if (cur->start == e->start && cur->end == e->end) {
            if (exact_idx) *exact_idx = i;
            continue;
        }
        if (map_entries_overlap(e, cur))
            return TRUE;
    }
    return FALSE;
}

static void on_map_update(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    if (ctx->selected_index < 0) return;
    segment_refresh_auto_name(ctx, TRUE);

    MapEntry e;
    if (!map_entry_from_form(ctx, &e)) return;

    int exact_idx = -1;
    if (map_entry_overlaps_other_segment(ctx, &e, ctx->selected_index, &exact_idx)) {
        GtkAlertDialog *alert = gtk_alert_dialog_new(
            "Segments cannot overlap");
        gtk_alert_dialog_set_detail(
            alert,
            "Direct-byte and define-message segments can sit inside a broader code "
            "range, but they cannot overlap another segment. Overwrite the existing "
            "range instead.");
        gtk_alert_dialog_show(alert, GTK_WINDOW(ctx->dialog));
        g_object_unref(alert);
        return;
    }

    if (exact_idx >= 0 && exact_idx != ctx->selected_index) {
        memmap_remove(ctx->project->map, exact_idx);
        if (exact_idx < ctx->selected_index)
            ctx->selected_index--;
    }

    memmap_replace(ctx->project->map, ctx->selected_index, &e);
    ctx->needs_reload = TRUE;
    ctx->pending_jump_addr = e.start;
    ctx->have_pending_jump = TRUE;
    map_list_populate(ctx->panel_list, ctx->project);
    map_dialog_populate(ctx);
    if (ctx->dialog_list) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(ctx->dialog_list),
                                                           ctx->selected_index);
        if (row)
            gtk_list_box_select_row(GTK_LIST_BOX(ctx->dialog_list), row);
    }
}

static void on_map_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer ud) {
    (void)box;
    MapPanelCtx *ctx = ud;
    if (!row || !ctx->listing_outer) return;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    const MapEntry *e = memmap_get(ctx->project->map, idx);
    if (!e) return;
    ui_listing_select_address(ctx->listing_outer, e->start);
}

static void on_map_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer ud) {
    (void)box;
    MapPanelCtx *ctx = ud;
    if (!row) return;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    const MapEntry *e = memmap_get(ctx->project->map, idx);
    if (!e) return;
    open_segment_dialog(ctx, e->start, e->end, e->name, e->notes, TRUE, e->type);
}

static void on_map_remove_confirmed(GObject *source, GAsyncResult *res, gpointer user_data) {
    MapRemoveConfirmCtx *mctx = user_data;
    GError *error = NULL;
    int response = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), res, &error);
    if (error) {
        g_error_free(error);
        g_free(mctx);
        return;
    }

    if (response == 1) {
        MapEditCtx *ctx = mctx->ctx;
        memmap_remove(ctx->project->map, mctx->entry_index);
        ctx->needs_reload = TRUE;
        map_dialog_populate(ctx);
    }

    g_free(mctx);
}

static void on_map_remove(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(ctx->dialog_list));
    if (!row) return;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    const MapEntry *e = memmap_get(ctx->project->map, idx);
    if (e && map_entry_is_segment(e->type)) {
        GtkAlertDialog *alert = gtk_alert_dialog_new(
            "Remove this segment?");
        gtk_alert_dialog_set_detail(
            alert,
            "Removing a direct-byte or define-message segment will not rebuild "
            "the section workflow. If you need a different range, overwrite "
            "this range with a new segment instead.");
        const char *buttons[] = { "Cancel", "Remove", NULL };
        gtk_alert_dialog_set_buttons(alert, buttons);
        gtk_alert_dialog_set_cancel_button(alert, 0);
        gtk_alert_dialog_set_default_button(alert, 1);

        MapRemoveConfirmCtx *mctx = g_new0(MapRemoveConfirmCtx, 1);
        mctx->ctx = ctx;
        mctx->entry_index = idx;
        gtk_alert_dialog_choose(alert, GTK_WINDOW(ctx->dialog), NULL,
                                on_map_remove_confirmed, mctx);
        g_object_unref(alert);
        return;
    }

    memmap_remove(ctx->project->map, idx);
    ctx->needs_reload = TRUE;
    map_dialog_populate(ctx);
}

static void on_map_add(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    if (!ctx->project->map) {
        ctx->project->map = memmap_new();
        if (!ctx->project->map) return;
    }
    segment_refresh_auto_name(ctx, TRUE);
    const char *name = gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry));
    if (!name[0]) return;

    MapEntry e = {0};
    strncpy(e.name, name, DISASM_LABEL_MAX - 1);
    e.start = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->start_entry)), NULL, 0);
    e.end   = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->end_entry)),   NULL, 0);
    e.type  = map_type_values[gtk_drop_down_get_selected(ctx->type_drop)];
    strncpy(e.notes, gtk_editable_get_text(GTK_EDITABLE(ctx->notes_entry)),
            DISASM_COMMENT_MAX - 1);

    if (map_entry_is_segment(e.type)) {
        for (int i = 0; i < memmap_count(ctx->project->map); i++) {
            const MapEntry *cur = memmap_get(ctx->project->map, i);
            if (!cur || !map_entry_is_segment(cur->type)) continue;
            if (map_entries_overlap(&e, cur)) {
                GtkAlertDialog *alert = gtk_alert_dialog_new(
                    "Segments cannot overlap");
                gtk_alert_dialog_set_detail(
                    alert,
                    "Direct-byte and define-message segments can sit inside a "
                    "broader code range, but they cannot overlap another segment. "
                    "Choose a different range or edit the existing segment.");
                gtk_alert_dialog_show(alert, GTK_WINDOW(ctx->dialog));
                g_object_unref(alert);
                return;
            }
        }
    }

    int new_index = memmap_count(ctx->project->map);
    if (memmap_add(ctx->project->map, &e) != 0)
        return;
    ctx->needs_reload = TRUE;
    if (map_entry_is_segment(e.type)) {
        ctx->pending_jump_addr = e.start;
        ctx->have_pending_jump = TRUE;
    }
    map_dialog_populate(ctx);
    GtkListBoxRow *new_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(ctx->dialog_list),
                                                           new_index);
    if (new_row)
        gtk_list_box_select_row(GTK_LIST_BOX(ctx->dialog_list), new_row);
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry),  "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->start_entry), "0x0000");
    gtk_editable_set_text(GTK_EDITABLE(ctx->end_entry),   "0x0000");
    gtk_editable_set_text(GTK_EDITABLE(ctx->notes_entry), "");
}

static MapEditCtx *segment_dialog_create(MapPanelCtx *pctx) {
    MapEditCtx *ctx = g_new0(MapEditCtx, 1);
    ctx->project     = pctx->project;
    ctx->panel_list  = pctx->panel_list;
    ctx->reload_cb   = pctx->reload_cb;
    ctx->reload_data = pctx->reload_data;
    ctx->jump_cb     = pctx->jump_cb;
    ctx->jump_data   = pctx->jump_data;
    ctx->selected_index = -1;
    ctx->focus_index = -1;
    ctx->updating_form = FALSE;

    GtkWidget *dlg = gtk_window_new();
    ctx->dialog = dlg;
    gtk_window_set_title(GTK_WINDOW(dlg), "Edit Segments");
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(pctx->window));
    gtk_window_set_default_size(GTK_WINDOW(dlg), 520, 420);
    g_signal_connect(dlg, "close-request", G_CALLBACK(on_map_dialog_close_request), ctx);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(outer, 12); gtk_widget_set_margin_bottom(outer, 12);
    gtk_widget_set_margin_start(outer, 12); gtk_widget_set_margin_end(outer, 12);
    gtk_window_set_child(GTK_WINDOW(dlg), outer);

    ctx->dialog_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(ctx->dialog_list), GTK_SELECTION_SINGLE);
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(sw, TRUE); gtk_widget_set_size_request(sw, -1, 140);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), ctx->dialog_list);
    gtk_box_append(GTK_BOX(outer), sw);
    map_dialog_populate(ctx);
    g_signal_connect(ctx->dialog_list, "row-selected",
                     G_CALLBACK(on_map_dialog_row_selected), ctx);

    GtkWidget *remove_btn = gtk_button_new_with_label("Remove Selected");
    g_signal_connect(remove_btn, "clicked", G_CALLBACK(on_map_remove), ctx);
    gtk_box_append(GTK_BOX(outer), remove_btn);
    gtk_box_append(GTK_BOX(outer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget *add_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(add_lbl), "<b>Add segment</b>");
    gtk_label_set_xalign(GTK_LABEL(add_lbl), 0.0);
    gtk_box_append(GTK_BOX(outer), add_lbl);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_box_append(GTK_BOX(outer), grid);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name"),  0, 0, 1, 1);
    ctx->name_entry = gtk_entry_new();
    gtk_widget_set_hexpand(ctx->name_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ctx->name_entry, 1, 0, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Start"), 0, 1, 1, 1);
    ctx->start_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->start_entry), "0x0000");
    gtk_grid_attach(GTK_GRID(grid), ctx->start_entry, 1, 1, 1, 1);
    g_signal_connect(ctx->start_entry, "changed", G_CALLBACK(on_segment_form_changed), ctx);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("End"),   0, 2, 1, 1);
    ctx->end_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->end_entry), "0x0000");
    gtk_grid_attach(GTK_GRID(grid), ctx->end_entry,   1, 2, 1, 1);
    g_signal_connect(ctx->end_entry, "changed", G_CALLBACK(on_segment_form_changed), ctx);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Type"),  0, 3, 1, 1);
    ctx->type_drop = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(map_type_names));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->type_drop), 1, 3, 1, 1);
    g_signal_connect(ctx->type_drop, "notify::selected",
                     G_CALLBACK(on_segment_type_changed), ctx);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Notes"), 0, 4, 1, 1);
    ctx->notes_entry = gtk_entry_new();
    gtk_widget_set_hexpand(ctx->notes_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ctx->notes_entry, 1, 4, 2, 1);

    GtkWidget *add_btn = gtk_button_new_with_label("Add");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_map_add), ctx);
    gtk_box_append(GTK_BOX(outer), add_btn);

    ctx->update_btn = gtk_button_new_with_label("Update");
    gtk_widget_set_sensitive(ctx->update_btn, FALSE);
    g_signal_connect(ctx->update_btn, "clicked", G_CALLBACK(on_map_update), ctx);
    gtk_box_append(GTK_BOX(outer), ctx->update_btn);

    gtk_box_append(GTK_BOX(outer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    ctx->save_btn = gtk_button_new_with_label("Save");
    g_signal_connect(ctx->save_btn, "clicked", G_CALLBACK(on_map_save), ctx);
    gtk_box_append(GTK_BOX(outer), ctx->save_btn);

    ctx->close_btn = gtk_button_new_with_label("Close");
    g_signal_connect(ctx->close_btn, "clicked", G_CALLBACK(on_map_close), ctx);
    gtk_box_append(GTK_BOX(outer), ctx->close_btn);

    return ctx;
}

static void open_segment_dialog(MapPanelCtx *pctx, int start_offset, int end_offset,
                                const char *name, const char *notes,
                                gboolean set_type, MapType type) {
    MapEditCtx *ctx = segment_dialog_create(pctx);
    ctx->prefill_start = start_offset;
    ctx->prefill_end = end_offset;
    ctx->have_prefill = (start_offset >= 0 && end_offset >= 0);
    if (name && name[0]) {
        strncpy(ctx->prefill_name, name, sizeof(ctx->prefill_name) - 1);
        ctx->have_name = TRUE;
    }
    if (notes && notes[0]) {
        strncpy(ctx->prefill_notes, notes, sizeof(ctx->prefill_notes) - 1);
        ctx->have_notes = TRUE;
    }
    ctx->prefill_type = type;
    ctx->have_type = set_type;

    if (start_offset >= 0 && end_offset >= 0)
        map_find_exact_segment(pctx, start_offset, end_offset, &ctx->focus_index);

    segment_dialog_apply_prefill(ctx);
    if ((!ctx->have_name || !ctx->prefill_name[0]) &&
        (type == MAP_DIRECT_BYTE || type == MAP_DEFINE_MSG))
        segment_refresh_auto_name(ctx, FALSE);
    if (ctx->focus_index >= 0) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(ctx->dialog_list),
                                                           ctx->focus_index);
        if (row)
            gtk_list_box_select_row(GTK_LIST_BOX(ctx->dialog_list), row);
    }
    segment_dialog_present(ctx);
}

static void on_map_edit_clicked(GtkButton *btn, gpointer ud) {
    MapPanelCtx *pctx = ud;
    open_segment_dialog(pctx, -1, -1, NULL, NULL, FALSE, MAP_DIRECT_BYTE);
}

static GtkWidget *ui_memmap_panel_new(Project *p, GtkWidget *window) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(box, 10); gtk_widget_set_margin_end(box, 10);

    MapPanelCtx *ctx = g_new0(MapPanelCtx, 1);
    ctx->project = p; ctx->window = window;

    GtkWidget *hdr = panel_header_row_new("SEGMENTS", "Edit",
                                          G_CALLBACK(on_map_edit_clicked), ctx);
    gtk_box_append(GTK_BOX(box), hdr);

    GtkWidget *list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    ctx->panel_list = list;
    g_signal_connect(list, "row-selected", G_CALLBACK(on_map_row_selected), ctx);
    g_signal_connect(list, "row-activated", G_CALLBACK(on_map_row_activated), ctx);

    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(sw, -1, 110);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), list);
    gtk_box_append(GTK_BOX(box), sw);

    map_list_populate(list, p);
    g_object_set_data_full(G_OBJECT(box), "map-ctx", ctx, (GDestroyNotify)g_free);
    return box;
}

/* ==========================================================================
 * Symbols panel (unchanged from before)
 * ========================================================================== */

typedef struct {
    Project   *project;
    GtkWidget *window;
    GtkWidget *listing_outer;
    GtkWidget *panel_list;
    GtkWidget *filter_entry;
} SymPanelCtx;

typedef struct {
    Project      *project;
    GtkWidget    *panel_list;
    GtkWidget    *dialog;
    GtkWidget    *dialog_list;
    GtkWidget    *name_entry;
    GtkWidget    *addr_entry;
    GtkDropDown  *type_drop;
    GtkWidget    *notes_entry;
} SymEditCtx;

static void sym_list_populate(GtkWidget *list, Project *p, const char *filter) {
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(list)))
        gtk_list_box_remove(GTK_LIST_BOX(list), c);

    int n = symbols_count(p->sym);
    int shown = 0;
    for (int i = 0; i < n; i++) {
        const Symbol *s = symbols_get(p->sym, i);
        if (filter && filter[0] && !strstr(s->name, filter)) continue;

        GtkWidget *rb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(rb, 6); gtk_widget_set_margin_end(rb, 6);
        gtk_widget_set_margin_top(rb, 2);   gtk_widget_set_margin_bottom(rb, 2);

        GtkWidget *nl = gtk_label_new(NULL);
        char *nm = g_markup_printf_escaped(
            "<span color='%s' face='monospace'>%s</span>",
            sym_type_colour(s->type), s->name);
        gtk_label_set_markup(GTK_LABEL(nl), nm); g_free(nm);
        gtk_label_set_xalign(GTK_LABEL(nl), 0.0);
        gtk_widget_set_hexpand(nl, TRUE);
        gtk_box_append(GTK_BOX(rb), nl);

        GtkWidget *al = gtk_label_new(NULL);
        char *am = g_markup_printf_escaped(
            "<span color='#888' face='monospace'>%04X</span>", s->addr);
        gtk_label_set_markup(GTK_LABEL(al), am); g_free(am);
        gtk_box_append(GTK_BOX(rb), al);

        GtkWidget *tl = gtk_label_new(NULL);
        char *tm = g_markup_printf_escaped(
            "<span color='%s' size='small'>%s</span>",
            sym_type_colour(s->type), sym_type_names[s->type]);
        gtk_label_set_markup(GTK_LABEL(tl), tm); g_free(tm);
        gtk_box_append(GTK_BOX(rb), tl);

        GtkListBoxRow *lbr = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
        gtk_list_box_row_set_child(lbr, rb);
        g_object_set_data(G_OBJECT(lbr), "entry-index", GINT_TO_POINTER(i));
        gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(lbr));
        shown++;
    }
    if (shown == 0) {
        GtkWidget *lbl = gtk_label_new(n == 0 ? "No symbols" : "No matches");
        gtk_widget_set_margin_top(lbl, 4);
        gtk_list_box_append(GTK_LIST_BOX(list), lbl);
    }
}

static void on_sym_filter_changed(GtkEditable *ed, gpointer ud) {
    SymPanelCtx *ctx = ud;
    sym_list_populate(ctx->panel_list, ctx->project, gtk_editable_get_text(ed));
}

static void sym_dialog_populate(SymEditCtx *ctx) {
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(ctx->dialog_list)))
        gtk_list_box_remove(GTK_LIST_BOX(ctx->dialog_list), c);

    int n = symbols_count(ctx->project->sym);
    if (n == 0) {
        gtk_list_box_append(GTK_LIST_BOX(ctx->dialog_list), gtk_label_new("No symbols"));
        return;
    }
    for (int i = 0; i < n; i++) {
        const Symbol *s = symbols_get(ctx->project->sym, i);
        char text[96];
        snprintf(text, sizeof(text), "%-16s  %04X  %s",
                 s->name, s->addr, sym_type_names[s->type]);
        GtkWidget *lbl = gtk_label_new(NULL);
        char *m = g_markup_printf_escaped("<span face='monospace'>%s</span>", text);
        gtk_label_set_markup(GTK_LABEL(lbl), m); g_free(m);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
        gtk_widget_set_margin_start(lbl, 6);
        gtk_widget_set_margin_top(lbl, 2); gtk_widget_set_margin_bottom(lbl, 2);

        GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
        gtk_list_box_row_set_child(row, lbl);
        g_object_set_data(G_OBJECT(row), "entry-index", GINT_TO_POINTER(i));
        gtk_list_box_append(GTK_LIST_BOX(ctx->dialog_list), GTK_WIDGET(row));
    }
}

static void on_sym_close(GtkButton *btn, gpointer ud) {
    SymEditCtx *ctx = ud;
    sym_list_populate(ctx->panel_list, ctx->project, NULL);
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    g_free(ctx);
}

static void on_sym_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer ud) {
    (void)box;
    SymPanelCtx *ctx = ud;
    if (!row || !ctx->listing_outer) return;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    const Symbol *s = symbols_get(ctx->project->sym, idx);
    if (!s) return;
    ui_listing_select_address(ctx->listing_outer, s->addr);
}

static void on_sym_remove(GtkButton *btn, gpointer ud) {
    SymEditCtx *ctx = ud;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(ctx->dialog_list));
    if (!row) return;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    symbols_remove(ctx->project->sym, idx);
    sym_dialog_populate(ctx);
}

static void on_sym_add(GtkButton *btn, gpointer ud) {
    SymEditCtx *ctx = ud;
    if (!ctx->project->sym) {
        ctx->project->sym = symbols_new();
        if (!ctx->project->sym) return;
    }
    const char *name = gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry));
    if (!name[0]) return;

    Symbol s = {0};
    strncpy(s.name, name, DISASM_LABEL_MAX - 1);
    s.addr = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->addr_entry)), NULL, 0);
    s.type = sym_type_values[gtk_drop_down_get_selected(ctx->type_drop)];
    strncpy(s.notes, gtk_editable_get_text(GTK_EDITABLE(ctx->notes_entry)),
            DISASM_COMMENT_MAX - 1);

    symbols_add(ctx->project->sym, &s);
    sym_dialog_populate(ctx);
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->addr_entry), "0x0000");
    gtk_editable_set_text(GTK_EDITABLE(ctx->notes_entry), "");
}

static void on_sym_edit_clicked(GtkButton *btn, gpointer ud) {
    SymPanelCtx *pctx = ud;
    SymEditCtx *ctx = g_new0(SymEditCtx, 1);
    ctx->project    = pctx->project;
    ctx->panel_list = pctx->panel_list;

    GtkWidget *dlg = gtk_window_new();
    ctx->dialog = dlg;
    gtk_window_set_title(GTK_WINDOW(dlg), "Edit Symbols");
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(pctx->window));
    gtk_window_set_default_size(GTK_WINDOW(dlg), 520, 440);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(outer, 12); gtk_widget_set_margin_bottom(outer, 12);
    gtk_widget_set_margin_start(outer, 12); gtk_widget_set_margin_end(outer, 12);
    gtk_window_set_child(GTK_WINDOW(dlg), outer);

    ctx->dialog_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(ctx->dialog_list), GTK_SELECTION_SINGLE);
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(sw, TRUE); gtk_widget_set_size_request(sw, -1, 160);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), ctx->dialog_list);
    gtk_box_append(GTK_BOX(outer), sw);
    sym_dialog_populate(ctx);

    GtkWidget *remove_btn = gtk_button_new_with_label("Remove Selected");
    g_signal_connect(remove_btn, "clicked", G_CALLBACK(on_sym_remove), ctx);
    gtk_box_append(GTK_BOX(outer), remove_btn);
    gtk_box_append(GTK_BOX(outer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget *add_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(add_lbl), "<b>Add symbol</b>");
    gtk_label_set_xalign(GTK_LABEL(add_lbl), 0.0);
    gtk_box_append(GTK_BOX(outer), add_lbl);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_box_append(GTK_BOX(outer), grid);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name"),    0, 0, 1, 1);
    ctx->name_entry = gtk_entry_new();
    gtk_widget_set_hexpand(ctx->name_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ctx->name_entry, 1, 0, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Address"), 0, 1, 1, 1);
    ctx->addr_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->addr_entry), "0x0000");
    gtk_grid_attach(GTK_GRID(grid), ctx->addr_entry, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Type"),    0, 2, 1, 1);
    ctx->type_drop = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(sym_type_names));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->type_drop), 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Notes"),   0, 3, 1, 1);
    ctx->notes_entry = gtk_entry_new();
    gtk_widget_set_hexpand(ctx->notes_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ctx->notes_entry, 1, 3, 2, 1);

    GtkWidget *add_btn = gtk_button_new_with_label("Add");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_sym_add), ctx);
    gtk_box_append(GTK_BOX(outer), add_btn);
    gtk_box_append(GTK_BOX(outer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_sym_close), ctx);
    gtk_box_append(GTK_BOX(outer), close_btn);

    gtk_window_present(GTK_WINDOW(dlg));
}

static GtkWidget *ui_symbols_panel_new(Project *p, GtkWidget *window) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(box, 10); gtk_widget_set_margin_end(box, 10);

    SymPanelCtx *ctx = g_new0(SymPanelCtx, 1);
    ctx->project = p; ctx->window = window;

    GtkWidget *hdr = panel_header_row_new("SYMBOLS", "Edit",
                                          G_CALLBACK(on_sym_edit_clicked), ctx);
    gtk_box_append(GTK_BOX(box), hdr);

    GtkWidget *filter = gtk_search_entry_new();
    gtk_box_append(GTK_BOX(box), filter);

    GtkWidget *list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    ctx->panel_list   = list;
    ctx->filter_entry = filter;
    g_signal_connect(list, "row-selected", G_CALLBACK(on_sym_row_selected), ctx);

    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(sw, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), list);
    gtk_box_append(GTK_BOX(box), sw);

    sym_list_populate(list, p, NULL);
    g_signal_connect(filter, "changed", G_CALLBACK(on_sym_filter_changed), ctx);
    g_object_set_data_full(G_OBJECT(box), "sym-ctx", ctx, (GDestroyNotify)g_free);
    return box;
}

/* ==========================================================================
 * Annotation panel — with live write-back and auto-save
 * ========================================================================== */

typedef struct {
    Project    *project;
    GtkWidget  *listing_outer;
    GtkWidget  *panels;          /* parent panels box — for sym refresh */
    char        project_path[512];
    GtkWidget  *header;
    GtkWidget  *label_entry;
    GtkWidget  *comment_entry;
    GtkWidget  *block_view;
    GtkWidget  *xref_entry;
    DisasmLine *dl;
    int         line_index;
    int         updating;
} AnnotationPanel;

static void ann_save(AnnotationPanel *ap) {
    if (ap->project_path[0] && ap->project)
        project_save(ap->project, ap->project_path);
}

/* Sync label field to the symbol table, then refresh the symbols panel list */
static void sync_label_to_symbols(AnnotationPanel *ap) {
    if (!ap->dl || !ap->project) return;

    const char *new_label = ap->dl->label;
    int addr = ap->dl->addr;

    /* Ensure sym table exists */
    if (!ap->project->sym) {
        ap->project->sym = symbols_new();
        if (!ap->project->sym) return;
    }

    /* Find existing JUMP_LABEL entry for this address */
    int found = -1;
    for (int i = 0; i < symbols_count(ap->project->sym); i++) {
        const Symbol *s = symbols_get(ap->project->sym, i);
        if (s->addr == addr && s->type == SYM_JUMP_LABEL) {
            found = i;
            break;
        }
    }

    if (new_label[0]) {
        if (found >= 0) {
            /* Remove old, re-add with new name to avoid needing a mutable accessor */
            const Symbol *old = symbols_get(ap->project->sym, found);
            Symbol s = *old;
            strncpy(s.name, new_label, DISASM_LABEL_MAX - 1);
            symbols_remove(ap->project->sym, found);
            symbols_add(ap->project->sym, &s);
        } else {
            Symbol s = {0};
            strncpy(s.name, new_label, DISASM_LABEL_MAX - 1);
            s.addr = addr;
            s.type = SYM_JUMP_LABEL;
            symbols_add(ap->project->sym, &s);
        }
    } else if (found >= 0) {
        symbols_remove(ap->project->sym, found);
    }

    /* Refresh the symbols panel list */
    if (ap->panels)
        ui_panels_refresh_symbols(ap->panels);
}

static void on_label_changed(GtkEditable *ed, gpointer ud) {
    AnnotationPanel *ap = ud;
    if (ap->updating || !ap->dl) return;
    strncpy(ap->dl->label, gtk_editable_get_text(ed), DISASM_LABEL_MAX - 1);
    ui_listing_refresh_line(ap->listing_outer, ap->line_index);
    sync_label_to_symbols(ap);
}

static void on_comment_changed(GtkEditable *ed, gpointer ud) {
    AnnotationPanel *ap = ud;
    if (ap->updating || !ap->dl) return;
    strncpy(ap->dl->comment, gtk_editable_get_text(ed), DISASM_COMMENT_MAX - 1);
    ui_listing_refresh_line(ap->listing_outer, ap->line_index);
}

static void on_xref_changed(GtkEditable *ed, gpointer ud) {
    AnnotationPanel *ap = ud;
    if (ap->updating || !ap->dl) return;
    strncpy(ap->dl->xref, gtk_editable_get_text(ed), DISASM_XREF_MAX - 1);
}

static void on_block_changed(GtkTextBuffer *buf, gpointer ud) {
    AnnotationPanel *ap = ud;
    if (ap->updating || !ap->dl) return;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    strncpy(ap->dl->block, text, DISASM_COMMENT_MAX - 1);
    g_free(text);
    ui_listing_refresh_line(ap->listing_outer, ap->line_index);
}

/* Save on focus-out of any annotation field */
static gboolean on_ann_focus_out(GtkEventControllerFocus *ctrl,
                                  gpointer ud) {
    ann_save((AnnotationPanel *)ud);
    return FALSE;
}

static void attach_focus_out(GtkWidget *widget, AnnotationPanel *ap) {
    GtkEventController *fc = gtk_event_controller_focus_new();
    g_signal_connect(fc, "leave", G_CALLBACK(on_ann_focus_out), ap);
    gtk_widget_add_controller(widget, fc);
}

static GtkWidget *ui_annotation_panel_new(Project *p,
                                           const char *project_path,
                                           GtkWidget *listing_outer,
                                           AnnotationPanel **out_ap) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(box, 10); gtk_widget_set_margin_end(box, 10);

    AnnotationPanel *ap = g_new0(AnnotationPanel, 1);
    ap->project       = p;
    ap->listing_outer = listing_outer;
    ap->line_index    = -1;
    if (project_path)
        strncpy(ap->project_path, project_path, sizeof(ap->project_path) - 1);

    ap->header = section_header_new("ANNOTATION");
    gtk_widget_set_margin_top(ap->header, 8);
    gtk_widget_set_margin_bottom(ap->header, 4);
    gtk_box_append(GTK_BOX(box), ap->header);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_box_append(GTK_BOX(box), grid);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Label"),   0, 0, 1, 1);
    ap->label_entry = gtk_entry_new();
    gtk_widget_set_hexpand(ap->label_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ap->label_entry, 1, 0, 1, 1);
    g_signal_connect(ap->label_entry, "changed",
                     G_CALLBACK(on_label_changed), ap);
    attach_focus_out(ap->label_entry, ap);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Comment"), 0, 1, 1, 1);
    ap->comment_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), ap->comment_entry, 1, 1, 1, 1);
    g_signal_connect(ap->comment_entry, "changed",
                     G_CALLBACK(on_comment_changed), ap);
    attach_focus_out(ap->comment_entry, ap);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Block"),   0, 2, 1, 1);
    GtkWidget *s = gtk_scrolled_window_new();
    gtk_widget_set_size_request(s, -1, 80);
    ap->block_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ap->block_view), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s), ap->block_view);
    gtk_grid_attach(GTK_GRID(grid), s, 1, 2, 1, 1);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ap->block_view));
    g_signal_connect(buf, "changed", G_CALLBACK(on_block_changed), ap);
    attach_focus_out(ap->block_view, ap);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Xref"),    0, 3, 1, 1);
    ap->xref_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), ap->xref_entry, 1, 3, 1, 1);
    g_signal_connect(ap->xref_entry, "changed",
                     G_CALLBACK(on_xref_changed), ap);
    attach_focus_out(ap->xref_entry, ap);

    if (out_ap) *out_ap = ap;
    g_object_set_data_full(G_OBJECT(box), "ap", ap, (GDestroyNotify)g_free);
    return box;
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void ui_panels_update_selection(GtkWidget *panels, DisasmLine *dl,
                                 int line_index) {
    GtkWidget *ann_panel = gtk_widget_get_last_child(panels);
    AnnotationPanel *ap = g_object_get_data(G_OBJECT(ann_panel), "ap");
    if (!ap) return;

    ap->updating = 1;

    ap->dl         = dl;
    ap->line_index = line_index;

    char *hm = g_markup_printf_escaped(
        "<span size='small' color='#888'><b>ANNOTATION — 0x%04X</b></span>",
        dl->addr);
    gtk_label_set_markup(GTK_LABEL(ap->header), hm);
    g_free(hm);

    gtk_editable_set_text(GTK_EDITABLE(ap->label_entry),   dl->label);
    gtk_editable_set_text(GTK_EDITABLE(ap->comment_entry), dl->comment);
    gtk_editable_set_text(GTK_EDITABLE(ap->xref_entry),    dl->xref);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ap->block_view));
    gtk_text_buffer_set_text(buf, dl->block, -1);

    ap->updating = 0;
}

void ui_panels_refresh_symbols(GtkWidget *panels) {
    /* The symbols panel is the third child (after memmap, sep, symbols, sep, ann) */
    /* Walk children to find the one with a "sym-ctx" */
    GtkWidget *child = gtk_widget_get_first_child(panels);
    while (child) {
        SymPanelCtx *ctx = g_object_get_data(G_OBJECT(child), "sym-ctx");
        if (ctx) {
            sym_list_populate(ctx->panel_list, ctx->project, NULL);
            return;
        }
        child = gtk_widget_get_next_sibling(child);
    }
}

void ui_panels_set_reload_cb(GtkWidget *panels, void (*cb)(gpointer), gpointer data) {
    GtkWidget *child = gtk_widget_get_first_child(panels);
    while (child) {
        MapPanelCtx *ctx = g_object_get_data(G_OBJECT(child), "map-ctx");
        if (ctx) {
            ctx->reload_cb   = cb;
            ctx->reload_data = data;
            return;
        }
        child = gtk_widget_get_next_sibling(child);
    }
}

void ui_panels_set_jump_cb(GtkWidget *panels, void (*cb)(gpointer, int), gpointer data) {
    GtkWidget *child = gtk_widget_get_first_child(panels);
    while (child) {
        MapPanelCtx *ctx = g_object_get_data(G_OBJECT(child), "map-ctx");
        if (ctx) {
            ctx->jump_cb = cb;
            ctx->jump_data = data;
            return;
        }
        child = gtk_widget_get_next_sibling(child);
    }
}

void ui_panels_open_segment_editor(GtkWidget *panels, int start_offset, int end_offset) {
    ui_panels_open_segment_editor_prefill(panels, start_offset, end_offset, NULL,
                                          MAP_DIRECT_BYTE);
}

void ui_panels_open_segment_editor_prefill(GtkWidget *panels, int start_offset,
                                           int end_offset, const char *name,
                                           MapType type) {
    GtkWidget *child = gtk_widget_get_first_child(panels);
    while (child) {
        MapPanelCtx *ctx = g_object_get_data(G_OBJECT(child), "map-ctx");
        if (ctx) {
            open_segment_dialog(ctx, start_offset, end_offset, name, NULL, TRUE, type);
            return;
        }
        child = gtk_widget_get_next_sibling(child);
    }
}

void ui_panels_open_segment_editor_with_details(GtkWidget *panels, int start_offset,
                                                int end_offset, const char *name,
                                                const char *notes, MapType type) {
    GtkWidget *child = gtk_widget_get_first_child(panels);
    while (child) {
        MapPanelCtx *ctx = g_object_get_data(G_OBJECT(child), "map-ctx");
        if (ctx) {
            open_segment_dialog(ctx, start_offset, end_offset, name, notes, TRUE, type);
            return;
        }
        child = gtk_widget_get_next_sibling(child);
    }
}

GtkWidget *ui_panels_new(Project *p, GtkWidget *window,
                          const char *project_path,
                          GtkWidget *listing_outer) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), ui_memmap_panel_new(p, window));
    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(box), ui_symbols_panel_new(p, window));
    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(box),
                   ui_annotation_panel_new(p, project_path, listing_outer, NULL));

    GtkWidget *ann_panel = gtk_widget_get_last_child(box);
    AnnotationPanel *ap = g_object_get_data(G_OBJECT(ann_panel), "ap");
    if (ap) ap->panels = box;

    GtkWidget *child = gtk_widget_get_first_child(box);
    while (child) {
        MapPanelCtx *mctx = g_object_get_data(G_OBJECT(child), "map-ctx");
        if (mctx) mctx->listing_outer = listing_outer;
        SymPanelCtx *sctx = g_object_get_data(G_OBJECT(child), "sym-ctx");
        if (sctx) sctx->listing_outer = listing_outer;
        child = gtk_widget_get_next_sibling(child);
    }

    return box;
}
