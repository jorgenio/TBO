/*
 * This file is part of TBO, a gnome comic editor
 * Copyright (C) 2010  Daniel Garcia Moreno <dani@danigm.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "ui-menu.h"
#include "ui-toolbar.h"
#include "config.h"
#include "comic-new-dialog.h"
#include "comic-saveas-dialog.h"
#include "comic-open-dialog.h"
#include "tbo-window.h"
#include "comic.h"
#include "custom-stock.h"
#include "ui-drawing.h"

#include "frame-tool.h"
#include "selector-tool.h"
#include "doodle-tool.h"
#include "text-tool.h"
#include "piximage.h"

static int SELECTED_TOOL = NONE;
static GtkActionGroup *ACTION_GROUP = NULL;

static ToolStruct TOOLS[] =
{
    {FRAME,
     frame_tool_on_select,
     frame_tool_on_unselect,
     frame_tool_on_move,
     frame_tool_on_click,
     frame_tool_on_release,
     frame_tool_on_key,
     frame_tool_drawing},

    {SELECTOR,
     selector_tool_on_select,
     selector_tool_on_unselect,
     selector_tool_on_move,
     selector_tool_on_click,
     selector_tool_on_release,
     selector_tool_on_key,
     selector_tool_drawing},

    {DOODLE,
     doodle_tool_on_select,
     doodle_tool_on_unselect,
     doodle_tool_on_move,
     doodle_tool_on_click,
     doodle_tool_on_release,
     doodle_tool_on_key,
     doodle_tool_drawing},

    {BUBBLE,
     doodle_tool_bubble_on_select,
     doodle_tool_bubble_on_unselect,
     doodle_tool_on_move,
     doodle_tool_on_click,
     doodle_tool_on_release,
     doodle_tool_on_key,
     doodle_tool_drawing},

    {TEXT,
     text_tool_on_select,
     text_tool_on_unselect,
     text_tool_on_move,
     text_tool_on_click,
     text_tool_on_release,
     text_tool_on_key,
     text_tool_drawing},
};

typedef struct
{
    enum Tool tool;
    char *action;

} tool_and_action;

void unselect (enum Tool tool, TboWindow *tbo);
gboolean select_tool (GtkAction *action, TboWindow *tbo);
void update_toolbar (TboWindow *tbo);

enum Tool
get_selected_tool ()
{
    return SELECTED_TOOL;
}


void
set_current_tab_page (TboWindow *tbo, gboolean setit)
{
    int nth;

    nth = tbo_comic_page_index (tbo->comic);
    if (setit)
        gtk_notebook_set_current_page (GTK_NOTEBOOK (tbo->notebook), nth);
    tbo->dw_scroll = gtk_notebook_get_nth_page (GTK_NOTEBOOK (tbo->notebook), nth);
    tbo->drawing = gtk_bin_get_child (GTK_BIN (tbo->dw_scroll));
    set_frame_view (NULL);
    set_selected_tool (NONE, tbo);
}

gboolean
notebook_switch_page_cb (GtkNotebook     *notebook,
                         GtkNotebookPage *page,
                         guint            page_num,
                         TboWindow        *tbo)
{
    tbo_comic_set_current_page_nth (tbo->comic, page_num);
    set_current_tab_page (tbo, FALSE);
    update_toolbar (tbo);
    tbo_window_update_status (tbo, 0, 0);
    tbo_drawing_adjust_scroll (tbo);
    return FALSE;
}

void
set_selected_tool (enum Tool tool, TboWindow *tbo)
{
    unselect (SELECTED_TOOL, tbo);
    SELECTED_TOOL = tool;

    tool_signal (tool, TOOL_SELECT, tbo);
    update_toolbar (tbo);
}

void
update_toolbar (TboWindow *tbo)
{
    GtkAction *prev;
    GtkAction *next;
    GtkAction *delete;

    GtkAction *doodle;
    GtkAction *bubble;
    GtkAction *text;
    GtkAction *new_frame;
    GtkAction *pix;

    if (!ACTION_GROUP)
        return;

    // Page next, prev and delete button sensitive
    prev = gtk_action_group_get_action (ACTION_GROUP, "PrevPage");
    next = gtk_action_group_get_action (ACTION_GROUP, "NextPage");
    delete = gtk_action_group_get_action (ACTION_GROUP, "DelPage");

    if (tbo_comic_page_first (tbo->comic))
        gtk_action_set_sensitive (prev, FALSE);
    else
        gtk_action_set_sensitive (prev, TRUE);

    if (tbo_comic_page_last (tbo->comic))
        gtk_action_set_sensitive (next, FALSE);
    else
        gtk_action_set_sensitive (next, TRUE);
    if (tbo_comic_len (tbo->comic) > 1)
        gtk_action_set_sensitive (delete, TRUE);
    else
        gtk_action_set_sensitive (delete, FALSE);

    // Frame view disabled in page view
    doodle = gtk_action_group_get_action (ACTION_GROUP, "Doodle");
    bubble = gtk_action_group_get_action (ACTION_GROUP, "Bubble");
    text = gtk_action_group_get_action (ACTION_GROUP, "Text");
    new_frame = gtk_action_group_get_action (ACTION_GROUP, "NewFrame");
    pix = gtk_action_group_get_action (ACTION_GROUP, "Pix");

    if (get_frame_view() == NULL)
    {
        gtk_action_set_sensitive (doodle, FALSE);
        gtk_action_set_sensitive (bubble, FALSE);
        gtk_action_set_sensitive (text, FALSE);
        gtk_action_set_sensitive (pix, FALSE);
        gtk_action_set_sensitive (new_frame, TRUE);
    }
    else
    {
        gtk_action_set_sensitive (doodle, TRUE);
        gtk_action_set_sensitive (bubble, TRUE);
        gtk_action_set_sensitive (text, TRUE);
        gtk_action_set_sensitive (pix, TRUE);
        gtk_action_set_sensitive (new_frame, FALSE);
    }
    update_menubar (tbo);
}

gboolean
toolbar_handler (GtkWidget *widget, gpointer data)
{
    return FALSE;
}

gboolean
add_new_page (GtkAction *action, TboWindow *tbo)
{
    Page *page = tbo_comic_new_page (tbo->comic);
    int nth = tbo_comic_page_nth (tbo->comic, page);
    gtk_notebook_insert_page (GTK_NOTEBOOK (tbo->notebook),
                              create_darea (tbo),
                              NULL,
                              nth);
    tbo_window_update_status (tbo, 0, 0);
    update_toolbar (tbo);
    return FALSE;
}

gboolean
del_current_page (GtkAction *action, TboWindow *tbo)
{
    int nth = tbo_comic_page_index (tbo->comic);
    tbo_comic_del_current_page (tbo->comic);
    set_current_tab_page (tbo, TRUE);
    gtk_notebook_remove_page (GTK_NOTEBOOK (tbo->notebook), nth);
    tbo_window_update_status (tbo, 0, 0);
    update_toolbar (tbo);
    return FALSE;
}

gboolean
next_page (GtkAction *action, TboWindow *tbo)
{
    tbo_comic_next_page (tbo->comic);
    set_current_tab_page (tbo, TRUE);
    update_toolbar (tbo);
    tbo_window_update_status (tbo, 0, 0);
    tbo_drawing_adjust_scroll (tbo);

    return FALSE;
}

gboolean
prev_page (GtkAction *action, TboWindow *tbo)
{
    tbo_comic_prev_page (tbo->comic);
    set_current_tab_page (tbo, TRUE);
    update_toolbar (tbo);
    tbo_window_update_status (tbo, 0, 0);
    tbo_drawing_adjust_scroll (tbo);

    return FALSE;
}

gboolean
zoom_100 (GtkAction *action, TboWindow *tbo)
{
    tbo_drawing_zoom_100 (tbo);
    return FALSE;
}

gboolean
zoom_fit (GtkAction *action, TboWindow *tbo)
{
    tbo_drawing_zoom_fit (tbo);
    return FALSE;
}

gboolean
zoom_in (GtkAction *action, TboWindow *tbo)
{
    tbo_drawing_zoom_in (tbo);
    return FALSE;
}

gboolean
zoom_out (GtkAction *action, TboWindow *tbo)
{
    tbo_drawing_zoom_out (tbo);
    return FALSE;
}

gboolean
add_pix (GtkAction *action, TboWindow *tbo)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;

    dialog = gtk_file_chooser_dialog_new (_("Add an Image"),
                     GTK_WINDOW (tbo->window),
                     GTK_FILE_CHOOSER_ACTION_OPEN,
                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                     NULL);

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("png"));
    gtk_file_filter_add_pattern (filter, "*.png");
    gtk_file_filter_add_pattern (filter, "*.PNG");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("All files"));
    gtk_file_filter_add_pattern (filter, "*");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        char *filename;
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        PIXImage *piximage = tbo_piximage_new_with_params (0, 0, 0, 0, filename);
        tbo_frame_add_obj (get_frame_view(), piximage);
        update_drawing (tbo);
        g_free (filename);
    }

    gtk_widget_destroy (dialog);
    return FALSE;
}

static const GtkActionEntry tbo_tools_entries [] = {
    { "NewFileTool", GTK_STOCK_NEW, N_("_New"), "<control>N",
      N_("New Comic"),
      G_CALLBACK (tbo_comic_new_dialog) },

    { "OpenFileTool", GTK_STOCK_OPEN, N_("_Open"), "<control>O",
      N_("Open comic"),
      G_CALLBACK (tbo_comic_open_dialog) },

    { "SaveFileTool", GTK_STOCK_SAVE, N_("_Save"), "<control>S",
      N_("Save current document"),
      G_CALLBACK (tbo_comic_save_dialog) },

    // Page tools
    { "NewPage", GTK_STOCK_ADD, N_("New Page"), "<control>P",
      N_("New page"),
      G_CALLBACK (add_new_page) },

    { "DelPage", GTK_STOCK_DELETE, N_("Delete Page"), "",
      N_("Delete current page"),
      G_CALLBACK (del_current_page) },

    { "PrevPage", GTK_STOCK_GO_BACK, N_("Prev Page"), "",
      N_("Prev page"),
      G_CALLBACK (prev_page) },

    { "NextPage", GTK_STOCK_GO_FORWARD, N_("Next Page"), "",
      N_("Next page"),
      G_CALLBACK (next_page) },

    // Zoom tools
    { "Zoomin", GTK_STOCK_ZOOM_IN, N_("Zoom in"), "",
      N_("Zoom in"),
      G_CALLBACK (zoom_in) },
    { "Zoom100", GTK_STOCK_ZOOM_100, N_("Zoom 1:1"), "",
      N_("Zoom 1:1"),
      G_CALLBACK (zoom_100) },
    { "Zoomfit", GTK_STOCK_ZOOM_FIT, N_("Zoom fit"), "",
      N_("Zoom fit"),
      G_CALLBACK (zoom_fit) },
    { "Zoomout", GTK_STOCK_ZOOM_OUT, N_("Zoom out"), "",
      N_("Zoom out"),
      G_CALLBACK (zoom_out) },

    // Png image tool
    { "Pix", TBO_STOCK_PIX, N_("Image"), "",
      N_("Image"),
      G_CALLBACK (add_pix) },
};

static const GtkToggleActionEntry tbo_tools_toogle_entries [] = {
    // Page view tools
    { "NewFrame", TBO_STOCK_FRAME, N_("New _Frame"), "f",
      N_("New Frame"),
      G_CALLBACK (select_tool), FALSE },

    { "Selector", TBO_STOCK_SELECTOR, N_("Selector"), "s",
      N_("Selector"),
      G_CALLBACK (select_tool), FALSE },

    // Frame view tools
    { "Doodle", TBO_STOCK_DOODLE, N_("Doodle"), "d",
      N_("Doodle"),
      G_CALLBACK (select_tool), FALSE },
    { "Bubble", TBO_STOCK_BUBBLE, N_("Booble"), "b",
      N_("Bubble"),
      G_CALLBACK (select_tool), FALSE },
    { "Text", TBO_STOCK_TEXT, N_("Text"), "t",
      N_("Text"),
      G_CALLBACK (select_tool), FALSE },
};

static const tool_and_action tools_actions [] = {
    {FRAME, "NewFrame"},
    {SELECTOR, "Selector"},
    {DOODLE, "Doodle"},
    {BUBBLE, "Bubble"},
    {TEXT, "Text"},
};

void
set_selected_tool_and_action (enum Tool tool, TboWindow *tbo)
{
    GtkToggleAction *action;
    enum Tool action_tool;
    gchar *name;

    int i;
    GtkToggleActionEntry entry;

    for (i=0; i<G_N_ELEMENTS (tools_actions); i++)
    {
        if (tool == tools_actions[i].tool)
        {
            name = (gchar *) tools_actions[i].action;
            break;
        }
    }

    action = (GtkToggleAction *) gtk_action_group_get_action (ACTION_GROUP, name);
    if (gtk_action_is_sensitive (GTK_ACTION (action)))
        gtk_toggle_action_set_active (action, TRUE);
}

void
unselect (enum Tool tool, TboWindow *tbo)
{
    int i;
    GtkToggleAction *action;

    for (i=0; i<G_N_ELEMENTS (tools_actions); i++)
    {
        if (tools_actions[i].tool == tool)
        {
            action = (GtkToggleAction *) gtk_action_group_get_action (ACTION_GROUP,
                    tools_actions[i].action);

            gtk_toggle_action_set_active (action, FALSE);
            break;
        }
    }
    tool_signal (tool, TOOL_UNSELECT, tbo);
}

gboolean
select_tool (GtkAction *action, TboWindow *tbo)
{
    GtkToggleAction *toggle_action;
    int i;
    const gchar *name;
    enum Tool tool;

    toggle_action = (GtkToggleAction *) action;
    name = gtk_action_get_name (action);


    for (i=0; i<G_N_ELEMENTS (tools_actions); i++)
    {
        if (strcmp (tools_actions[i].action, name) == 0)
        {
            tool = tools_actions[i].tool;
            break;
        }
    }

    if (gtk_toggle_action_get_active (toggle_action))
        set_selected_tool (tool, tbo);
    else
        set_selected_tool (NONE, tbo);
    tbo_window_update_status (tbo, 0, 0);
    return FALSE;
}

GtkWidget *generate_toolbar (TboWindow *window){
    GtkWidget *toolbar;
    GtkUIManager *manager;
    GError *error = NULL;

    manager = gtk_ui_manager_new ();
    gtk_ui_manager_add_ui_from_file (manager, DATA_DIR "/ui/tbo-toolbar-ui.xml", &error);
    if (error != NULL)
    {
        g_warning ("Could not merge tbo-toolbar-ui.xml: %s", error->message);
        g_error_free (error);
    }

    ACTION_GROUP = gtk_action_group_new ("ToolsActions");
    gtk_action_group_set_translation_domain (ACTION_GROUP, NULL);
    gtk_action_group_add_actions (ACTION_GROUP, tbo_tools_entries,
                        G_N_ELEMENTS (tbo_tools_entries), window);
    gtk_action_group_add_toggle_actions (ACTION_GROUP, tbo_tools_toogle_entries,
                        G_N_ELEMENTS (tbo_tools_toogle_entries), window);

    gtk_ui_manager_insert_action_group (manager, ACTION_GROUP, 0);

    toolbar = gtk_ui_manager_get_widget (manager, "/toolbar");

    update_toolbar (window);

    return toolbar;
}

void
tool_signal (enum Tool tool, enum ToolSignal signal, gpointer data)
{
    int i;
    ToolStruct *toolstruct = NULL;
    void **pdata;

    for (i=0; i<G_N_ELEMENTS (TOOLS); i++)
    {
        if (tool == TOOLS[i].tool)
        {
            toolstruct = &TOOLS[i];
            break;
        }
    }

    if (toolstruct)
    {
        switch (signal)
        {
            case TOOL_SELECT:
                toolstruct->tool_on_select(data);
                break;
            case TOOL_UNSELECT:
                toolstruct->tool_on_unselect(data);
                break;
            case TOOL_MOVE:
                pdata = data;
                toolstruct->tool_on_move (pdata[0], pdata[1], pdata[2]);
                break;
            case TOOL_CLICK:
                pdata = data;
                toolstruct->tool_on_click (pdata[0], pdata[1], pdata[2]);
                break;
            case TOOL_RELEASE:
                pdata = data;
                toolstruct->tool_on_release (pdata[0], pdata[1], pdata[2]);
                break;
            case TOOL_KEY:
                pdata = data;
                toolstruct->tool_on_key (pdata[0], pdata[1], pdata[2]);
                break;
            case TOOL_DRAWING:
                toolstruct->tool_drawing (data);
                break;
            default:
                break;
        }
    }
}
