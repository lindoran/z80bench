#include <gtk/gtk.h>
#include "z80bench.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Column layout (monospace characters):
 *
 *   0000  C3 74 00       jp    00074h          ; comment to col 79
 *   ^     ^              ^     ^               ^
 *   0     6              18    24              40
 *
 *   COL_ADDR   =  0  (4 chars + 2 space = 6)
 *   COL_BYTES  =  6  (12 chars: up to 4 × "XX " padded)
 *   COL_ASCII  = 18  (4 chars + 1 space = 5)
 *   COL_MNEM   = 23  (5 chars + 1 space)
 *   COL_OPS    = 29  (variable)
 *   COL_CMT    = 45  ("; " + comment text, to col 79)
 *   COL_MAX    = 79
 *
 * Block/label rows indent to COL_MNEM - 1 (column 22).
 * Block comment text wraps at 77 chars (79 - 2 for "; ") and each
 * wrapped line is shown on its own display line prefixed with "; ".
 */

#define COL_MNEM 23
#define COL_OPS  29
#define COL_CMT  45
#define COL_MAX  79
#define CMT_MAX  (COL_MAX - COL_CMT - 2)   /* 32 chars after "; " */
#define BLOCK_WRAP 77                        /* 79 - 2 for "; " */

/* Row type tags */
#define ROW_TYPE_MAIN  0
#define ROW_TYPE_BLOCK 1
#define ROW_TYPE_LABEL 2

typedef struct {
    int      line_index;
    Project *project;
} RowData;

/* ==========================================================================
 * Colour helpers
 * ========================================================================== */

static const char *mnemonic_colour(const char *m) {
    if (g_ascii_strcasecmp(m,"JP"  )==0||g_ascii_strcasecmp(m,"JR"  )==0||
        g_ascii_strcasecmp(m,"CALL")==0||g_ascii_strcasecmp(m,"RET" )==0||
        g_ascii_strcasecmp(m,"RST" )==0||g_ascii_strcasecmp(m,"DJNZ")==0)
        return "#F0997B";
    if (g_ascii_strcasecmp(m,"LD"  )==0||g_ascii_strcasecmp(m,"PUSH")==0||
        g_ascii_strcasecmp(m,"POP" )==0)
        return "#85B7EB";
    if (g_ascii_strcasecmp(m,"ADD" )==0||g_ascii_strcasecmp(m,"ADC" )==0||
        g_ascii_strcasecmp(m,"SUB" )==0||g_ascii_strcasecmp(m,"SBC" )==0||
        g_ascii_strcasecmp(m,"AND" )==0||g_ascii_strcasecmp(m,"OR"  )==0||
        g_ascii_strcasecmp(m,"XOR" )==0||g_ascii_strcasecmp(m,"CP"  )==0||
        g_ascii_strcasecmp(m,"INC" )==0||g_ascii_strcasecmp(m,"DEC" )==0)
        return "#EF9F27";
    if (g_ascii_strcasecmp(m,"LDIR")==0||g_ascii_strcasecmp(m,"CPIR")==0||
        g_ascii_strcasecmp(m,"INIR")==0||g_ascii_strcasecmp(m,"OTIR")==0)
        return "#5DCAA5";
    if (g_ascii_strcasecmp(m,"HALT")==0) return "#E24B4A";
    return NULL;
}

static const char *symbol_colour(int type) {
    switch (type) {
        case SYM_ROM_CALL:   return "#F0997B";
        case SYM_VECTOR:     return "#EF9F27";
        case SYM_JUMP_LABEL: return "#639922";
        case SYM_WRITABLE:   return "#85B7EB";
        case SYM_PORT:       return "#D485EB";
        default:             return NULL;
    }
}

/* ==========================================================================
 * Block comment formatting — wrap at BLOCK_WRAP chars, prefix each line
 * ========================================================================== */

/*
 * Format the block text into a GString: each logical line (from user \n or
 * from word-wrap at BLOCK_WRAP chars) becomes "; <text>\n".
 * The caller owns the returned GString.
 */
static GString *format_block(const char *text, int indent) {
    GString *out = g_string_new(NULL);
    const char *p = text;

    while (*p) {
        const char *nl = strchr(p, '\n');
        int line_len = nl ? (int)(nl - p) : (int)strlen(p);

        int consumed = 0;
        int avail = BLOCK_WRAP - indent;
        if (avail < 4) avail = 4;

        while (consumed < line_len) {
            int remaining = line_len - consumed;
            int chunk = remaining <= avail ? remaining : avail;

            if (chunk < remaining && p[consumed + chunk] != ' ') {
                int j = chunk;
                while (j > 0 && p[consumed + j - 1] != ' ') j--;
                if (j > 0) chunk = j;
            }

            /* Indent + "; " + text chunk */
            for (int s = 0; s < indent; s++) g_string_append_c(out, ' ');
            g_string_append(out, "; ");
            g_string_append_len(out, p + consumed, chunk);
            g_string_append_c(out, '\n');
            consumed += chunk;
            while (consumed < line_len && p[consumed] == ' ') consumed++;
        }

        if (line_len == 0) {
            for (int s = 0; s < indent; s++) g_string_append_c(out, ' ');
            g_string_append(out, ";\n");
        }

        p += line_len;
        if (*p == '\n') p++;
    }

    if (out->len > 0 && out->str[out->len - 1] == '\n')
        g_string_truncate(out, out->len - 1);

    return out;
}

/* ==========================================================================
 * Row content builders — single monospace label per row
 * ========================================================================== */

static GtkWidget *build_block_content(DisasmLine *dl) {
    GString *formatted = format_block(dl->block[0] ? dl->block : "", COL_MNEM - 1);
    char *escaped = g_markup_escape_text(formatted->str, -1);
    g_string_free(formatted, TRUE);

    /* Indent is baked into every line by format_block — no %*s prefix needed */
    char *markup = g_strdup_printf(
        "<span face='monospace' color='#639922'><i>%s</i></span>", escaped);
    g_free(escaped);

    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    g_free(markup);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_hexpand(lbl, TRUE);
    return lbl;
}

static GtkWidget *build_label_content(DisasmLine *dl) {
    char *escaped = g_markup_escape_text(dl->label, -1);
    char *markup = g_strdup_printf(
        "<span face='monospace'><span color='#EF9F27'>%*s%s:</span></span>",
        COL_MNEM, "", escaped);
    g_free(escaped);

    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    g_free(markup);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_hexpand(lbl, TRUE);
    return lbl;
}

static GtkWidget *build_main_content(DisasmLine *dl, Project *p) {
    /* Column layout (monospace):
     *   0000  C3 74 00     .t.  jp    sub_0074h       ; inline comment
     *
     * For DATA lines with byte_count > 4, continuation rows show:
     *   ....  HH HH HH HH  CCCC  (no addr, no mnemonic)
     */
    int is_data = (dl->rtype == RTYPE_DATA_BYTE || dl->rtype == RTYPE_DATA_STRING
                || dl->rtype == RTYPE_DATA_WORD || dl->rtype == RTYPE_GAP
                || dl->rtype == RTYPE_ORPHAN);

    /* Address */
    char addr_str[8];
    snprintf(addr_str, sizeof(addr_str), "%04X  ", dl->addr);

    /* Mnemonic — 5 chars padded */
    char mnem_str[8];
    strncpy(mnem_str, dl->mnemonic, 5); mnem_str[5] = '\0';
    { int ml = strlen(mnem_str); while (ml < 5) mnem_str[ml++] = ' '; mnem_str[5] = '\0'; }

    /* Operands */
    int ops_col = COL_MNEM + 1 + 5 + 1;
    int ops_len = strlen(dl->operands);
    int cmt_pad = COL_CMT - ops_col - ops_len;

    /* Comment */
    char cmt_str[64] = "";
    if (dl->comment[0])
        snprintf(cmt_str, sizeof(cmt_str), "; %-.*s", CMT_MAX, dl->comment);

    /* Build markup */
    char *addr_esc = g_markup_escape_text(addr_str,  -1);
    char *mnem_esc = g_markup_escape_text(mnem_str,  -1);
    char *ops_esc  = g_markup_escape_text(dl->operands, -1);
    char *cmt_esc  = dl->comment[0] ? g_markup_escape_text(cmt_str, -1) : g_strdup("");

    const char *mc = mnemonic_colour(dl->mnemonic);
    const char *sc = dl->sym_name[0] ? symbol_colour(dl->sym_type) : NULL;

    /* Data colour — dim amber for data directives */
    const char *data_col = "#A0804A";

    GString *markup = g_string_new("<span face='monospace'>");

    /* Helper: emit one row of up to 4 bytes (bytes col + ascii col) */
    /* Returns number of bytes emitted */
    /* We do this inline for first row, then loop for continuations */

    int total = dl->byte_count;
    const unsigned char *rom_ptr = (p && p->rom && dl->offset >= 0
                                    && dl->offset < p->rom_size)
                                   ? (p->rom + dl->offset) : dl->bytes;

    /* First row */
    {
        /* Bytes */
        char bytes_str[16] = "";
        int row_count = total < 4 ? total : 4;
        for (int b = 0; b < row_count; b++) {
            char tmp[4]; sprintf(tmp, "%02X ", rom_ptr[b]); strcat(bytes_str, tmp);
        }
        int blen = strlen(bytes_str);
        while (blen < 12) bytes_str[blen++] = ' ';
        bytes_str[12] = '\0';

        /* ASCII */
        char ascii_str[6] = "     ";
        for (int b = 0; b < row_count; b++) {
            unsigned char c = rom_ptr[b];
            ascii_str[b] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
        }
        ascii_str[4] = ' '; ascii_str[5] = '\0';

        char *bytes_esc = g_markup_escape_text(bytes_str, -1);
        char *ascii_esc = g_markup_escape_text(ascii_str, -1);

        g_string_append_printf(markup, "<span color='#85B7EB'>%s</span>", addr_esc);
        g_string_append_printf(markup, "<span color='#666'>%s</span>", bytes_esc);
        g_string_append_printf(markup, "<span color='#4a7a4a'>%s</span>", ascii_esc);

        if (is_data) {
            g_string_append_printf(markup,
                "<span color='%s' weight='bold'>%s</span> ", data_col, mnem_esc);
            g_string_append_printf(markup,
                "<span color='#ccc'>%s</span>", ops_esc);
        } else {
            if (mc)
                g_string_append_printf(markup,
                    "<span color='%s' weight='bold'>%s</span>", mc, mnem_esc);
            else
                g_string_append_printf(markup,
                    "<span color='#ddd'>%s</span>", mnem_esc);
            g_string_append(markup, " ");
            if (sc)
                g_string_append_printf(markup,
                    "<span color='%s'>%s</span>", sc, ops_esc);
            else
                g_string_append_printf(markup,
                    "<span color='#ccc'>%s</span>", ops_esc);
        }

        if (dl->comment[0]) {
            if (cmt_pad > 0)
                for (int s = 0; s < cmt_pad; s++) g_string_append_c(markup, ' ');
            else
                g_string_append_c(markup, ' ');
            g_string_append_printf(markup,
                "<span color='#639922'>%s</span>", cmt_esc);
        }

        g_free(bytes_esc); g_free(ascii_esc);
    }

    /* Continuation rows for data lines with more than 4 bytes */
    if (is_data && total > 4) {
        /* addr column width is 6, so indent continuation rows to same position */
        char indent[8]; snprintf(indent, sizeof(indent), "%6s", "");
        char *ind_esc = g_markup_escape_text(indent, -1);

        for (int base = 4; base < total; base += 4) {
            int row_count = total - base;
            if (row_count > 4) row_count = 4;

            char bytes_str[16] = "";
            for (int b = 0; b < row_count; b++) {
                char tmp[4]; sprintf(tmp, "%02X ", rom_ptr[base + b]);
                strcat(bytes_str, tmp);
            }
            int blen = strlen(bytes_str);
            while (blen < 12) bytes_str[blen++] = ' ';
            bytes_str[12] = '\0';

            char ascii_str[6] = "     ";
            for (int b = 0; b < row_count; b++) {
                unsigned char c = rom_ptr[base + b];
                ascii_str[b] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
            }
            ascii_str[4] = ' '; ascii_str[5] = '\0';

            char *bytes_esc = g_markup_escape_text(bytes_str, -1);
            char *ascii_esc = g_markup_escape_text(ascii_str, -1);

            g_string_append_printf(markup, "\n%s", ind_esc);
            g_string_append_printf(markup, "<span color='#666'>%s</span>", bytes_esc);
            g_string_append_printf(markup, "<span color='#4a7a4a'>%s</span>", ascii_esc);

            g_free(bytes_esc); g_free(ascii_esc);
        }
        g_free(ind_esc);
    }

    g_string_append(markup, "</span>");
    g_free(addr_esc); g_free(mnem_esc); g_free(ops_esc); g_free(cmt_esc);

    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), markup->str);
    g_string_free(markup, TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_hexpand(lbl, TRUE);
    return lbl;
}

static void row_set_content(GtkListBoxRow *row, GtkWidget *content) {
    gtk_list_box_row_set_child(row, NULL);
    gtk_list_box_row_set_child(row, content);
}

/* ==========================================================================
 * make_row
 * ========================================================================== */

static GtkListBoxRow *make_row(Project *p, int line_index,
                                int is_block, int is_label) {
    DisasmLine *dl = &p->lines[line_index];
    GtkWidget *content = is_block ? build_block_content(dl)
                       : is_label ? build_label_content(dl)
                       :            build_main_content(dl, p);

    GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
    gtk_list_box_row_set_child(row, content);

    g_object_set_data(G_OBJECT(row), "line-index", GINT_TO_POINTER(line_index));
    g_object_set_data(G_OBJECT(row), "row-type",
                      GINT_TO_POINTER(is_block ? ROW_TYPE_BLOCK
                                     : is_label ? ROW_TYPE_LABEL
                                     : ROW_TYPE_MAIN));

    if (!is_block && !is_label) {
        RowData *rd = g_new0(RowData, 1);
        rd->line_index = line_index;
        rd->project    = p;
        g_object_set_data_full(G_OBJECT(row), "rd", rd, (GDestroyNotify)g_free);
    } else {
        gtk_list_box_row_set_selectable(row, FALSE);
        gtk_list_box_row_set_activatable(row, FALSE);
    }

    return row;
}

/* ==========================================================================
 * ui_listing_refresh_line
 *
 * Row order: [block] [label] [main]
 * ========================================================================== */

void ui_listing_refresh_line(GtkWidget *outer, int line_index) {
    GtkWidget *list_box = g_object_get_data(G_OBJECT(outer), "list-box");
    if (!list_box) return;

    Project *p = NULL;
    GtkListBoxRow *main_row  = NULL;
    GtkListBoxRow *block_row = NULL;
    GtkListBoxRow *label_row = NULL;

    int i = 0;
    while (TRUE) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(
            GTK_LIST_BOX(list_box), i);
        if (!row) break;

        int li = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "line-index"));
        int rt = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "row-type"));

        if (li == line_index) {
            if (rt == ROW_TYPE_MAIN)  main_row  = row;
            if (rt == ROW_TYPE_BLOCK) block_row = row;
            if (rt == ROW_TYPE_LABEL) label_row = row;
            RowData *rd = g_object_get_data(G_OBJECT(row), "rd");
            if (rd) p = rd->project;
        }
        i++;
    }

    if (!main_row || !p) return;
    DisasmLine *dl = &p->lines[line_index];

    /* Rebuild main row */
    row_set_content(main_row, build_main_content(dl, p));

    /* ---- Block row: must be ABOVE label row ----
     * Insert position = just before main row, then label row goes between. */
    int main_idx = gtk_list_box_row_get_index(main_row);

    if (dl->block[0]) {
        if (block_row) {
            row_set_content(block_row, build_block_content(dl));
        } else {
            /* Insert before label (or before main if no label) */
            int insert_at = main_idx;
            if (label_row)
                insert_at = gtk_list_box_row_get_index(label_row);
            GtkListBoxRow *nb = make_row(p, line_index, 1, 0);
            gtk_list_box_insert(GTK_LIST_BOX(list_box),
                                GTK_WIDGET(nb), insert_at);
        }
    } else if (block_row) {
        gtk_list_box_remove(GTK_LIST_BOX(list_box), GTK_WIDGET(block_row));
    }

    /* ---- Label row: between block and main ---- */
    /* Recompute main_idx after possible block insertion */
    main_idx = gtk_list_box_row_get_index(main_row);

    if (dl->label[0]) {
        if (label_row) {
            row_set_content(label_row, build_label_content(dl));
        } else {
            /* Insert immediately before main row */
            GtkListBoxRow *nl = make_row(p, line_index, 0, 1);
            gtk_list_box_insert(GTK_LIST_BOX(list_box),
                                GTK_WIDGET(nl), main_idx);
        }
    } else if (label_row) {
        gtk_list_box_remove(GTK_LIST_BOX(list_box), GTK_WIDGET(label_row));
    }
}

gboolean ui_listing_select_address(GtkWidget *outer, int addr) {
    GtkWidget *list_box = g_object_get_data(G_OBJECT(outer), "list-box");
    GtkWidget *scrolled = g_object_get_data(G_OBJECT(outer), "list-scrolled");
    if (!list_box) return FALSE;

    GtkListBoxRow *best_main = NULL;
    GtkListBoxRow *best_any = NULL;
    int i = 0;
    while (TRUE) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_box), i);
        if (!row) break;
        RowData *rd = g_object_get_data(G_OBJECT(row), "rd");
        if (rd && rd->project && rd->line_index >= 0) {
            DisasmLine *dl = &rd->project->lines[rd->line_index];
            if (dl->addr == addr) {
                if (!best_any)
                    best_any = row;
                if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "row-type")) == ROW_TYPE_MAIN) {
                    best_main = row;
                    break;
                }
            }
        }
        i++;
    }

    GtkListBoxRow *best = best_main ? best_main : best_any;
    if (!best) return FALSE;
    gtk_list_box_unselect_all(GTK_LIST_BOX(list_box));
    gtk_list_box_select_row(GTK_LIST_BOX(list_box), best);
    gtk_widget_grab_focus(GTK_WIDGET(best));

    if (!scrolled) return TRUE;

    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
    graphene_rect_t bounds;
    if (!vadj) return FALSE;
    if (!gtk_widget_get_realized(scrolled)) return FALSE;
    if (!gtk_widget_compute_bounds(GTK_WIDGET(best), GTK_WIDGET(list_box), &bounds))
        return FALSE;

    double top = bounds.origin.y;
    double bottom = top + bounds.size.height;
    double value = gtk_adjustment_get_value(vadj);
    double page = gtk_adjustment_get_page_size(vadj);
    double lower = gtk_adjustment_get_lower(vadj);
    double upper = gtk_adjustment_get_upper(vadj);
    if (page <= 0.0 || bounds.size.height <= 0.0)
        return FALSE;

    /* Keep selected row comfortably visible and avoid "selected but off-screen". */
    if (top < value || bottom > value + page) {
        double target = top - (page * 0.30);
        double max_value = upper - page;
        if (max_value < lower) max_value = lower;
        if (target < lower) target = lower;
        if (target > max_value) target = max_value;
        gtk_adjustment_set_value(vadj, target);
    }

    value = gtk_adjustment_get_value(vadj);
    return (top >= value && bottom <= value + page);
}

/* ==========================================================================
 * ListingCtx
 * ========================================================================== */

typedef struct {
    Project *project;
    GtkWidget *list_box;
    GtkWidget *scrolled;
    GtkWidget *panels;
    int        selected_row;
    gulong     panels_destroy_handler;
    UIListingVisibleAddrFn visible_addr_cb;
    gpointer               visible_addr_data;
    UIListingFocusSearchFn focus_search_cb;
    gpointer               focus_search_data;
    int                    last_visible_addr;
} ListingCtx;

static void listing_ctx_free(gpointer p) { g_free(p); }

static void on_panels_destroyed(GtkWidget *panels, gpointer ud) {
    ListingCtx *ctx = (ListingCtx *)ud;
    ctx->panels = NULL;
    ctx->panels_destroy_handler = 0;
}

static void bind_panels(ListingCtx *ctx, GtkWidget *panels) {
    if (!ctx) return;
    if (ctx->panels && ctx->panels_destroy_handler) {
        g_signal_handler_disconnect(ctx->panels, ctx->panels_destroy_handler);
    }
    ctx->panels = panels;
    ctx->panels_destroy_handler = 0;
    if (panels) {
        ctx->panels_destroy_handler =
            g_signal_connect(panels, "destroy", G_CALLBACK(on_panels_destroyed), ctx);
    }
}

static void on_row_selected(GtkListBox *lb, GtkListBoxRow *row, gpointer ud) {
    ListingCtx *ctx = ud;
    if (!row) {
        ctx->selected_row = -1;
        return;
    }
    if (!ctx->panels || !gtk_list_box_row_is_selected(row)) return;
    RowData *rd = g_object_get_data(G_OBJECT(row), "rd");
    if (!rd) return;
    ui_panels_clear_selection(ctx->panels);
    ui_panels_update_selection(ctx->panels,
                               &rd->project->lines[rd->line_index],
                               rd->line_index);
    ctx->selected_row = gtk_list_box_row_get_index(row);
}

static void listing_emit_visible_addr(ListingCtx *ctx) {
    if (!ctx || !ctx->visible_addr_cb || !ctx->project || !ctx->scrolled || !ctx->list_box)
        return;
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(ctx->scrolled));
    if (!vadj) return;

    int y = (int)gtk_adjustment_get_value(vadj);
    GtkListBoxRow *row = gtk_list_box_get_row_at_y(GTK_LIST_BOX(ctx->list_box), y);
    if (!row)
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(ctx->list_box), 0);
    if (!row) return;

    int line_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "line-index"));
    if (line_index < 0 || line_index >= ctx->project->nlines)
        return;
    int addr = ctx->project->lines[line_index].addr;
    if (addr == ctx->last_visible_addr)
        return;
    ctx->last_visible_addr = addr;
    ctx->visible_addr_cb(ctx->visible_addr_data, addr);
}

static void on_listing_scroll_value_changed(GtkAdjustment *adj, gpointer ud) {
    (void)adj;
    listing_emit_visible_addr((ListingCtx *)ud);
}

/* ==========================================================================
 * Search
 * ========================================================================== */

static gboolean do_search_text(ListingCtx *ctx, const char *text, UISearchMode mode) {
    if (!ctx || !text || !text[0]) return FALSE;
    int start = ctx->selected_row;
    int idx   = start + 1;

    while (TRUE) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(
            GTK_LIST_BOX(ctx->list_box), idx);
        if (!row) {
            if (idx == 0) break;
            idx = 0;
            row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(ctx->list_box), 0);
            if (!row) break;
        }
        RowData *rd = g_object_get_data(G_OBJECT(row), "rd");
        if (rd) {
            DisasmLine *dl = &rd->project->lines[rd->line_index];
            gboolean match = FALSE;
            if      (mode == UI_SEARCH_ADDR) {
                int a = (int)strtol(text, NULL, 0);
                match = (dl->addr == a);
            } else if (mode == UI_SEARCH_MNEM) {
                match = (g_ascii_strcasecmp(dl->mnemonic, text) == 0);
            } else {
                match = (dl->comment[0] && strstr(dl->comment, text)) ||
                        (dl->block[0]   && strstr(dl->block,   text));
            }
            if (match) {
                gtk_list_box_select_row(GTK_LIST_BOX(ctx->list_box), row);
                gtk_widget_grab_focus(GTK_WIDGET(row));
                ctx->selected_row = idx;
                return TRUE;
            }
        }
        idx++;
        if (start >= 0 && idx > start) break;
        if (start <  0 && idx > 50000) break;
    }
    return FALSE;
}

static void on_listing_click_pressed(GtkGestureClick *gesture, int n_press,
                                     double x, double y, gpointer ud) {
    (void)gesture;
    (void)n_press;
    (void)x;
    (void)y;
    ListingCtx *ctx = ud;
    if (ctx && ctx->panels)
        ui_panels_clear_selection(ctx->panels);
}

static gboolean on_key_pressed(GtkEventControllerKey *c, guint kv,
                                guint kc, GdkModifierType st, gpointer ud) {
    (void)kc;
    ListingCtx *ctx = ud;
    if ((kv == GDK_KEY_s || kv == GDK_KEY_S) && (st & GDK_CONTROL_MASK)) {
        if (ctx->focus_search_cb)
            ctx->focus_search_cb(ctx->focus_search_data);
        return TRUE;
    }
    if (kv == GDK_KEY_Escape) {
        gtk_list_box_unselect_all(GTK_LIST_BOX(ctx->list_box));
        if (ctx->panels)
            ui_panels_clear_selection(ctx->panels);
        ctx->selected_row = -1;
        return TRUE;
    }
    return FALSE;
}

/* ==========================================================================
 * Setters called by main.c after construction
 * ========================================================================== */

void ui_listing_set_panels(GtkWidget *outer, GtkWidget *panels) {
    ListingCtx *ctx = g_object_get_data(G_OBJECT(outer), "listing-ctx");
    bind_panels(ctx, panels);
}

void ui_listing_set_visible_addr_cb(GtkWidget *outer,
                                    UIListingVisibleAddrFn cb,
                                    gpointer data) {
    ListingCtx *ctx = g_object_get_data(G_OBJECT(outer), "listing-ctx");
    if (!ctx) return;
    ctx->visible_addr_cb = cb;
    ctx->visible_addr_data = data;
    listing_emit_visible_addr(ctx);
}

void ui_listing_set_search_focus_cb(GtkWidget *outer,
                                    UIListingFocusSearchFn cb,
                                    gpointer data) {
    ListingCtx *ctx = g_object_get_data(G_OBJECT(outer), "listing-ctx");
    if (!ctx) return;
    ctx->focus_search_cb = cb;
    ctx->focus_search_data = data;
}

gboolean ui_listing_search_next(GtkWidget *outer, const char *text, UISearchMode mode) {
    ListingCtx *ctx = g_object_get_data(G_OBJECT(outer), "listing-ctx");
    if (!ctx) return FALSE;
    return do_search_text(ctx, text, mode);
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

GtkWidget *ui_listing_new(Project *p, GtkWidget *panels) {
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(outer, TRUE);

    /* List box */
    GtkWidget *list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box), GTK_SELECTION_SINGLE);

    for (int i = 0; i < p->nlines; i++) {
        DisasmLine *dl = &p->lines[i];
        /* Order: block → label → main */
        if (dl->block[0])
            gtk_list_box_append(GTK_LIST_BOX(list_box),
                                GTK_WIDGET(make_row(p, i, 1, 0)));
        if (dl->label[0])
            gtk_list_box_append(GTK_LIST_BOX(list_box),
                                GTK_WIDGET(make_row(p, i, 0, 1)));
        gtk_list_box_append(GTK_LIST_BOX(list_box),
                            GTK_WIDGET(make_row(p, i, 0, 0)));
    }

    /* Horizontal scroll so long lines can be seen */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list_box);
    gtk_box_append(GTK_BOX(outer), scrolled);

    g_object_set_data(G_OBJECT(outer), "list-box", list_box);
    g_object_set_data(G_OBJECT(outer), "list-scrolled", scrolled);

    ListingCtx *ctx      = g_new0(ListingCtx, 1);
    ctx->project         = p;
    ctx->list_box        = list_box;
    ctx->scrolled        = scrolled;
    ctx->panels          = panels;
    ctx->selected_row    = -1;
    ctx->panels_destroy_handler = 0;
    ctx->last_visible_addr = -1;

    g_object_set_data_full(G_OBJECT(outer), "listing-ctx", ctx, listing_ctx_free);

    bind_panels(ctx, panels);
    g_signal_connect(list_box,  "row-selected", G_CALLBACK(on_row_selected),     ctx);
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
    g_signal_connect(vadj, "value-changed", G_CALLBACK(on_listing_scroll_value_changed), ctx);

    GtkEventController *key = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key, GTK_PHASE_CAPTURE);
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), ctx);
    gtk_widget_add_controller(outer, key);

    GtkGesture *click = gtk_gesture_click_new();
    gtk_widget_add_controller(list_box, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(click, "pressed", G_CALLBACK(on_listing_click_pressed), ctx);

    return outer;
}
