#include <gtk/gtk.h>
#include "z80bench.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define PANEL_LOG(fmt, ...) \
    G_STMT_START { \
        if (g_getenv("Z80BENCH_GUI_DEBUG")) \
            g_printerr("[z80bench-panels] " fmt "\n", ##__VA_ARGS__); \
    } G_STMT_END

/* ==========================================================================
 * Type/colour tables
 * ========================================================================== */

static const char * const map_type_names[] =
    { "ROM", "RAM", "VRAM", "IO", "SYSVARS", "UNMAPPED",
      "Direct Byte Range", "Direct Word Range", "Define Message Range", NULL };
static const char * const map_type_edit_names[] =
    { "ROM", "RAM", "VRAM", "IO", "SYSVARS",
      "Direct Byte Range", "Direct Word Range", "Define Message Range", NULL };
static const MapType map_type_edit_values[] =
    { MAP_ROM, MAP_RAM, MAP_VRAM, MAP_IO, MAP_SYSVARS,
      MAP_DIRECT_BYTE, MAP_DIRECT_WORD, MAP_DEFINE_MSG };

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
        case MAP_DIRECT_WORD: return "#C49B59";
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

static const char *map_type_name(MapType t) {
    if (t < 0 || t >= MAP_DEFINE_MSG + 1)
        return "UNKNOWN";
    return map_type_names[t];
}

static const char *map_type_compact_name(MapType t) {
    switch (t) {
        case MAP_DIRECT_BYTE: return "DEFB";
        case MAP_DIRECT_WORD: return "DEFW";
        case MAP_DEFINE_MSG:  return "DEFM";
        default:              return map_type_name(t);
    }
}

static int map_edit_index_from_type(MapType t) {
    for (int i = 0; i < (int)G_N_ELEMENTS(map_type_edit_values); i++) {
        if (map_type_edit_values[i] == t)
            return i;
    }
    return 0;
}

static void apply_panel_list_style(GtkWidget *list) {
    static gboolean installed = FALSE;
    static GtkCssProvider *provider = NULL;
    if (!list) return;

    gtk_widget_add_css_class(list, "z80bench-panel-list");

    if (!installed) {
        provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(
            provider,
            ".z80bench-panel-list row,"
            ".z80bench-panel-list row:hover,"
            ".z80bench-panel-list row:selected,"
            ".z80bench-panel-list row:focus,"
            ".z80bench-panel-list row:focus-within {"
            "  background: transparent;"
            "  box-shadow: none;"
            "  outline: none;"
            "}"
        );
        gtk_style_context_add_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        installed = TRUE;
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
    GtkWidget *panels_root;
    GtkWidget *listing_outer;
    GtkWidget *panel_list;
    void     (*reload_cb)(gpointer);
    gpointer   reload_data;
    void     (*jump_cb)(gpointer, int);
    gpointer   jump_data;
    UISegmentCommandFn segment_cmd_cb;
    gpointer   segment_cmd_data;
    UISegmentSaveFn segment_save_cb;
    gpointer   segment_save_data;
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
    GtkWidget    *update_btn;
    GtkWidget    *close_btn;
    void        (*reload_cb)(gpointer);
    gpointer      reload_data;
    gboolean      needs_reload;  /* set when direct-data segments are added/removed */
    int           selected_index;
    int           prefill_start;
    int           prefill_end;
    gboolean      have_prefill;
    char          prefill_name[DISASM_LABEL_MAX];
    gboolean      have_name;
    MapType       prefill_type;
    gboolean      have_type;
    int           pending_jump_addr;
    gboolean      have_pending_jump;
    void        (*jump_cb)(gpointer, int);
    gpointer      jump_data;
    UISegmentCommandFn segment_cmd_cb;
    gpointer      segment_cmd_data;
    UISegmentSaveFn segment_save_cb;
    gpointer      segment_save_data;
    int           focus_index;
    gboolean      updating_form;
    gboolean      closing;
} MapEditCtx;

static void open_segment_dialog(MapPanelCtx *pctx, int start_offset, int end_offset,
                                const char *name,
                                gboolean set_type, MapType type);
static gboolean map_find_exact_segment(MapPanelCtx *pctx, int start_offset,
                                       int end_offset, int *entry_index);
static void map_dialog_set_entry(MapEditCtx *ctx, const MapEntry *e, int idx);
static gboolean map_entry_from_form(MapEditCtx *ctx, MapEntry *e);
static void segment_refresh_auto_name(MapEditCtx *ctx, gboolean force);
static void on_segment_form_changed(GtkEditable *editable, gpointer ud);
static void map_list_populate(MapPanelCtx *ctx);
static void map_dialog_populate(MapEditCtx *ctx);

static void map_show_form_error(MapEditCtx *ctx, const char *title, const char *detail) {
    GtkAlertDialog *alert = gtk_alert_dialog_new("%s", title);
    if (detail && detail[0])
        gtk_alert_dialog_set_detail(alert, detail);
    gtk_alert_dialog_show(alert, GTK_WINDOW(ctx->dialog));
    g_object_unref(alert);
}

static gboolean map_parse_int_field(MapEditCtx *ctx, GtkWidget *entry,
                                    const char *field_name, int *out) {
    const char *txt = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!txt || !txt[0]) {
        char detail[128];
        snprintf(detail, sizeof(detail), "%s is required.", field_name);
        map_show_form_error(ctx, "Invalid segment", detail);
        return FALSE;
    }
    char *end = NULL;
    long v = strtol(txt, &end, 0);
    if (end == txt || (end && *end != '\0') || v < 0 || v > 0xFFFF) {
        char detail[128];
        snprintf(detail, sizeof(detail), "%s must be a valid address (0x0000-0xFFFF).",
                 field_name);
        map_show_form_error(ctx, "Invalid segment", detail);
        return FALSE;
    }
    *out = (int)v;
    return TRUE;
}

static gboolean map_entry_is_segment(MapType t) {
    return t == MAP_DIRECT_BYTE || t == MAP_DIRECT_WORD || t == MAP_DEFINE_MSG;
}

static gboolean map_entry_is_parent(MapType t) {
    return t == MAP_ROM || t == MAP_RAM || t == MAP_VRAM ||
           t == MAP_IO || t == MAP_SYSVARS;
}

static gboolean map_project_span_bounds_for_project(const Project *p,
                                                    int *project_start, int *project_end) {
    if (!p) return FALSE;
    if (p->rom_size <= 0) return FALSE;

    int start = p->load_addr;
    int end = start + p->rom_size - 1;
    if (project_start) *project_start = start;
    if (project_end) *project_end = end;
    return TRUE;
}

static gboolean map_project_span_bounds(const MapEditCtx *ctx,
                                        int *project_start, int *project_end) {
    if (!ctx || !ctx->project) return FALSE;
    return map_project_span_bounds_for_project(ctx->project, project_start, project_end);
}

static gboolean map_entry_within_project_span(const MapEditCtx *ctx,
                                              const MapEntry *e) {
    int project_start = 0, project_end = 0;
    if (!map_project_span_bounds(ctx, &project_start, &project_end))
        return FALSE;
    return e && e->start >= project_start && e->end <= project_end;
}

static void map_ensure_default_rom_segment(Project *p) {
    if (!p) return;
    int span_start = 0, span_end = 0;
    if (!map_project_span_bounds_for_project(p, &span_start, &span_end))
        return;

    if (!p->map) {
        p->map = memmap_new();
        if (!p->map) return;
    }
    if (memmap_count(p->map) > 0)
        return;

    MapEntry e;
    memset(&e, 0, sizeof(e));
    strncpy(e.name, "ROM", sizeof(e.name) - 1);
    e.start = span_start;
    e.end = span_end;
    e.type = MAP_ROM;
    (void)memmap_add(p->map, &e);
}

static gboolean map_run_command(MapEditCtx *ctx, UISegmentCmdType type, int index,
                                const MapEntry *entry, int *resulting_index) {
    if (!ctx) return FALSE;

    if (ctx->segment_cmd_cb) {
        UISegmentCmd cmd;
        UISegmentCmdResult res;
        memset(&cmd, 0, sizeof(cmd));
        memset(&res, 0, sizeof(res));
        cmd.type = type;
        cmd.index = index;
        if (entry) cmd.entry = *entry;
        ctx->segment_cmd_cb(ctx->segment_cmd_data, &cmd, &res);
        if (!res.ok) {
            map_show_form_error(ctx, "Segment operation failed",
                                res.error[0] ? res.error : "Unknown segment controller error.");
            return FALSE;
        }
        if (resulting_index) *resulting_index = res.resulting_index;
        return TRUE;
    }

    if (type == UI_SEGMENT_CMD_ADD) {
        if (!ctx->project->map) {
            ctx->project->map = memmap_new();
            if (!ctx->project->map) return FALSE;
        }
        if (!entry || memmap_add(ctx->project->map, entry) != 0) return FALSE;
        if (resulting_index) *resulting_index = memmap_count(ctx->project->map) - 1;
        return TRUE;
    }
    if (type == UI_SEGMENT_CMD_UPDATE) {
        if (!entry || memmap_replace(ctx->project->map, index, entry) != 0) return FALSE;
        if (resulting_index) *resulting_index = index;
        return TRUE;
    }
    if (type == UI_SEGMENT_CMD_REMOVE) {
        if (memmap_remove(ctx->project->map, index) != 0) return FALSE;
        if (resulting_index) {
            int n = memmap_count(ctx->project->map);
            *resulting_index = (index < n) ? index : (n - 1);
        }
        return TRUE;
    }
    return FALSE;
}

static GtkListBoxRow *find_row_by_entry_index(GtkWidget *list, int entry_index) {
    GtkWidget *child = gtk_widget_get_first_child(list);
    while (child) {
        int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "entry-index"));
        if (idx == entry_index)
            return GTK_LIST_BOX_ROW(child);
        child = gtk_widget_get_next_sibling(child);
    }
    return NULL;
}

static int map_entry_cmp_by_addr(const void *a, const void *b, gpointer user_data) {
    Project *p = (Project *)user_data;
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    const MapEntry *ea = memmap_get(p->map, ia);
    const MapEntry *eb = memmap_get(p->map, ib);
    if (!ea || !eb) return ia - ib;
    if (ea->start != eb->start) return ea->start - eb->start;
    if (ea->end   != eb->end)   return ea->end   - eb->end;
    return ia - ib;
}

static gboolean map_entry_is_parent_like(MapType t) {
    return map_entry_is_parent(t);
}

static gboolean map_parent_contains_segment(const MapEntry *parent, const MapEntry *seg) {
    if (!parent || !seg) return FALSE;
    return parent->start <= seg->start && seg->end <= parent->end;
}

static int map_collect_sorted_indices(Project *p, int *parents, int *np,
                                      int *segs, int *ns, int cap) {
    if (!p || !p->map || cap <= 0) return 0;
    int n = memmap_count(p->map);
    int nparents = 0, nsegs = 0;

    for (int i = 0; i < n; i++) {
        const MapEntry *e = memmap_get(p->map, i);
        if (!e) continue;
        if (map_entry_is_segment(e->type)) {
            if (nsegs < cap) segs[nsegs++] = i;
            continue;
        }
        if (map_entry_is_parent_like(e->type)) {
            if (nparents < cap) parents[nparents++] = i;
        }
    }
    if (nparents > 1)
        g_qsort_with_data(parents, nparents, sizeof(int), map_entry_cmp_by_addr, p);
    if (nsegs > 1)
        g_qsort_with_data(segs, nsegs, sizeof(int), map_entry_cmp_by_addr, p);

    if (np) *np = nparents;
    if (ns) *ns = nsegs;
    return nparents + nsegs;
}

static int map_pick_owner_parent(Project *p, const int *parents, int np, const MapEntry *seg) {
    int best_parent = -1;
    int best_span = 0x7FFFFFFF;
    int best_start = 0x7FFFFFFF;
    for (int i = 0; i < np; i++) {
        int idx = parents[i];
        const MapEntry *cur = memmap_get(p->map, idx);
        if (!cur || !map_entry_is_parent(cur->type)) continue;
        if (!map_parent_contains_segment(cur, seg)) continue;

        int span = cur->end - cur->start;
        if (best_parent < 0 || span < best_span ||
            (span == best_span && cur->start < best_start)) {
            best_parent = idx;
            best_span = span;
            best_start = cur->start;
        }
    }
    return best_parent;
}

static void map_list_append_entry_row(GtkWidget *list, const MapEntry *e, int entry_index,
                                      gboolean is_child) {
    GtkWidget *rb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(rb, is_child ? 20 : 6);
    gtk_widget_set_margin_end(rb, 6);
    gtk_widget_set_margin_top(rb, 2);
    gtk_widget_set_margin_bottom(rb, 2);

    GtkWidget *nl = gtk_label_new(NULL);
    gtk_widget_add_css_class(nl, "monospace");
    char name_text[DISASM_LABEL_MAX + 4];
    if (is_child) snprintf(name_text, sizeof(name_text), "%s", e->name);
    else snprintf(name_text, sizeof(name_text), "- %s", e->name);
    char *m = g_markup_printf_escaped(
        "<span color='%s' font_family='Monospace'>%s</span>",
        map_type_colour(e->type), name_text);
    gtk_label_set_markup(GTK_LABEL(nl), m);
    g_free(m);
    gtk_label_set_xalign(GTK_LABEL(nl), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(nl), PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode(GTK_LABEL(nl), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(nl), 28);
    gtk_widget_set_hexpand(nl, TRUE);
    gtk_box_append(GTK_BOX(rb), nl);

    char range[24];
    snprintf(range, sizeof(range), "0x%04X-0x%04X", e->start, e->end);
    GtkWidget *rl = gtk_label_new(NULL);
    gtk_widget_add_css_class(rl, "monospace");
    char *rm = g_markup_printf_escaped(
        "<span color='#888' font_family='Monospace'>%s</span>", range);
    gtk_label_set_markup(GTK_LABEL(rl), rm);
    g_free(rm);
    gtk_box_append(GTK_BOX(rb), rl);

    GtkWidget *tl = gtk_label_new(NULL);
    char *tm = g_markup_printf_escaped(
        "<span color='%s' size='small'>%s</span>",
        map_type_colour(e->type), map_type_compact_name(e->type));
    gtk_label_set_markup(GTK_LABEL(tl), tm);
    g_free(tm);
    gtk_label_set_xalign(GTK_LABEL(tl), 1.0);
    gtk_label_set_ellipsize(GTK_LABEL(tl), PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode(GTK_LABEL(tl), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(tl), 14);
    gtk_box_append(GTK_BOX(rb), tl);

    GtkListBoxRow *lbr = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
    gtk_list_box_row_set_child(lbr, rb);
    gtk_list_box_row_set_selectable(lbr, FALSE);
    gtk_list_box_row_set_activatable(lbr, FALSE);
    g_object_set_data(G_OBJECT(lbr), "entry-index", GINT_TO_POINTER(entry_index));
    gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(lbr));
}

static void map_dialog_append_entry_row(MapEditCtx *ctx, const MapEntry *e,
                                        int entry_index, gboolean is_child) {
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(row_box, is_child ? 20 : 6);
    gtk_widget_set_margin_top(row_box, 2);
    gtk_widget_set_margin_bottom(row_box, 2);

    char text[120];
    char name_text[DISASM_LABEL_MAX + 4];
    if (is_child) snprintf(name_text, sizeof(name_text), "%s", e->name);
    else snprintf(name_text, sizeof(name_text), "- %s", e->name);
    snprintf(text, sizeof(text), "%-20s  0x%04X-0x%04X  %s",
             name_text, e->start, e->end, map_type_compact_name(e->type));
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_widget_add_css_class(lbl, "monospace");
    char *m = g_markup_printf_escaped("<span font_family='Monospace'>%s</span>", text);
    gtk_label_set_markup(GTK_LABEL(lbl), m);
    g_free(m);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_widget_set_margin_start(lbl, 0);
    gtk_widget_set_margin_top(lbl, 0);
    gtk_widget_set_margin_bottom(lbl, 0);
    gtk_box_append(GTK_BOX(row_box), lbl);

    GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
    gtk_list_box_row_set_child(row, row_box);
    g_object_set_data(G_OBJECT(row), "entry-index", GINT_TO_POINTER(entry_index));
    gtk_list_box_append(GTK_LIST_BOX(ctx->dialog_list), GTK_WIDGET(row));
}

static gboolean map_entries_overlap(const MapEntry *a, const MapEntry *b) {
    return a->start <= b->end && b->start <= a->end;
}

static void segment_dialog_apply_prefill(MapEditCtx *ctx) {
    ctx->updating_form = TRUE;
    if (ctx->have_prefill) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%04X", ctx->prefill_start);
        gtk_editable_set_text(GTK_EDITABLE(ctx->start_entry), buf);
        snprintf(buf, sizeof(buf), "0x%04X", ctx->prefill_end);
        gtk_editable_set_text(GTK_EDITABLE(ctx->end_entry), buf);
    }
    if (ctx->have_name)
        gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), ctx->prefill_name);
    if (ctx->have_type)
        gtk_drop_down_set_selected(ctx->type_drop, map_edit_index_from_type(ctx->prefill_type));
    ctx->updating_form = FALSE;
}

static void segment_dialog_present(MapEditCtx *ctx) {
    gtk_window_present(GTK_WINDOW(ctx->dialog));
}

static void map_list_populate(MapPanelCtx *ctx) {
    if (!ctx || !ctx->panel_list || !ctx->project) return;
    GtkWidget *list = ctx->panel_list;
    Project *p = ctx->project;
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(list)))
        gtk_list_box_remove(GTK_LIST_BOX(list), c);

    int parents[100], segs[100];
    int np = 0, ns = 0;
    map_collect_sorted_indices(p, parents, &np, segs, &ns, (int)G_N_ELEMENTS(parents));
    gboolean seg_used[100] = {0};
    gboolean have_rows = FALSE;

    for (int pi = 0; pi < np; pi++) {
        int pidx = parents[pi];
        const MapEntry *parent = memmap_get(p->map, pidx);
        if (!parent) continue;
        map_list_append_entry_row(list, parent, pidx, FALSE);
        have_rows = TRUE;
        for (int si = 0; si < ns; si++) {
            if (seg_used[si]) continue;
            int sidx = segs[si];
            const MapEntry *seg = memmap_get(p->map, sidx);
            if (!seg) continue;
            if (map_pick_owner_parent(p, parents, np, seg) == pidx) {
                map_list_append_entry_row(list, seg, sidx, TRUE);
                seg_used[si] = TRUE;
                have_rows = TRUE;
            }
        }
    }

    for (int si = 0; si < ns; si++) {
        if (seg_used[si]) continue;
        int sidx = segs[si];
        const MapEntry *seg = memmap_get(p->map, sidx);
        if (!seg) continue;
        map_list_append_entry_row(list, seg, sidx, FALSE);
        have_rows = TRUE;
    }

    if (!have_rows) {
        GtkWidget *lbl = gtk_label_new("No segments");
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        gtk_list_box_append(GTK_LIST_BOX(list), lbl);
    }
}

static void map_dialog_populate(MapEditCtx *ctx) {
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(ctx->dialog_list)))
        gtk_list_box_remove(GTK_LIST_BOX(ctx->dialog_list), c);

    int parents[100], segs[100];
    int np = 0, ns = 0;
    map_collect_sorted_indices(ctx->project, parents, &np, segs, &ns,
                               (int)G_N_ELEMENTS(parents));
    gboolean seg_used[100] = {0};
    gboolean have_rows = FALSE;

    for (int pi = 0; pi < np; pi++) {
        int pidx = parents[pi];
        const MapEntry *parent = memmap_get(ctx->project->map, pidx);
        if (!parent) continue;
        map_dialog_append_entry_row(ctx, parent, pidx, FALSE);
        have_rows = TRUE;
        for (int si = 0; si < ns; si++) {
            if (seg_used[si]) continue;
            int sidx = segs[si];
            const MapEntry *seg = memmap_get(ctx->project->map, sidx);
            if (!seg) continue;
            if (map_pick_owner_parent(ctx->project, parents, np, seg) == pidx) {
                map_dialog_append_entry_row(ctx, seg, sidx, TRUE);
                seg_used[si] = TRUE;
                have_rows = TRUE;
            }
        }
    }

    for (int si = 0; si < ns; si++) {
        if (seg_used[si]) continue;
        int sidx = segs[si];
        const MapEntry *seg = memmap_get(ctx->project->map, sidx);
        if (!seg) continue;
        map_dialog_append_entry_row(ctx, seg, sidx, FALSE);
        have_rows = TRUE;
    }

    if (!have_rows) {
        GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
        GtkWidget *lbl = gtk_label_new("No segments");
        gtk_widget_set_margin_start(lbl, 6);
        gtk_widget_set_margin_top(lbl, 2);
        gtk_widget_set_margin_bottom(lbl, 2);
        gtk_list_box_row_set_child(row, lbl);
        gtk_list_box_row_set_selectable(row, FALSE);
        gtk_list_box_row_set_activatable(row, FALSE);
        g_object_set_data(G_OBJECT(row), "entry-index", GINT_TO_POINTER(-1));
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
    gtk_drop_down_set_selected(ctx->type_drop, map_edit_index_from_type(e->type));
    ctx->updating_form = FALSE;
    ctx->selected_index = idx;
    if (ctx->update_btn)
        gtk_widget_set_sensitive(ctx->update_btn, TRUE);
}

static gboolean map_entry_from_form(MapEditCtx *ctx, MapEntry *e) {
    if (!ctx || !e) return FALSE;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry));
    if (!name[0]) {
        map_show_form_error(ctx, "Invalid segment", "Name is required.");
        return FALSE;
    }

    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, DISASM_LABEL_MAX - 1);
    if (!map_parse_int_field(ctx, ctx->start_entry, "Start", &e->start))
        return FALSE;
    if (!map_parse_int_field(ctx, ctx->end_entry, "End", &e->end))
        return FALSE;
    if (e->end < e->start) {
        map_show_form_error(ctx, "Invalid segment", "End must be greater than or equal to Start.");
        return FALSE;
    }
    guint sel = gtk_drop_down_get_selected(ctx->type_drop);
    if (sel >= G_N_ELEMENTS(map_type_edit_values)) {
        map_show_form_error(ctx, "Invalid segment", "Type selection is invalid.");
        return FALSE;
    }
    e->type  = map_type_edit_values[sel];
    if (e->type == MAP_UNMAPPED) {
        map_show_form_error(
            ctx,
            "Invalid segment",
            "UNMAPPED is derived automatically. Choose a concrete range type.");
        return FALSE;
    }
    if (!map_entry_within_project_span(ctx, e)) {
        int project_start = 0, project_end = 0;
        if (!map_project_span_bounds(ctx, &project_start, &project_end)) {
            map_show_form_error(
                ctx,
                "Invalid segment",
                "Project ROM span is unavailable. Open or import a valid project first.");
            return FALSE;
        }
        char detail[192];
        snprintf(detail, sizeof(detail),
                 "Range must be inside project span 0x%04X-0x%04X.",
                 project_start & 0xFFFF, project_end & 0xFFFF);
        map_show_form_error(ctx, "Invalid segment", detail);
        return FALSE;
    }
    return TRUE;
}

static void segment_build_auto_name(MapEditCtx *ctx, char *buf, size_t buf_sz) {
    buf[0] = '\0';
    if (!ctx) return;

    guint sel = gtk_drop_down_get_selected(ctx->type_drop);
    if (sel >= G_N_ELEMENTS(map_type_edit_values))
        return;
    MapType type = map_type_edit_values[sel];
    int start = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->start_entry)), NULL, 0);
    int end   = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->end_entry)),   NULL, 0);

    if (type == MAP_DIRECT_BYTE) {
        if (start == end)
            snprintf(buf, buf_sz, "bytes @ 0x%04X", start);
        else
            snprintf(buf, buf_sz, "bytes @ 0x%04X-0x%04X", start, end);
    } else if (type == MAP_DIRECT_WORD) {
        if (start == end)
            snprintf(buf, buf_sz, "words @ 0x%04X", start);
        else
            snprintf(buf, buf_sz, "words @ 0x%04X-0x%04X", start, end);
    } else if (type == MAP_DEFINE_MSG) {
        if (start == end)
            snprintf(buf, buf_sz, "msg @ 0x%04X", start);
        else
            snprintf(buf, buf_sz, "msg @ 0x%04X-0x%04X", start, end);
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

static void map_commit_and_close(MapEditCtx *ctx) {
    if (!ctx || ctx->closing) return;
    ctx->closing = TRUE;
    gboolean needs = ctx->needs_reload;
    void (*cb)(gpointer) = ctx->reload_cb;
    gpointer data = ctx->reload_data;
    void (*jump_cb)(gpointer, int) = ctx->jump_cb;
    gpointer jump_data = ctx->jump_data;
    UISegmentSaveFn save_cb = ctx->segment_save_cb;
    gpointer save_data = ctx->segment_save_data;
    UISegmentSaveRequest req;
    req.needs_reload = needs;
    req.have_jump = ctx->have_pending_jump;
    req.jump_addr = req.have_jump ? ctx->pending_jump_addr : -1;
    PANEL_LOG("map close commit needs=%d have_jump=%d jump=0x%04X map=%d selected=%d",
              needs ? 1 : 0, req.have_jump ? 1 : 0,
              req.have_jump ? (req.jump_addr & 0xFFFF) : 0xFFFF,
              memmap_count(ctx->project ? ctx->project->map : NULL), ctx->selected_index);
    if (!req.have_jump && ctx->selected_index >= 0) {
        const MapEntry *e = memmap_get(ctx->project->map, ctx->selected_index);
        if (e) {
            req.have_jump = TRUE;
            req.jump_addr = e->start;
        }
    }
    /* Controller-driven save/reload path (phase 3). */
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    if (save_cb) {
        save_cb(save_data, &req);
        return;
    }
    /* Backward-compatible fallback path. */
    if (req.have_jump) {
        if (jump_cb)
            jump_cb(jump_data, req.jump_addr);
    }
    if (needs && cb) cb(data);
}

static void on_map_close(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    map_commit_and_close(ctx);
}

static void on_map_dialog_destroy(GtkWidget *widget, gpointer ud) {
    (void)widget;
    g_free((MapEditCtx *)ud);
}

static gboolean on_map_dialog_close_request(GtkWindow *window, gpointer ud) {
    (void)window;
    MapEditCtx *ctx = (MapEditCtx *)ud;
    if (ctx) ctx->closing = TRUE;
    return FALSE; /* allow normal destroy path; context is freed in destroy handler */
}

static void on_map_dialog_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer ud) {
    (void)box;
    MapEditCtx *ctx = ud;
    if (!ctx || ctx->updating_form) return;
    if (!row) {
        ctx->selected_index = -1;
        if (ctx->update_btn)
            gtk_widget_set_sensitive(ctx->update_btn, FALSE);
        return;
    }
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    if (idx < 0) {
        ctx->selected_index = -1;
        if (ctx->update_btn)
            gtk_widget_set_sensitive(ctx->update_btn, FALSE);
        return;
    }
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

static gboolean map_entry_overlaps_other_parent(MapEditCtx *ctx, const MapEntry *e,
                                                int skip_index) {
    if (!ctx || !ctx->project || !ctx->project->map || !e) return FALSE;
    if (!map_entry_is_parent(e->type)) return FALSE;

    for (int i = 0; i < memmap_count(ctx->project->map); i++) {
        const MapEntry *cur = memmap_get(ctx->project->map, i);
        if (!cur || i == skip_index || !map_entry_is_parent(cur->type)) continue;
        if (map_entries_overlap(e, cur))
            return TRUE;
    }
    return FALSE;
}

static void on_map_update(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    if (ctx->selected_index < 0) return;
    /* Preserve a user-edited name; only fill it if the field is blank. */
    segment_refresh_auto_name(ctx, FALSE);

    int edited_index = ctx->selected_index;
    MapEntry e;
    if (!map_entry_from_form(ctx, &e)) return;

    int exact_idx = -1;
    if (map_entry_overlaps_other_segment(ctx, &e, ctx->selected_index, &exact_idx)) {
        GtkAlertDialog *alert = gtk_alert_dialog_new(
            "Segments cannot overlap");
        gtk_alert_dialog_set_detail(
            alert,
            "Direct-data segments (DEFB/DEFW/DEFM) can sit inside a broader code "
            "range, but they cannot overlap another segment. Overwrite the existing "
            "range instead.");
        gtk_alert_dialog_show(alert, GTK_WINDOW(ctx->dialog));
        g_object_unref(alert);
        return;
    }
    if (map_entry_overlaps_other_parent(ctx, &e, ctx->selected_index)) {
        GtkAlertDialog *alert = gtk_alert_dialog_new(
            "Parent ranges cannot overlap");
        gtk_alert_dialog_set_detail(
            alert,
            "ROM/RAM/VRAM/IO/SYSVARS ranges must be disjoint. "
            "Adjust this range or edit the conflicting parent range.");
        gtk_alert_dialog_show(alert, GTK_WINDOW(ctx->dialog));
        g_object_unref(alert);
        return;
    }

    if (exact_idx >= 0 && exact_idx != ctx->selected_index) {
        if (!map_run_command(ctx, UI_SEGMENT_CMD_REMOVE, exact_idx, NULL, NULL))
            return;
        if (exact_idx < ctx->selected_index) {
            ctx->selected_index--;
            edited_index = ctx->selected_index;
        }
    }

    if (!map_run_command(ctx, UI_SEGMENT_CMD_UPDATE, ctx->selected_index, &e, NULL))
        return;
    ctx->needs_reload = TRUE;
    ctx->pending_jump_addr = e.start;
    ctx->have_pending_jump = TRUE;
    if (ctx->panel_list) {
        MapPanelCtx *pctx = g_object_get_data(G_OBJECT(ctx->panel_list), "map-panel-ctx");
        if (pctx) map_list_populate(pctx);
    }
    ctx->updating_form = TRUE;
    map_dialog_populate(ctx);
    ctx->selected_index = edited_index;
    if (ctx->dialog_list) {
        GtkListBoxRow *row = find_row_by_entry_index(ctx->dialog_list, edited_index);
        if (row)
            gtk_list_box_select_row(GTK_LIST_BOX(ctx->dialog_list), row);
    }
    ctx->updating_form = FALSE;
    map_dialog_set_entry(ctx, &e, edited_index);
}

static void clear_panel_selection_except(GtkWidget *panels, GtkWidget *except_list) {
    GtkWidget *map_list = panels ? g_object_get_data(G_OBJECT(panels), "map-list") : NULL;
    GtkWidget *sym_list = panels ? g_object_get_data(G_OBJECT(panels), "sym-list") : NULL;
    if (map_list && map_list != except_list) {
        gtk_list_box_unselect_all(GTK_LIST_BOX(map_list));
        gtk_list_box_select_row(GTK_LIST_BOX(map_list), NULL);
    }
    if (sym_list && sym_list != except_list) {
        gtk_list_box_unselect_all(GTK_LIST_BOX(sym_list));
        gtk_list_box_select_row(GTK_LIST_BOX(sym_list), NULL);
    }
}

static gboolean is_descendant_of(GtkWidget *w, GtkWidget *ancestor) {
    GtkWidget *cur = w;
    while (cur) {
        if (cur == ancestor) return TRUE;
        cur = gtk_widget_get_parent(cur);
    }
    return FALSE;
}

static GtkListBoxRow *list_row_from_click(GtkWidget *list, double x, double y) {
    if (!list) return NULL;
    GtkWidget *picked = gtk_widget_pick(list, x, y, GTK_PICK_DEFAULT);
    if (!picked) return NULL;
    GtkWidget *row_w = gtk_widget_get_ancestor(picked, GTK_TYPE_LIST_BOX_ROW);
    if (!row_w) return NULL;
    return GTK_LIST_BOX_ROW(row_w);
}

static void on_panels_click_pressed(GtkGestureClick *gesture, int n_press,
                                    double x, double y, gpointer ud) {
    (void)n_press;
    GtkWidget *panels = GTK_WIDGET(ud);
    GtkWidget *map_list = g_object_get_data(G_OBJECT(panels), "map-list");
    GtkWidget *sym_list = g_object_get_data(G_OBJECT(panels), "sym-list");
    GtkWidget *picked =
        gtk_widget_pick(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture)),
                        x, y, GTK_PICK_DEFAULT);

    if ((map_list && picked && is_descendant_of(picked, map_list)) ||
        (sym_list && picked && is_descendant_of(picked, sym_list))) {
        return;
    }
    clear_panel_selection_except(panels, NULL);
}

static gboolean on_list_escape_clear(GtkEventControllerKey *controller,
                                     guint keyval, guint keycode,
                                     GdkModifierType state, gpointer ud) {
    (void)controller;
    (void)keycode;
    (void)state;
    GtkWidget *list = GTK_WIDGET(ud);
    if (keyval == GDK_KEY_Escape && list) {
        gtk_list_box_unselect_all(GTK_LIST_BOX(list));
        gtk_list_box_select_row(GTK_LIST_BOX(list), NULL);
        return TRUE;
    }
    return FALSE;
}

static void on_list_focus_leave(GtkEventControllerFocus *controller, gpointer ud) {
    (void)controller;
    GtkWidget *list = GTK_WIDGET(ud);
    if (list) {
        gtk_list_box_unselect_all(GTK_LIST_BOX(list));
        gtk_list_box_select_row(GTK_LIST_BOX(list), NULL);
    }
}

static void on_map_dialog_click_pressed(GtkGestureClick *gesture, int n_press,
                                        double x, double y, gpointer ud) {
    (void)n_press;
    MapEditCtx *ctx = ud;
    if (!ctx || !ctx->dialog_list) return;
    GtkWidget *root = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    GtkWidget *picked =
        gtk_widget_pick(root, x, y, GTK_PICK_DEFAULT);
    if (picked && is_descendant_of(picked, ctx->dialog_list))
        return;
    /* Do not clear selection when clicking form/buttons; only clear on empty outer area. */
    if (picked != root) return;
    gtk_list_box_unselect_all(GTK_LIST_BOX(ctx->dialog_list));
}


static void on_map_list_click_pressed(GtkGestureClick *gesture, int n_press,
                                      double x, double y, gpointer ud) {
    (void)gesture;
    MapPanelCtx *ctx = ud;
    if (!ctx || !ctx->panel_list) return;
    GtkListBoxRow *row = list_row_from_click(ctx->panel_list, x, y);
    if (!row) return;

    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    const MapEntry *e = memmap_get(ctx->project->map, idx);
    if (!e) return;

    if (n_press >= 2) {
        if (ctx->panels_root)
            clear_panel_selection_except(ctx->panels_root, NULL);
        open_segment_dialog(ctx, e->start, e->end, e->name, TRUE, e->type);
        return;
    }
    if (n_press == 1 && ctx->listing_outer) {
        if (ctx->panels_root)
            clear_panel_selection_except(ctx->panels_root, NULL);
        ui_listing_select_address(ctx->listing_outer, e->start);
    }
}

static void on_map_remove(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    int idx = ctx ? ctx->selected_index : -1;
    if (idx < 0) return;
    if (!map_run_command(ctx, UI_SEGMENT_CMD_REMOVE, idx, NULL, NULL))
        return;
    ctx->needs_reload = TRUE;
    ctx->selected_index = -1;
    if (ctx->update_btn)
        gtk_widget_set_sensitive(ctx->update_btn, FALSE);
    map_dialog_populate(ctx);
    if (ctx->panel_list) {
        MapPanelCtx *pctx = g_object_get_data(G_OBJECT(ctx->panel_list), "map-panel-ctx");
        if (pctx) map_list_populate(pctx);
    }
}

static void on_map_add(GtkButton *btn, gpointer ud) {
    MapEditCtx *ctx = ud;
    segment_refresh_auto_name(ctx, TRUE);
    MapEntry e;
    if (!map_entry_from_form(ctx, &e)) return;

    if (map_entry_is_segment(e.type)) {
        for (int i = 0; i < memmap_count(ctx->project->map); i++) {
            const MapEntry *cur = memmap_get(ctx->project->map, i);
            if (!cur || !map_entry_is_segment(cur->type)) continue;
            if (map_entries_overlap(&e, cur)) {
                GtkAlertDialog *alert = gtk_alert_dialog_new(
                    "Segments cannot overlap");
                gtk_alert_dialog_set_detail(
                    alert,
                    "Direct-data segments (DEFB/DEFW/DEFM) can sit inside a "
                    "broader code range, but they cannot overlap another segment. "
                    "Choose a different range or edit the existing segment.");
                gtk_alert_dialog_show(alert, GTK_WINDOW(ctx->dialog));
                g_object_unref(alert);
                return;
            }
        }
    }
    if (map_entry_is_parent(e.type) &&
        map_entry_overlaps_other_parent(ctx, &e, -1)) {
        GtkAlertDialog *alert = gtk_alert_dialog_new(
            "Parent ranges cannot overlap");
        gtk_alert_dialog_set_detail(
            alert,
            "ROM/RAM/VRAM/IO/SYSVARS ranges must be disjoint. "
            "Adjust this range or edit the existing parent range.");
        gtk_alert_dialog_show(alert, GTK_WINDOW(ctx->dialog));
        g_object_unref(alert);
        return;
    }

    int new_index = -1;
    if (!map_run_command(ctx, UI_SEGMENT_CMD_ADD, -1, &e, &new_index))
        return;
    PANEL_LOG("map add [%04X-%04X] type=%d map=%d new_idx=%d",
              e.start & 0xFFFF, e.end & 0xFFFF, (int)e.type,
              memmap_count(ctx->project ? ctx->project->map : NULL), new_index);
    ctx->needs_reload = TRUE;
    if (map_entry_is_segment(e.type)) {
        ctx->pending_jump_addr = e.start;
        ctx->have_pending_jump = TRUE;
    }
    map_dialog_populate(ctx);
    if (ctx->panel_list) {
        MapPanelCtx *pctx = g_object_get_data(G_OBJECT(ctx->panel_list), "map-panel-ctx");
        if (pctx) map_list_populate(pctx);
    }
    GtkListBoxRow *new_row = find_row_by_entry_index(ctx->dialog_list, new_index);
    if (new_row)
        gtk_list_box_select_row(GTK_LIST_BOX(ctx->dialog_list), new_row);
}

static MapEditCtx *segment_dialog_create(MapPanelCtx *pctx) {
    MapEditCtx *ctx = g_new0(MapEditCtx, 1);
    ctx->project     = pctx->project;
    ctx->panel_list  = pctx->panel_list;
    ctx->reload_cb   = pctx->reload_cb;
    ctx->reload_data = pctx->reload_data;
    ctx->jump_cb     = pctx->jump_cb;
    ctx->jump_data   = pctx->jump_data;
    ctx->segment_cmd_cb = pctx->segment_cmd_cb;
    ctx->segment_cmd_data = pctx->segment_cmd_data;
    ctx->segment_save_cb = pctx->segment_save_cb;
    ctx->segment_save_data = pctx->segment_save_data;
    ctx->selected_index = -1;
    ctx->focus_index = -1;
    ctx->updating_form = FALSE;

    GtkWidget *dlg = gtk_window_new();
    ctx->dialog = dlg;
    gtk_window_set_title(GTK_WINDOW(dlg), "Edit Segments");
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(pctx->window));
    gtk_window_set_default_size(GTK_WINDOW(dlg), 520, 420);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
    g_signal_connect(dlg, "close-request", G_CALLBACK(on_map_dialog_close_request), ctx);
    g_signal_connect(dlg, "destroy", G_CALLBACK(on_map_dialog_destroy), ctx);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(outer, 12); gtk_widget_set_margin_bottom(outer, 12);
    gtk_widget_set_margin_start(outer, 12); gtk_widget_set_margin_end(outer, 12);
    gtk_window_set_child(GTK_WINDOW(dlg), outer);
    GtkGesture *outer_click = gtk_gesture_click_new();
    gtk_widget_add_controller(outer, GTK_EVENT_CONTROLLER(outer_click));
    g_signal_connect(outer_click, "pressed",
                     G_CALLBACK(on_map_dialog_click_pressed), ctx);

    ctx->dialog_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(ctx->dialog_list), GTK_SELECTION_SINGLE);
    GtkEventController *dlg_map_key = gtk_event_controller_key_new();
    gtk_widget_add_controller(ctx->dialog_list, dlg_map_key);
    g_signal_connect(dlg_map_key, "key-pressed",
                     G_CALLBACK(on_list_escape_clear), ctx->dialog_list);
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
    ctx->type_drop = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(map_type_edit_names));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->type_drop), 1, 3, 1, 1);
    gtk_drop_down_set_selected(ctx->type_drop, map_edit_index_from_type(MAP_ROM));
    g_signal_connect(ctx->type_drop, "notify::selected",
                     G_CALLBACK(on_segment_type_changed), ctx);

    GtkWidget *add_btn = gtk_button_new_with_label("Add");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_map_add), ctx);
    gtk_box_append(GTK_BOX(outer), add_btn);

    ctx->update_btn = gtk_button_new_with_label("Update");
    gtk_widget_set_sensitive(ctx->update_btn, FALSE);
    g_signal_connect(ctx->update_btn, "clicked", G_CALLBACK(on_map_update), ctx);
    gtk_box_append(GTK_BOX(outer), ctx->update_btn);

    gtk_box_append(GTK_BOX(outer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    ctx->close_btn = gtk_button_new_with_label("Close");
    g_signal_connect(ctx->close_btn, "clicked", G_CALLBACK(on_map_close), ctx);
    gtk_box_append(GTK_BOX(outer), ctx->close_btn);

    return ctx;
}

static void open_segment_dialog(MapPanelCtx *pctx, int start_offset, int end_offset,
                                const char *name,
                                gboolean set_type, MapType type) {
    MapEditCtx *ctx = segment_dialog_create(pctx);
    ctx->prefill_start = start_offset;
    ctx->prefill_end = end_offset;
    ctx->have_prefill = (start_offset >= 0 && end_offset >= 0);
    if (!ctx->have_prefill && pctx && pctx->project) {
        int span_start = 0, span_end = 0;
        if (map_project_span_bounds_for_project(pctx->project, &span_start, &span_end)) {
            ctx->prefill_start = span_start;
            ctx->prefill_end = span_end;
            ctx->have_prefill = TRUE;
        }
    }
    if (name && name[0]) {
        strncpy(ctx->prefill_name, name, sizeof(ctx->prefill_name) - 1);
        ctx->have_name = TRUE;
    }
    ctx->prefill_type = type;
    ctx->have_type = set_type;

    if (start_offset >= 0 && end_offset >= 0)
        map_find_exact_segment(pctx, start_offset, end_offset, &ctx->focus_index);

    segment_dialog_apply_prefill(ctx);
    if ((!ctx->have_name || !ctx->prefill_name[0]) &&
        (type == MAP_DIRECT_BYTE || type == MAP_DIRECT_WORD || type == MAP_DEFINE_MSG))
        segment_refresh_auto_name(ctx, FALSE);
    if (ctx->focus_index >= 0) {
        GtkListBoxRow *row = find_row_by_entry_index(ctx->dialog_list, ctx->focus_index);
        if (row)
            gtk_list_box_select_row(GTK_LIST_BOX(ctx->dialog_list), row);
    }
    segment_dialog_present(ctx);
}

static void on_map_edit_clicked(GtkButton *btn, gpointer ud) {
    MapPanelCtx *pctx = ud;
    if (pctx && pctx->project)
        map_ensure_default_rom_segment(pctx->project);
    if (pctx && pctx->panels_root)
        clear_panel_selection_except(pctx->panels_root, NULL);
    open_segment_dialog(pctx, -1, -1, "ROM", TRUE, MAP_ROM);
}

static GtkWidget *ui_memmap_panel_new(Project *p, GtkWidget *window) {
    map_ensure_default_rom_segment(p);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(box, 10); gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_vexpand(box, TRUE);

    MapPanelCtx *ctx = g_new0(MapPanelCtx, 1);
    ctx->project = p; ctx->window = window;

    GtkWidget *hdr = panel_header_row_new("SEGMENTS", "Edit",
                                          G_CALLBACK(on_map_edit_clicked), ctx);
    gtk_box_append(GTK_BOX(box), hdr);

    GtkWidget *list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), FALSE);
    apply_panel_list_style(list);
    GtkEventController *map_key = gtk_event_controller_key_new();
    gtk_widget_add_controller(list, map_key);
    g_signal_connect(map_key, "key-pressed",
                     G_CALLBACK(on_list_escape_clear), list);
    GtkEventController *map_focus = gtk_event_controller_focus_new();
    gtk_widget_add_controller(list, map_focus);
    g_signal_connect(map_focus, "leave",
                     G_CALLBACK(on_list_focus_leave), list);
    ctx->panel_list = list;
    g_object_set_data(G_OBJECT(list), "map-panel-ctx", ctx);
    GtkGesture *map_click = gtk_gesture_click_new();
    gtk_widget_add_controller(list, GTK_EVENT_CONTROLLER(map_click));
    g_signal_connect(map_click, "pressed",
                     G_CALLBACK(on_map_list_click_pressed), ctx);

    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(sw, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), list);
    gtk_box_append(GTK_BOX(box), sw);

    map_list_populate(ctx);
    g_object_set_data_full(G_OBJECT(box), "map-ctx", ctx, (GDestroyNotify)g_free);
    return box;
}

/* ==========================================================================
 * Symbols panel (unchanged from before)
 * ========================================================================== */

typedef struct {
    Project   *project;
    GtkWidget *window;
    GtkWidget *panels_root;
    GtkWidget *listing_outer;
    GtkWidget *panel_list;
    GtkWidget *filter_entry;
} SymPanelCtx;

typedef struct {
    Project      *project;
    GtkWidget    *panel_list;
    GtkWidget    *listing_outer;
    GtkWidget    *dialog;
    GtkWidget    *dialog_list;
    GtkWidget    *name_entry;
    GtkWidget    *addr_entry;
    GtkDropDown  *type_drop;
    GtkWidget    *update_btn;
    int           selected_index;
    int           pending_jump_addr;
    gboolean      have_pending_jump;
} SymEditCtx;

static void on_sym_edit_clicked(GtkButton *btn, gpointer ud);
static void open_symbol_dialog(SymPanelCtx *pctx, int focus_index);

static int find_line_index_by_addr(Project *p, int addr) {
    if (!p || !p->lines || p->nlines <= 0) return -1;
    for (int i = 0; i < p->nlines; i++) {
        if (p->lines[i].addr == addr)
            return i;
    }
    return -1;
}

static void set_line_label_for_symbol(SymEditCtx *ctx, const Symbol *s) {
    if (!ctx || !s || s->type != SYM_JUMP_LABEL) return;
    int li = find_line_index_by_addr(ctx->project, s->addr);
    if (li < 0) return;
    strncpy(ctx->project->lines[li].label, s->name, DISASM_LABEL_MAX - 1);
    if (ctx->listing_outer)
        ui_listing_refresh_line(ctx->listing_outer, li);
}

static void clear_line_label_for_symbol(SymEditCtx *ctx, const Symbol *s) {
    if (!ctx || !s || s->type != SYM_JUMP_LABEL) return;
    int li = find_line_index_by_addr(ctx->project, s->addr);
    if (li < 0) return;
    /* Clear only if it still matches the symbol being removed/replaced. */
    if (strncmp(ctx->project->lines[li].label, s->name, DISASM_LABEL_MAX - 1) == 0) {
        ctx->project->lines[li].label[0] = '\0';
        if (ctx->listing_outer)
            ui_listing_refresh_line(ctx->listing_outer, li);
    }
}

static void sync_symbol_edit_to_labels(SymEditCtx *ctx,
                                       const Symbol *old_s, gboolean have_old,
                                       const Symbol *new_s, gboolean have_new) {
    if (!ctx || !ctx->project || !ctx->project->lines) return;
    if (have_old) clear_line_label_for_symbol(ctx, old_s);
    if (have_new) set_line_label_for_symbol(ctx, new_s);
}

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
        gtk_widget_add_css_class(nl, "monospace");
        char *nm = g_markup_printf_escaped(
            "<span color='%s' font_family='Monospace'>%s</span>",
            sym_type_colour(s->type), s->name);
        gtk_label_set_markup(GTK_LABEL(nl), nm); g_free(nm);
        gtk_label_set_xalign(GTK_LABEL(nl), 0.0);
        gtk_label_set_ellipsize(GTK_LABEL(nl), PANGO_ELLIPSIZE_END);
        gtk_label_set_single_line_mode(GTK_LABEL(nl), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(nl), 28);
        gtk_widget_set_hexpand(nl, TRUE);
        gtk_box_append(GTK_BOX(rb), nl);

        GtkWidget *al = gtk_label_new(NULL);
        gtk_widget_add_css_class(al, "monospace");
        char *am = g_markup_printf_escaped(
            "<span color='#888' font_family='Monospace'>%04X</span>", s->addr);
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
        gtk_list_box_row_set_selectable(lbr, FALSE);
        gtk_list_box_row_set_activatable(lbr, FALSE);
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
        gtk_widget_add_css_class(lbl, "monospace");
        char *m = g_markup_printf_escaped("<span font_family='Monospace'>%s</span>", text);
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

static void sym_dialog_set_entry(SymEditCtx *ctx, const Symbol *s, int idx) {
    if (!ctx || !s) return;
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), s->name);
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%04X", s->addr & 0xFFFF);
        gtk_editable_set_text(GTK_EDITABLE(ctx->addr_entry), buf);
    }
    gtk_drop_down_set_selected(ctx->type_drop, s->type);
    ctx->selected_index = idx;
    if (ctx->update_btn)
        gtk_widget_set_sensitive(ctx->update_btn, TRUE);
}

static gboolean sym_from_form(SymEditCtx *ctx, Symbol *out) {
    if (!ctx || !out) return FALSE;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry));
    if (!name || !name[0]) return FALSE;

    memset(out, 0, sizeof(*out));
    strncpy(out->name, name, DISASM_LABEL_MAX - 1);
    out->addr = (int)strtol(gtk_editable_get_text(GTK_EDITABLE(ctx->addr_entry)), NULL, 0);
    out->type = sym_type_values[gtk_drop_down_get_selected(ctx->type_drop)];
    return TRUE;
}

static void on_sym_dialog_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer ud) {
    (void)box;
    SymEditCtx *ctx = ud;
    if (!ctx) return;
    if (!row) {
        ctx->selected_index = -1;
        if (ctx->update_btn)
            gtk_widget_set_sensitive(ctx->update_btn, FALSE);
        return;
    }
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    const Symbol *s = symbols_get(ctx->project->sym, idx);
    if (!s) return;
    sym_dialog_set_entry(ctx, s, idx);
    ctx->pending_jump_addr = s->addr;
    ctx->have_pending_jump = TRUE;
}

static void on_sym_close(GtkButton *btn, gpointer ud) {
    SymEditCtx *ctx = ud;
    sym_list_populate(ctx->panel_list, ctx->project, NULL);
    if (ctx->have_pending_jump && ctx->listing_outer)
        ui_listing_select_address(ctx->listing_outer, ctx->pending_jump_addr);
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    g_free(ctx);
}

static void on_sym_list_click_pressed(GtkGestureClick *gesture, int n_press,
                                      double x, double y, gpointer ud) {
    (void)gesture;
    SymPanelCtx *ctx = ud;
    if (!ctx || !ctx->panel_list) return;
    GtkListBoxRow *row = list_row_from_click(ctx->panel_list, x, y);
    if (!row) return;

    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "entry-index"));
    const Symbol *s = symbols_get(ctx->project->sym, idx);
    if (!s) return;

    if (n_press >= 2) {
        if (ctx->panels_root)
            clear_panel_selection_except(ctx->panels_root, NULL);
        open_symbol_dialog(ctx, idx);
        return;
    }
    if (n_press == 1 && ctx->listing_outer) {
        if (ctx->panels_root)
            clear_panel_selection_except(ctx->panels_root, NULL);
        ui_listing_select_address(ctx->listing_outer, s->addr);
    }
}

static void on_sym_dialog_click_pressed(GtkGestureClick *gesture, int n_press,
                                        double x, double y, gpointer ud) {
    (void)n_press;
    SymEditCtx *ctx = ud;
    if (!ctx || !ctx->dialog_list) return;
    GtkWidget *root = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    GtkWidget *picked =
        gtk_widget_pick(root, x, y, GTK_PICK_DEFAULT);
    if (picked && is_descendant_of(picked, ctx->dialog_list))
        return;
    /* Do not clear selection when clicking form/buttons; only clear on empty outer area. */
    if (picked != root) return;
    gtk_list_box_unselect_all(GTK_LIST_BOX(ctx->dialog_list));
}

static void on_sym_remove(GtkButton *btn, gpointer ud) {
    SymEditCtx *ctx = ud;
    int idx = ctx ? ctx->selected_index : -1;
    if (idx < 0) return;
    Symbol old_s = {0};
    gboolean have_old = (symbols_get_safe(ctx->project->sym, idx, &old_s) == 0);
    {
        const Symbol *cur = symbols_get(ctx->project->sym, idx);
        if (cur) {
            ctx->pending_jump_addr = cur->addr;
            ctx->have_pending_jump = TRUE;
        }
    }
    symbols_remove(ctx->project->sym, idx);
    sync_symbol_edit_to_labels(ctx, &old_s, have_old, NULL, FALSE);
    sym_dialog_populate(ctx);
    sym_list_populate(ctx->panel_list, ctx->project, NULL);
    ctx->selected_index = -1;
    if (ctx->update_btn)
        gtk_widget_set_sensitive(ctx->update_btn, FALSE);
}

static void on_sym_update(GtkButton *btn, gpointer ud) {
    SymEditCtx *ctx = ud;
    if (!ctx || ctx->selected_index < 0) return;
    Symbol old_s = {0};
    gboolean have_old = (symbols_get_safe(ctx->project->sym, ctx->selected_index, &old_s) == 0);
    Symbol s;
    if (!sym_from_form(ctx, &s)) return;
    if (symbols_replace(ctx->project->sym, ctx->selected_index, &s) != 0) return;
    sync_symbol_edit_to_labels(ctx, &old_s, have_old, &s, TRUE);
    ctx->pending_jump_addr = s.addr;
    ctx->have_pending_jump = TRUE;
    sym_dialog_populate(ctx);
    sym_list_populate(ctx->panel_list, ctx->project, NULL);
    GtkListBoxRow *row =
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(ctx->dialog_list), ctx->selected_index);
    if (row)
        gtk_list_box_select_row(GTK_LIST_BOX(ctx->dialog_list), row);
}

static void on_sym_add(GtkButton *btn, gpointer ud) {
    SymEditCtx *ctx = ud;
    if (!ctx->project->sym) {
        ctx->project->sym = symbols_new();
        if (!ctx->project->sym) return;
    }
    Symbol s = {0};
    if (!sym_from_form(ctx, &s)) return;

    symbols_add(ctx->project->sym, &s);
    sync_symbol_edit_to_labels(ctx, NULL, FALSE, &s, TRUE);
    ctx->pending_jump_addr = s.addr;
    ctx->have_pending_jump = TRUE;
    sym_dialog_populate(ctx);
    sym_list_populate(ctx->panel_list, ctx->project, NULL);
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->addr_entry), "0x0000");
    ctx->selected_index = -1;
    if (ctx->update_btn)
        gtk_widget_set_sensitive(ctx->update_btn, FALSE);
}

static void on_sym_edit_clicked(GtkButton *btn, gpointer ud) {
    SymPanelCtx *pctx = ud;
    open_symbol_dialog(pctx, -1);
}

static void open_symbol_dialog(SymPanelCtx *pctx, int focus_index) {
    if (!pctx) return;
    if (pctx->panels_root)
        clear_panel_selection_except(pctx->panels_root, NULL);
    SymEditCtx *ctx = g_new0(SymEditCtx, 1);
    ctx->project    = pctx->project;
    ctx->panel_list = pctx->panel_list;
    ctx->listing_outer = pctx->listing_outer;
    ctx->selected_index = -1;

    GtkWidget *dlg = gtk_window_new();
    ctx->dialog = dlg;
    gtk_window_set_title(GTK_WINDOW(dlg), "Edit Symbols");
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(pctx->window));
    gtk_window_set_default_size(GTK_WINDOW(dlg), 520, 440);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(outer, 12); gtk_widget_set_margin_bottom(outer, 12);
    gtk_widget_set_margin_start(outer, 12); gtk_widget_set_margin_end(outer, 12);
    gtk_window_set_child(GTK_WINDOW(dlg), outer);
    GtkGesture *outer_click = gtk_gesture_click_new();
    gtk_widget_add_controller(outer, GTK_EVENT_CONTROLLER(outer_click));
    g_signal_connect(outer_click, "pressed",
                     G_CALLBACK(on_sym_dialog_click_pressed), ctx);

    ctx->dialog_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(ctx->dialog_list), GTK_SELECTION_SINGLE);
    GtkEventController *dlg_sym_key = gtk_event_controller_key_new();
    gtk_widget_add_controller(ctx->dialog_list, dlg_sym_key);
    g_signal_connect(dlg_sym_key, "key-pressed",
                     G_CALLBACK(on_list_escape_clear), ctx->dialog_list);
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(sw, TRUE); gtk_widget_set_size_request(sw, -1, 160);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), ctx->dialog_list);
    gtk_box_append(GTK_BOX(outer), sw);
    sym_dialog_populate(ctx);
    g_signal_connect(ctx->dialog_list, "row-selected",
                     G_CALLBACK(on_sym_dialog_row_selected), ctx);

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

    GtkWidget *add_btn = gtk_button_new_with_label("Add");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_sym_add), ctx);
    gtk_box_append(GTK_BOX(outer), add_btn);

    ctx->update_btn = gtk_button_new_with_label("Update");
    gtk_widget_set_sensitive(ctx->update_btn, FALSE);
    g_signal_connect(ctx->update_btn, "clicked", G_CALLBACK(on_sym_update), ctx);
    gtk_box_append(GTK_BOX(outer), ctx->update_btn);

    gtk_box_append(GTK_BOX(outer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_sym_close), ctx);
    gtk_box_append(GTK_BOX(outer), close_btn);

    if (focus_index >= 0) {
        GtkListBoxRow *row =
            gtk_list_box_get_row_at_index(GTK_LIST_BOX(ctx->dialog_list), focus_index);
        if (row)
            gtk_list_box_select_row(GTK_LIST_BOX(ctx->dialog_list), row);
    }

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
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), FALSE);
    apply_panel_list_style(list);
    GtkEventController *sym_key = gtk_event_controller_key_new();
    gtk_widget_add_controller(list, sym_key);
    g_signal_connect(sym_key, "key-pressed",
                     G_CALLBACK(on_list_escape_clear), list);
    GtkEventController *sym_focus = gtk_event_controller_focus_new();
    gtk_widget_add_controller(list, sym_focus);
    g_signal_connect(sym_focus, "leave",
                     G_CALLBACK(on_list_focus_leave), list);
    ctx->panel_list   = list;
    ctx->filter_entry = filter;
    GtkGesture *sym_click = gtk_gesture_click_new();
    gtk_widget_add_controller(list, GTK_EVENT_CONTROLLER(sym_click));
    g_signal_connect(sym_click, "pressed",
                     G_CALLBACK(on_sym_list_click_pressed), ctx);

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
    gtk_widget_set_vexpand(box, TRUE);

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
    gtk_widget_set_vexpand(grid, TRUE);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_box_append(GTK_BOX(box), grid);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Label"),   0, 0, 1, 1);
    ap->label_entry = gtk_entry_new();
    gtk_widget_add_css_class(ap->label_entry, "monospace");
    gtk_widget_set_hexpand(ap->label_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ap->label_entry, 1, 0, 1, 1);
    g_signal_connect(ap->label_entry, "changed",
                     G_CALLBACK(on_label_changed), ap);
    attach_focus_out(ap->label_entry, ap);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Comment"), 0, 1, 1, 1);
    ap->comment_entry = gtk_entry_new();
    gtk_widget_add_css_class(ap->comment_entry, "monospace");
    gtk_grid_attach(GTK_GRID(grid), ap->comment_entry, 1, 1, 1, 1);
    g_signal_connect(ap->comment_entry, "changed",
                     G_CALLBACK(on_comment_changed), ap);
    attach_focus_out(ap->comment_entry, ap);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Block"),   0, 2, 1, 1);
    GtkWidget *s = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(s, TRUE);
    gtk_widget_set_hexpand(s, TRUE);
    ap->block_view = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(ap->block_view), TRUE);
    gtk_widget_add_css_class(ap->block_view, "monospace");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ap->block_view), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s), ap->block_view);
    gtk_grid_attach(GTK_GRID(grid), s, 1, 2, 1, 1);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ap->block_view));
    g_signal_connect(buf, "changed", G_CALLBACK(on_block_changed), ap);
    attach_focus_out(ap->block_view, ap);

    if (out_ap) *out_ap = ap;
    g_object_set_data_full(G_OBJECT(box), "ap", ap, (GDestroyNotify)g_free);
    return box;
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void ui_panels_update_selection(GtkWidget *panels, DisasmLine *dl,
                                 int line_index) {
    AnnotationPanel *ap = panels ? g_object_get_data(G_OBJECT(panels), "ann-ap") : NULL;
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

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ap->block_view));
    gtk_text_buffer_set_text(buf, dl->block, -1);

    ap->updating = 0;
}

void ui_panels_refresh_symbols(GtkWidget *panels) {
    SymPanelCtx *ctx = panels ? g_object_get_data(G_OBJECT(panels), "sym-ctx-root") : NULL;
    if (ctx)
        sym_list_populate(ctx->panel_list, ctx->project, NULL);
}

void ui_panels_clear_selection(GtkWidget *panels) {
    clear_panel_selection_except(panels, NULL);
}

void ui_panels_set_reload_cb(GtkWidget *panels, void (*cb)(gpointer), gpointer data) {
    MapPanelCtx *ctx = panels ? g_object_get_data(G_OBJECT(panels), "map-ctx-root") : NULL;
    if (!ctx) return;
    ctx->reload_cb   = cb;
    ctx->reload_data = data;
}

void ui_panels_set_jump_cb(GtkWidget *panels, void (*cb)(gpointer, int), gpointer data) {
    MapPanelCtx *ctx = panels ? g_object_get_data(G_OBJECT(panels), "map-ctx-root") : NULL;
    if (!ctx) return;
    ctx->jump_cb = cb;
    ctx->jump_data = data;
}

void ui_panels_set_segment_command_cb(GtkWidget *panels, UISegmentCommandFn cb, gpointer data) {
    MapPanelCtx *ctx = panels ? g_object_get_data(G_OBJECT(panels), "map-ctx-root") : NULL;
    if (!ctx) return;
    ctx->segment_cmd_cb = cb;
    ctx->segment_cmd_data = data;
}

void ui_panels_set_segment_save_cb(GtkWidget *panels, UISegmentSaveFn cb, gpointer data) {
    MapPanelCtx *ctx = panels ? g_object_get_data(G_OBJECT(panels), "map-ctx-root") : NULL;
    if (!ctx) return;
    ctx->segment_save_cb = cb;
    ctx->segment_save_data = data;
}

GtkWidget *ui_panels_new(Project *p, GtkWidget *window,
                          const char *project_path,
                          GtkWidget *listing_outer) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(box, 340, -1);
    gtk_widget_set_hexpand(box, FALSE);
    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_widget_set_margin_start(switcher, 6);
    gtk_widget_set_margin_end(switcher, 6);
    gtk_widget_set_margin_top(switcher, 6);
    gtk_widget_set_margin_bottom(switcher, 4);
    gtk_box_append(GTK_BOX(box), switcher);

    GtkWidget *stack = gtk_stack_new();
    gtk_widget_set_hexpand(stack, TRUE);
    gtk_widget_set_vexpand(stack, TRUE);
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher), GTK_STACK(stack));
    gtk_box_append(GTK_BOX(box), stack);

    GtkWidget *map_panel = ui_memmap_panel_new(p, window);
    GtkWidget *sym_panel = ui_symbols_panel_new(p, window);
    GtkWidget *ann_panel = ui_annotation_panel_new(p, project_path, listing_outer, NULL);
    gtk_stack_add_titled(GTK_STACK(stack), map_panel, "segments", "Segments");
    gtk_stack_add_titled(GTK_STACK(stack), sym_panel, "symbols", "Symbols");
    gtk_stack_add_titled(GTK_STACK(stack), ann_panel, "annotation", "Annotation");

    AnnotationPanel *ap = g_object_get_data(G_OBJECT(ann_panel), "ap");
    if (ap) {
        ap->panels = box;
        g_object_set_data(G_OBJECT(box), "ann-ap", ap);
    }

    MapPanelCtx *mctx = g_object_get_data(G_OBJECT(map_panel), "map-ctx");
    if (mctx) {
        mctx->listing_outer = listing_outer;
        mctx->panels_root = box;
        g_object_set_data(G_OBJECT(box), "map-list", mctx->panel_list);
        g_object_set_data(G_OBJECT(box), "map-ctx-root", mctx);
    }

    SymPanelCtx *sctx = g_object_get_data(G_OBJECT(sym_panel), "sym-ctx");
    if (sctx) {
        sctx->listing_outer = listing_outer;
        sctx->panels_root = box;
        g_object_set_data(G_OBJECT(box), "sym-list", sctx->panel_list);
        g_object_set_data(G_OBJECT(box), "sym-ctx-root", sctx);
    }

    GtkGesture *click = gtk_gesture_click_new();
    gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(click, "pressed", G_CALLBACK(on_panels_click_pressed), box);

    return box;
}
