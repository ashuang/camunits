#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#include <camunits/dbg.h>
#include "unit_control_widget.h"

#define err(args...) fprintf(stderr, args)

static void cam_unit_control_widget_finalize(GObject *obj);

static void on_output_formats_changed(CamUnit *unit, 
        CamUnitControlWidget *self);
static void on_close_button_clicked(GtkButton *bt, CamUnitControlWidget *self);
static void on_expander_notify(GtkWidget*widget, GParamSpec *param, 
        CamUnitControlWidget *self);
static void on_status_changed(CamUnit *unit, CamUnitControlWidget *self);
static void on_control_value_changed(CamUnit *unit, CamUnitControl *ctl, 
        CamUnitControlWidget *self);
static void on_formats_combo_changed(GtkComboBox *combo, 
        CamUnitControlWidget *self);

typedef struct _CamUnitControlWidgetPriv CamUnitControlWidgetPriv;
struct _CamUnitControlWidgetPriv {
    CamUnit *unit;

    /*< private >*/
    GtkAlignment *alignment;
    GtkExpander *expander;
    GtkButton *close_button;
    GtkTable *table;
    GtkWidget * arrow_bin;
    GtkWidget * exp_label;
    GtkTooltips * tooltips;

    GtkComboBox *formats_combo;
    int formats_combo_nentries;

    int trows;
    GHashTable *ctl_info;

    int status_changed_handler_id;
    int formats_changed_handler_id;
};
#define CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_TYPE_UNIT_CONTROL_WIDGET, CamUnitControlWidgetPriv))

enum {
    CLOSE_BUTTON_CLICKED_SIGNAL,
    LAST_SIGNAL
};

static guint unit_control_widget_signals[LAST_SIGNAL] = { 0 };

GtkTargetEntry cam_unit_control_widget_target_entry = {
    .target = "UnitControlWidget",
    .flags = GTK_TARGET_SAME_APP,
    .info = CAM_UNIT_CONTROL_WIDGET_DND_ID
};

typedef struct _ControlWidgetInfo {
    GtkWidget *widget;
    GtkWidget *file_chooser_bt;
    GtkWidget *labelval;
    GtkWidget *label;
    GtkWidget *button;
    int maxchars;
    CamUnitControl *ctl;
    int use_int;
} ControlWidgetInfo;

G_DEFINE_TYPE (CamUnitControlWidget, cam_unit_control_widget, 
        GTK_TYPE_EVENT_BOX);

static void
cam_unit_control_widget_init(CamUnitControlWidget *self)
{
    dbg(DBG_GUI, "unit control widget constructor\n");
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    
    gtk_drag_source_set (GTK_WIDGET(self), GDK_BUTTON1_MASK,
            &cam_unit_control_widget_target_entry, 1, GDK_ACTION_PRIVATE);

    // vbox for everything
    GtkWidget * vbox_outer = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (self), vbox_outer);
    gtk_widget_show (vbox_outer);

    // frame for controls everything
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
    gtk_box_pack_start (GTK_BOX (vbox_outer), frame, FALSE, FALSE, 0);
    gtk_widget_show(frame);

    // vbox for rows
    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);
    gtk_widget_show(vbox);

    // box for expander and close button
    GtkWidget * hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    // alignment widget to contain the expander
    priv->alignment = GTK_ALIGNMENT(gtk_alignment_new(0, 0.5, 1, 0));
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(priv->alignment), 
            FALSE, FALSE, 0);
    gtk_widget_show(GTK_WIDGET(priv->alignment));

    // expander
    priv->expander = GTK_EXPANDER(gtk_expander_new(NULL));
    g_signal_connect(G_OBJECT(priv->expander), "notify::expanded",
            G_CALLBACK(on_expander_notify), self);
    gtk_container_add(GTK_CONTAINER(priv->alignment), 
            GTK_WIDGET(priv->expander));
    //gtk_expander_set_expanded(priv->expander, FALSE);
    gtk_widget_show(GTK_WIDGET(priv->expander));

    priv->exp_label = gtk_label_new ("blah");
    gtk_misc_set_alignment (GTK_MISC (priv->exp_label), 0, 0.5);
    gtk_label_set_ellipsize (GTK_LABEL (priv->exp_label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (priv->exp_label),
            TRUE, TRUE, 0);
    gtk_widget_show (GTK_WIDGET (priv->exp_label));
    
    // close button
    priv->close_button = GTK_BUTTON(gtk_button_new ());
    GtkWidget * image = gtk_image_new_from_stock (GTK_STOCK_CLOSE,
            GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (priv->close_button, image);
    gtk_button_set_relief (priv->close_button, GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click (priv->close_button, FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(priv->close_button), 
            FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (priv->close_button), "clicked",
            G_CALLBACK (on_close_button_clicked), self);
    gtk_widget_show (GTK_WIDGET(priv->close_button));

    // table for all the widgets
    priv->table = GTK_TABLE(gtk_table_new(1, 3, FALSE));
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(priv->table), 
            FALSE, FALSE, 0);
    //gtk_widget_show(GTK_WIDGET(priv->table));
    priv->trows = 0;

//    // arrow bin for the output format combo box
//    priv->arrow_bin = gtk_arrow_bin_new ();
//    gtk_box_pack_start (GTK_BOX (vbox_outer), priv->arrow_bin,
//            FALSE, FALSE, 0);
//    gtk_widget_show (priv->arrow_bin);
    // output formats selector
    priv->formats_combo = GTK_COMBO_BOX(gtk_combo_box_new_text());
    GtkWidget *fmtlabel = gtk_label_new("Format:");
    gtk_misc_set_alignment (GTK_MISC (fmtlabel), 1, 0.5);

    GtkWidget * fhbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (fhbox), fmtlabel, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (fhbox), GTK_WIDGET (priv->formats_combo),
            TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), fhbox, FALSE, FALSE, 0);
    gtk_widget_show (fmtlabel);
    gtk_widget_show (GTK_WIDGET (priv->formats_combo));
    gtk_widget_show (fhbox);

    priv->ctl_info = g_hash_table_new_full(NULL, NULL, NULL, free);

    priv->status_changed_handler_id = 0;
    priv->formats_changed_handler_id = 0;

    priv->unit = NULL;

    priv->tooltips = gtk_tooltips_new ();
    gtk_tooltips_enable (priv->tooltips);
    g_object_ref (priv->tooltips);
}

static void
cam_unit_control_widget_class_init(CamUnitControlWidgetClass *klass)
{
    dbg(DBG_GUI, "unit control widget class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    // add a class-specific destructor
    gobject_class->finalize = cam_unit_control_widget_finalize;

    unit_control_widget_signals[CLOSE_BUTTON_CLICKED_SIGNAL] = 
        g_signal_new("close-button-clicked",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    g_type_class_add_private (gobject_class, sizeof (CamUnitControlWidgetPriv));
}

// destructor (more or less)
static void
cam_unit_control_widget_finalize(GObject *obj)
{
    CamUnitControlWidget *self = CAM_UNIT_CONTROL_WIDGET(obj);
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    dbg(DBG_GUI, "unit control widget finalize (%s)\n",
            priv->unit ?  cam_unit_get_id(priv->unit) : NULL);

    g_hash_table_destroy(priv->ctl_info);
    priv->ctl_info = NULL;

    cam_unit_control_widget_detach(self);
    g_object_unref (priv->tooltips);

    G_OBJECT_CLASS (cam_unit_control_widget_parent_class)->finalize(obj);
}

CamUnitControlWidget *
cam_unit_control_widget_new(CamUnit *unit)
{
    CamUnitControlWidget * self = 
        CAM_UNIT_CONTROL_WIDGET(
                g_object_new(CAM_TYPE_UNIT_CONTROL_WIDGET, NULL));
    cam_unit_control_widget_set_unit(self, unit);
    return self;
}

static int
num_chars (int n)
{
    int i = 0;
    if (n <= 0) { i++; n = -n; }
    while (n != 0) { n /= 10; i++; }
    return i;
}

static void
set_slider_label(CamUnitControlWidget *self, ControlWidgetInfo *ci)
{
    if (ci->labelval) {
        char *fmt = cam_unit_control_get_display_format(ci->ctl);
        char *str = NULL;
        if (!ci->use_int) {
            str = g_strdup_printf(fmt, 
                    gtk_range_get_value(GTK_RANGE(ci->widget)));
        } else {
            str = g_strdup_printf(fmt, 
                    (int) gtk_range_get_value (GTK_RANGE(ci->widget)));
        }
        gtk_label_set_text (GTK_LABEL(ci->labelval), str);
        g_free(str);
        g_free(fmt);
    }
}

static void
on_slider_changed(GtkRange *range, CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(range), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;

    CamUnitControlType ctl_type = cam_unit_control_get_control_type(ctl);
    if (ctl_type == CAM_UNIT_CONTROL_TYPE_INT) {
        int newval = (int) gtk_range_get_value (range);
        int oldval = cam_unit_control_get_int(ctl);

        if(oldval != newval) {
            cam_unit_control_try_set_int(ctl, newval);
        }

        // was the control successfully set?
        oldval = cam_unit_control_get_int(ctl);

        if(oldval != newval) {
            gtk_range_set_value (GTK_RANGE (range), oldval);
            set_slider_label (self, ci);
        }
    }
    else if (ctl_type == CAM_UNIT_CONTROL_TYPE_FLOAT) {
        float newval = gtk_range_get_value (range);
        float oldval = cam_unit_control_get_float(ctl);

        if(oldval != newval) {
            cam_unit_control_try_set_float(ctl, newval);
        }

        // was the control successfully set?
        oldval = cam_unit_control_get_float(ctl);

        if(oldval != newval) {
            gtk_range_set_value (GTK_RANGE (range), oldval);
            set_slider_label (self, ci);
        }
    }
}

static void
control_set_sensitive (ControlWidgetInfo * ci)
{
    CamUnitControlType ctl_type = cam_unit_control_get_control_type(ci->ctl);
    int enabled = cam_unit_control_get_enabled(ci->ctl);
    switch(ctl_type) {
        case CAM_UNIT_CONTROL_TYPE_INT:
        case CAM_UNIT_CONTROL_TYPE_BOOLEAN:
        case CAM_UNIT_CONTROL_TYPE_FLOAT:
        case CAM_UNIT_CONTROL_TYPE_ENUM:
            gtk_widget_set_sensitive(ci->widget, enabled);
            if (ci->label)
                gtk_widget_set_sensitive(ci->label, enabled);
            if (ci->labelval)
                gtk_widget_set_sensitive(ci->labelval, enabled);
            break;
        case CAM_UNIT_CONTROL_TYPE_STRING:
            gtk_widget_set_sensitive(ci->widget, enabled);

            if (cam_unit_control_get_ui_hints(ci->ctl) & 
                    CAM_UNIT_CONTROL_FILENAME) {
                gtk_widget_set_sensitive(ci->file_chooser_bt,
                        enabled);
            } else if(ci->button) {
                gtk_widget_set_sensitive(ci->button, enabled);
            }
            break;
        default:
            err("UnitControlWidget:  unrecognized control type %d\n",
                    ctl_type);
            break;
    }
}

static void
set_tooltip (CamUnitControlWidget * self, GtkWidget * widget,
        CamUnitControl * ctl)
{
    char str[256];
    snprintf (str, sizeof (str), "ID: %s\nType: %s", 
            cam_unit_control_get_id(ctl),
            cam_unit_control_get_control_type_str (ctl));
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    gtk_tooltips_set_tip (priv->tooltips, widget, str, str);
}

static void
add_slider (CamUnitControlWidget * self,
        float min, float max, float step, float initial_value,
        int use_int, CamUnitControl * ctl)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    // label
    char *tmp = g_strjoin("", cam_unit_control_get_name(ctl), ":", NULL);
    GtkWidget *label = gtk_label_new(tmp);
    free(tmp);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
    gtk_table_attach (priv->table, label, 0, 1,
            priv->trows, priv->trows+1,
            GTK_FILL, 0, 0, 0);
    gtk_widget_show (label);
    GtkWidget *range = gtk_hscale_new_with_range(min, max, step);
    gtk_scale_set_draw_value(GTK_SCALE(range), FALSE);
    set_tooltip (self, range, ctl);

    // slider widget

    /* This is a hack to use always round the HScale to integer
     * values.  Strangely, this functionality is normally only
     * available when draw_value is TRUE. */
    if (use_int)
        GTK_RANGE (range)->round_digits = 0;

    gtk_table_attach (priv->table, range, 1, 2,
            priv->trows, priv->trows+1,
            GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
    gtk_widget_show (range);
    gtk_range_set_value (GTK_RANGE (range), initial_value);

    // numerical label
    GtkWidget *labelval = gtk_label_new (NULL);
    gtk_table_attach (priv->table, labelval, 2, 3,
            priv->trows, priv->trows+1,
            GTK_FILL, 0, 0, 0);
    PangoFontDescription * desc = pango_font_description_new ();
    pango_font_description_set_family_static (desc, "monospace");
    gtk_widget_modify_font (labelval, desc);
    gtk_misc_set_alignment (GTK_MISC(labelval), 1, 0.5);
    gtk_widget_show (labelval);

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) calloc(1, sizeof(ControlWidgetInfo));
    g_object_set_data(G_OBJECT(range), "ControlWidgetInfo", ci);
    ci->widget = range;
    ci->labelval = labelval;
    ci->label = label;
    ci->maxchars = MAX (num_chars (min), num_chars (max));
    ci->ctl = ctl;
    ci->use_int = use_int;
    g_hash_table_insert(priv->ctl_info, ctl, ci);
    control_set_sensitive (ci);

    set_slider_label (self, ci);
    g_signal_connect (G_OBJECT (range), "value-changed",
            G_CALLBACK (on_slider_changed), self);

    priv->trows++;
}

static void
on_spin_button_changed(GtkSpinButton *spinb, CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(spinb), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;

    CamUnitControlType ctl_type = cam_unit_control_get_control_type(ci->ctl);
    if (ctl_type == CAM_UNIT_CONTROL_TYPE_INT) {
        int newval = (int) gtk_spin_button_get_value (spinb);
        int oldval = cam_unit_control_get_int(ctl);

        if(oldval != newval) {
            cam_unit_control_try_set_int(ctl, newval);
        }

        // was the control successfully set?
        oldval = cam_unit_control_get_int(ctl);

        if(oldval != newval) {
            gtk_spin_button_set_value (spinb, oldval);
        }
    }
    else if (ctl_type == CAM_UNIT_CONTROL_TYPE_FLOAT) {
        float newval = gtk_spin_button_get_value (spinb);
        float oldval = cam_unit_control_get_float(ctl);

        if(oldval != newval) {
            cam_unit_control_try_set_float(ctl, newval);
        }

        // was the control successfully set?
        oldval = cam_unit_control_get_float(ctl);

        if(oldval != newval) {
            gtk_spin_button_set_value (spinb, oldval);
        }
    }
}

// this callback function allows us to override the text formatting for the
// displayed values in the GtkSpinButton
static gboolean
on_spin_button_output(GtkSpinButton *spinb, CamUnitControlWidget *self)
{
    char *text = NULL;
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return FALSE;
    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(spinb), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;
    char *fmt = cam_unit_control_get_display_format(ctl);
    CamUnitControlType ctl_type = cam_unit_control_get_control_type(ci->ctl);
    if (ctl_type == CAM_UNIT_CONTROL_TYPE_INT) {
        text = g_strdup_printf(fmt, cam_unit_control_get_int(ctl));
    } else if (ctl_type == CAM_UNIT_CONTROL_TYPE_FLOAT) {
        text = g_strdup_printf(fmt, cam_unit_control_get_float(ctl));
    }
    gtk_entry_set_text(GTK_ENTRY(spinb), text);
    g_free(text);
    return TRUE;
}

static void
add_spinbutton (CamUnitControlWidget * self,
        float min, float max, float step, float initial_value,
        int use_int, CamUnitControl * ctl)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    // label
    char *tmp = g_strjoin("", cam_unit_control_get_name(ctl), ":", NULL);
    GtkWidget *label = gtk_label_new(tmp);
    free(tmp);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
    gtk_table_attach (priv->table, label, 0, 1,
            priv->trows, priv->trows+1,
            GTK_FILL, 0, 0, 0);
    gtk_widget_show (label);

    // spinbutton widget

    GtkWidget *spinb = gtk_spin_button_new_with_range (min, max, step);
    set_tooltip (self, spinb, ctl);

    gtk_table_attach (priv->table, spinb, 1, 3,
            priv->trows, priv->trows+1,
            GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
    gtk_widget_show (spinb);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinb), initial_value);

    CamUnitControlType ctl_type = cam_unit_control_get_control_type(ctl);
    if (ctl_type == CAM_UNIT_CONTROL_TYPE_FLOAT) {
        gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinb), 6);
    } else {
        gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinb), 0);
    }

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) calloc(1, sizeof(ControlWidgetInfo));
    g_object_set_data(G_OBJECT(spinb), "ControlWidgetInfo", ci);
    ci->widget = spinb;
    ci->labelval = NULL;
    ci->label = label;
    ci->maxchars = MAX (num_chars (min), num_chars (max));
    ci->ctl = ctl;
    ci->use_int = use_int;
    g_hash_table_insert(priv->ctl_info, ctl, ci);
    control_set_sensitive (ci);

    set_slider_label (self, ci);
    g_signal_connect (G_OBJECT (spinb), "value-changed",
            G_CALLBACK (on_spin_button_changed), self);
    g_signal_connect (G_OBJECT (spinb), "output",
            G_CALLBACK (on_spin_button_output), self);

    priv->trows++;
}

static void
add_float_control (CamUnitControlWidget * self, CamUnitControl * ctl)
{
    int ui_hints = cam_unit_control_get_ui_hints (ctl);
    float min_float = cam_unit_control_get_min_float(ctl);
    float max_float = cam_unit_control_get_max_float(ctl);
    float step_float = cam_unit_control_get_step_float(ctl);
    float val = cam_unit_control_get_float(ctl);
    if (ui_hints & CAM_UNIT_CONTROL_SPINBUTTON) {
        add_spinbutton (self, min_float, max_float, step_float, val, 1, ctl);
    } else {
        add_slider (self, min_float, max_float, step_float, val, 0, ctl);
    }
}

static void
add_integer_control(CamUnitControlWidget *self, CamUnitControl *ctl)
{
    int min_int = cam_unit_control_get_min_int(ctl);
    int max_int = cam_unit_control_get_max_int(ctl);
    int step_int = cam_unit_control_get_step_int(ctl);
    int val = cam_unit_control_get_int(ctl);
    const char *name = cam_unit_control_get_name(ctl);
    if(step_int == 0) {
        err("UnitControlWidget: refusing to add a widget for integer\n"
            "                   control [%s] with step 0\n", name);
        return;
    }
    int ui_hints = cam_unit_control_get_ui_hints(ctl);
    if (ui_hints & CAM_UNIT_CONTROL_SPINBUTTON) {
        add_spinbutton (self, min_int, max_int, step_int, val, 1, ctl);
    } else {
        add_slider (self, min_int, max_int, step_int, val, 1, ctl);
    }
}

static void
on_boolean_ctl_clicked(GtkWidget *cb, CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(cb), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;

    cam_unit_control_try_set_boolean(ctl, TRUE);
}

static void
on_boolean_ctl_changed(GtkWidget *cb, CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(cb), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;

    int newval = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    int oldval = cam_unit_control_get_boolean(ctl);
    if(oldval != newval) {
        cam_unit_control_try_set_boolean(ctl, newval);
    }

    // was the control successfully set?
    oldval = cam_unit_control_get_boolean(ctl);
    if(oldval != newval) {
        // nope.  re-set the toggle button
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), 
                oldval ? TRUE : FALSE);
    }
}

static void
add_boolean_ctl_helper(CamUnitControlWidget *self, CamUnitControl *ctl,
        GtkWidget *cb) 
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    gtk_widget_show (cb);

    gtk_table_attach (GTK_TABLE (priv->table), cb, 1, 3,
            priv->trows, priv->trows+1,
            GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

    if (GTK_IS_TOGGLE_BUTTON (cb)) {
        int val = cam_unit_control_get_boolean(ctl);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb),
                val ? TRUE : FALSE);
    }

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) calloc(1, sizeof(ControlWidgetInfo));
    g_object_set_data(G_OBJECT(cb), "ControlWidgetInfo", ci);
    ci->widget = cb;
    ci->label = NULL;
    ci->labelval = NULL;
    ci->maxchars = 0;
    ci->ctl = ctl;
    g_hash_table_insert(priv->ctl_info, ctl, ci);

    control_set_sensitive (ci);

    if (GTK_IS_TOGGLE_BUTTON (cb)) {
        g_signal_connect (G_OBJECT (cb), "toggled",
                G_CALLBACK (on_boolean_ctl_changed), self);
    }
    else {
        g_signal_connect (G_OBJECT (cb), "clicked",
                G_CALLBACK (on_boolean_ctl_clicked), self);
    }

    priv->trows++;
}

static void
add_boolean_control(CamUnitControlWidget *self, CamUnitControl *ctl)
{
    int ui_hints = cam_unit_control_get_ui_hints(ctl);
    GtkWidget *widget = NULL;
    const char *name = cam_unit_control_get_name(ctl);
    if (ui_hints & CAM_UNIT_CONTROL_ONE_SHOT)
        widget = gtk_button_new_with_label (name);
    else if (ui_hints & CAM_UNIT_CONTROL_TOGGLE_BUTTON)
        widget = gtk_toggle_button_new_with_label (name);
    else
        widget = gtk_check_button_new_with_label (name);

    set_tooltip (self, widget, ctl);
    add_boolean_ctl_helper(self, ctl, widget);
}

enum {
    ENUM_COLUMN_NICKNAME = 0,
    ENUM_COLUMN_ENABLED,
    ENUM_COLUMN_VALUE,
};

static gboolean
_enum_val_to_iter(GtkComboBox *combo, int value, GtkTreeIter *result)
{
    GtkTreeModel *model = gtk_combo_box_get_model(combo);
    if(!gtk_tree_model_get_iter_first(model, result))
        return FALSE;

    int row_val;
    gtk_tree_model_get(model, result, ENUM_COLUMN_VALUE, &row_val, -1);
    if(row_val == value)
        return TRUE;

    while(gtk_tree_model_iter_next(model, result)) {
        gtk_tree_model_get(model, result, ENUM_COLUMN_VALUE, &row_val, -1);
        if(row_val == value)
            return TRUE;
    }
    return FALSE;
}

static void
on_menu_changed (GtkComboBox * combo, CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(combo), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;

    int oldval = cam_unit_control_get_enum(ctl);

    GtkTreeIter iter;
    if(! gtk_combo_box_get_active_iter(combo, &iter)) {
        return;
    }
    int newval;
    gtk_tree_model_get(gtk_combo_box_get_model(combo),
            &iter, ENUM_COLUMN_VALUE, &newval, -1);
    
    if(oldval != newval) {
        cam_unit_control_try_set_enum(ctl, newval);
    }

    // was the control successfully set?
    int actual_val = cam_unit_control_get_enum(ctl);

    if(actual_val != newval) {
        GtkTreeIter iter;
        if(_enum_val_to_iter(combo, newval, &iter)) {
            gtk_combo_box_set_active_iter(combo, &iter);
        }
    }
}

static void
add_menu_control(CamUnitControlWidget *self, CamUnitControl *ctl)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    // label
    char *tmp = g_strjoin("", cam_unit_control_get_name(ctl), ":", NULL);
    GtkWidget *label = gtk_label_new(tmp);
    free(tmp);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
    gtk_table_attach (priv->table, label, 0, 1,
            priv->trows, priv->trows+1,
            GTK_FILL, 0, 0, 0);
    gtk_widget_show (label);

    int active_val = cam_unit_control_get_enum(ctl);

    GtkListStore * store = gtk_list_store_new (3, 
            G_TYPE_STRING,
            G_TYPE_BOOLEAN,
            G_TYPE_INT);
    GList * entries = cam_unit_control_get_enum_entries(ctl);
    GtkTreeIter active_iter;
    gboolean found_active = FALSE;
    for(GList *eiter=entries; eiter; eiter=eiter->next) {
        const CamUnitControlEnumValue * entry = eiter->data;
        GtkTreeIter iter;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                ENUM_COLUMN_NICKNAME, entry->nickname,
                ENUM_COLUMN_ENABLED, entry->enabled,
                ENUM_COLUMN_VALUE, entry->value,
                -1);
        if(entry->value == active_val) {
            active_iter = iter;
            found_active = TRUE;
        }
    }
    g_list_free(entries);

    GtkWidget *eb = gtk_event_box_new ();
    GtkWidget *mb = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
    gtk_container_add (GTK_CONTAINER (eb), mb);
    set_tooltip (self, eb, ctl);
    GtkCellRenderer * renderer = gtk_cell_renderer_text_new ();
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (mb), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (mb), renderer,
            "text", ENUM_COLUMN_NICKNAME);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (mb), renderer,
            "sensitive", ENUM_COLUMN_ENABLED);

    g_object_unref (store);

#if 0
    GtkWidget *mb = gtk_combo_box_new_text ();
    for (j = 0; j <= cam_unit_control_get_max_int(ctl); j++) {
        gtk_combo_box_append_text (GTK_COMBO_BOX (mb), ctl->enum_entries[j]);
    }
#endif

    gtk_table_attach (priv->table, eb, 1, 3,
            priv->trows, priv->trows+1,
            GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
    gtk_widget_show (eb);
    gtk_widget_show (mb);

    if(found_active) {
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX(mb), &active_iter);
    }

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) calloc(1, sizeof(ControlWidgetInfo));
    g_object_set_data(G_OBJECT(mb), "ControlWidgetInfo", ci);
    ci->widget = mb;
    ci->label = label;
    ci->labelval = NULL;
    ci->maxchars = 0;
    ci->ctl = ctl;
    g_hash_table_insert(priv->ctl_info, ctl, ci);

    control_set_sensitive (ci);

    g_signal_connect (G_OBJECT (mb), "changed", G_CALLBACK (on_menu_changed), 
            self);

    priv->trows++;
}

static void
on_file_entry_choser_bt_clicked(GtkButton *button, CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(button), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;

    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new ("Choose File",
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self))),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL);
    char *newval = NULL;
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        newval = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        dbg(DBG_GUI, "user chose file: [%s]\n", newval);
        gtk_widget_destroy (dialog);
    } else {
        gtk_widget_destroy (dialog);
        return;
    }

    const char *oldval = cam_unit_control_get_string(ctl);
    
    if(!oldval || strcmp(newval, oldval)) {
        cam_unit_control_try_set_string(ctl, newval);
    }

    oldval = cam_unit_control_get_string(ctl);
    gtk_entry_set_text(GTK_ENTRY(ci->widget), oldval);
    free(newval);
}

static void
add_string_control_filename(CamUnitControlWidget *self, CamUnitControl *ctl)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    // label
    char *tmp = g_strjoin("", cam_unit_control_get_name(ctl), ":", NULL);
    GtkWidget *label = gtk_label_new(tmp);
    free(tmp);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
    gtk_table_attach (priv->table, label, 0, 1,
            priv->trows, priv->trows+1,
            GTK_FILL, 0, 0, 0);
    gtk_widget_show (label);

    // hbox to contain the text entry and file chooser button
    GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(priv->table, hbox, 1, 3, 
            priv->trows, priv->trows+1,
            GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
    gtk_widget_show(hbox);

    // text entry
    GtkWidget *entry = gtk_entry_new();
    set_tooltip (self, entry, ctl);
    gtk_entry_set_text(GTK_ENTRY(entry), cam_unit_control_get_string(ctl));
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);
    gtk_widget_show(entry);

    // file chooser button
    GtkWidget *chooser_bt = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(hbox), chooser_bt, FALSE, TRUE, 0);
    gtk_widget_show(chooser_bt);

    g_signal_connect(G_OBJECT(chooser_bt), "clicked", 
            G_CALLBACK(on_file_entry_choser_bt_clicked), self);

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) calloc(1, sizeof(ControlWidgetInfo));
    g_object_set_data(G_OBJECT(entry), "ControlWidgetInfo", ci);
    g_object_set_data(G_OBJECT(chooser_bt), "ControlWidgetInfo", ci);
    ci->widget = entry;
    ci->file_chooser_bt = chooser_bt;
    ci->label = label;
    ci->labelval = NULL;
    ci->maxchars = -1;
    ci->ctl = ctl;
    g_hash_table_insert(priv->ctl_info, ctl, ci);
    priv->trows++;
}

static void
on_string_control_clicked (GtkButton * button, CamUnitControlWidget * self)
{
    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(button), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;
    cam_unit_control_try_set_string (ctl,
            gtk_entry_get_text (GTK_ENTRY (ci->widget)));

    gtk_entry_set_text (GTK_ENTRY (ci->widget),
        cam_unit_control_get_string (ctl));

    /* Move cursor to end */
    gtk_editable_set_position (GTK_EDITABLE (ci->widget), -1);
}

static void
on_string_control_activated (GtkEntry * entry, CamUnitControlWidget * self)
{
    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(entry), 
                "ControlWidgetInfo");
    gtk_button_clicked (GTK_BUTTON (ci->button));
}

static gboolean
on_string_control_key (GtkEntry * entry, GdkEventKey * event,
        CamUnitControlWidget * self)
{
    if (event->keyval != GDK_Escape)
        return FALSE;

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) g_object_get_data(G_OBJECT(entry), 
                "ControlWidgetInfo");
    CamUnitControl *ctl = ci->ctl;
    /* Restore entry to its previous value */
    gtk_entry_set_text (GTK_ENTRY (entry), cam_unit_control_get_string (ctl));

    /* Move cursor to end */
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    return FALSE;
}

static void
add_string_control_entry(CamUnitControlWidget *self, CamUnitControl *ctl)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    // label
    char *tmp = g_strjoin("", cam_unit_control_get_name(ctl), ":", NULL);
    GtkWidget *label = gtk_label_new(tmp);
    free(tmp);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
    gtk_table_attach (priv->table, label, 0, 1,
            priv->trows, priv->trows+1,
            GTK_FILL, 0, 0, 0);
    gtk_widget_show (label);

    GtkWidget *hbox = gtk_hbox_new (FALSE, 1);
    // text entry
    GtkWidget *entry = gtk_entry_new();
    set_tooltip (self, entry, ctl);
    gtk_entry_set_text(GTK_ENTRY(entry), cam_unit_control_get_string(ctl));
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    //gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);
    
    GtkWidget *button = gtk_button_new_with_label ("Set");
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

    gtk_table_attach(priv->table, hbox, 1, 3, 
            priv->trows, priv->trows+1,
            GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

    gtk_widget_show_all(hbox);

    g_signal_connect (G_OBJECT (entry), "activate",
            G_CALLBACK (on_string_control_activated), self);
    g_signal_connect (G_OBJECT (entry), "key-press-event",
            G_CALLBACK (on_string_control_key), self);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (on_string_control_clicked), self);

    ControlWidgetInfo *ci = 
        (ControlWidgetInfo*) calloc(1, sizeof(ControlWidgetInfo));
    g_object_set_data(G_OBJECT(entry), "ControlWidgetInfo", ci);
    g_object_set_data(G_OBJECT(button), "ControlWidgetInfo", ci);
    ci->widget = entry;
    ci->label = label;
    ci->labelval = NULL;
    ci->button = button;
    ci->maxchars = -1;
    ci->ctl = ctl;
    g_hash_table_insert(priv->ctl_info, ctl, ci);
    priv->trows++;

    control_set_sensitive (ci);
}

static void
add_string_control(CamUnitControlWidget *self, CamUnitControl *ctl)
{
    int ui_hints = cam_unit_control_get_ui_hints(ctl);
    if(ui_hints & CAM_UNIT_CONTROL_FILENAME) {
        add_string_control_filename(self, ctl);
    } else {
        add_string_control_entry(self, ctl);
        // TODO
    }
}

static void
on_control_value_changed(CamUnit *unit, CamUnitControl *ctl, 
        CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;

    ControlWidgetInfo *ci = g_hash_table_lookup(priv->ctl_info, ctl);
    if(ci) {
        GValue val = { 0, };
        cam_unit_control_get_val(ctl, &val);

        CamUnitControlType ctl_type = cam_unit_control_get_control_type(ctl);
        switch(ctl_type) {
            case CAM_UNIT_CONTROL_TYPE_INT:
                if (GTK_IS_SPIN_BUTTON (ci->widget)) {
//                    printf("set value %d\n", g_value_get_int(&val));
                    gtk_spin_button_set_value (GTK_SPIN_BUTTON(ci->widget),
                        g_value_get_int(&val));
                } else if (GTK_IS_RANGE (ci->widget)) {
                    gtk_range_set_value (GTK_RANGE(ci->widget), 
                            g_value_get_int(&val));
                    set_slider_label(self, ci);
                } else {
                    err ("wtf?? %s:%d", __FILE__, __LINE__);
                }
                break;
            case CAM_UNIT_CONTROL_TYPE_FLOAT:
                if (GTK_IS_SPIN_BUTTON (ci->widget)) {
//                    printf("set value %d\n", g_value_get_int(&val));
                    gtk_spin_button_set_value (GTK_SPIN_BUTTON(ci->widget),
                        g_value_get_float(&val));
                } else if (GTK_IS_RANGE (ci->widget)) {
                    gtk_range_set_value (GTK_RANGE(ci->widget), 
                            g_value_get_float(&val));
                    set_slider_label(self, ci);
                } else {
                    err ("wtf?? %s:%d", __FILE__, __LINE__);
                }
                break;
            case CAM_UNIT_CONTROL_TYPE_BOOLEAN:
                if (GTK_IS_TOGGLE_BUTTON (ci->widget))
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ci->widget),
                            g_value_get_boolean(&val) ? TRUE : FALSE);
                break;
            case CAM_UNIT_CONTROL_TYPE_ENUM:
                {
                    GtkComboBox *combo = GTK_COMBO_BOX(ci->widget);
                    int chosen = g_value_get_int(&val);
                    GtkTreeIter iter;
                    if(_enum_val_to_iter(combo, chosen, &iter))
                        gtk_combo_box_set_active_iter(combo, &iter);
                }
                break;
            case CAM_UNIT_CONTROL_TYPE_STRING:
                gtk_entry_set_text(GTK_ENTRY(ci->widget), 
                        g_value_get_string(&val));
                break;
            default:
                err("UnitControlWidget:  unrecognized control type %d\n",
                        ctl_type);
                break;
        }
        g_value_unset(&val);
    }
}

static void
on_control_parameters_changed(CamUnit *unit, CamUnitControl *ctl,
        CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;
//    int enabled = cam_unit_control_get_enabled(ctl);
//    printf("control enabled changed [%s] to %d\n", 
//                cam_unit_control_get_name(ctl), enabled);

    ControlWidgetInfo *ci = g_hash_table_lookup(priv->ctl_info, ctl);
    if(ci) {
        control_set_sensitive (ci);

        CamUnitControlType ctl_type = cam_unit_control_get_control_type(ctl);
        switch (ctl_type) {
            case CAM_UNIT_CONTROL_TYPE_INT:
                {
                    int min = cam_unit_control_get_min_int (ctl);
                    int max = cam_unit_control_get_max_int (ctl);
                    int step = cam_unit_control_get_step_int (ctl);

                    if (GTK_IS_SPIN_BUTTON (ci->widget)) {
                        GtkSpinButton *sb = GTK_SPIN_BUTTON (ci->widget);
                        gtk_spin_button_set_range (sb, min, max);
                        gtk_spin_button_set_increments (sb, step, step * 10);
                    } else if (GTK_IS_RANGE (ci->widget)) {
                        gtk_range_set_range (GTK_RANGE(ci->widget), min, max);
                        gtk_range_set_increments (GTK_RANGE(ci->widget),
                                step, step * 10);
                    } else {
                        err ("wtf?? %s:%d", __FILE__, __LINE__);
                    }
                break;
                }
            case CAM_UNIT_CONTROL_TYPE_FLOAT:
                {
                    float min = cam_unit_control_get_min_float (ctl);
                    float max = cam_unit_control_get_max_float (ctl);
                    float step = cam_unit_control_get_step_float (ctl);

                    if (GTK_IS_SPIN_BUTTON (ci->widget)) {
                        GtkSpinButton *sb = GTK_SPIN_BUTTON (ci->widget);
                        gtk_spin_button_set_range (sb, min, max);
                        gtk_spin_button_set_increments (sb, step, step * 10);
                    } else if (GTK_IS_RANGE (ci->widget)) {
                        gtk_range_set_range (GTK_RANGE(ci->widget), min, max);
                        gtk_range_set_increments (GTK_RANGE(ci->widget),
                                step, step * 10);
                    } else {
                        err ("wtf?? %s:%d", __FILE__, __LINE__);
                    }
                }
                break;
            case CAM_UNIT_CONTROL_TYPE_ENUM:
                {
                    GList * entries = cam_unit_control_get_enum_entries(ctl);
                    int active_val = cam_unit_control_get_enum(ctl);
                    GtkTreeIter active_iter;
                    gboolean found_active = FALSE;
                    GtkComboBox *combo = GTK_COMBO_BOX(ci->widget);
                    GtkTreeModel *tree_model = gtk_combo_box_get_model(combo);
                    GtkListStore *store = GTK_LIST_STORE(tree_model);
                    gtk_list_store_clear(store);
                    for(GList *eiter=entries; eiter; eiter=eiter->next) {
                        const CamUnitControlEnumValue * entry = eiter->data;
                        GtkTreeIter iter;
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter,
                                ENUM_COLUMN_NICKNAME, entry->nickname,
                                ENUM_COLUMN_ENABLED, entry->enabled,
                                ENUM_COLUMN_VALUE, entry->value,
                                -1);
                        if(entry->value == active_val) {
                            active_iter = iter;
                            found_active = TRUE;
                        }
                    }
                    g_list_free(entries);
                    if(found_active) {
                        gtk_combo_box_set_active_iter (combo, &active_iter);
                    }
                }
                break;
            default:
                break;
        }
    }
}

static void
set_frame_label(CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if (priv->unit) {
        const char *uname = cam_unit_get_name(priv->unit);
        const char *sstr = cam_unit_is_streaming (priv->unit) ? 
            "Streaming" : "Off";
        char *tmp = g_strjoin("", uname, " [", sstr, "]", NULL);
        gtk_label_set (GTK_LABEL (priv->exp_label), tmp);
        free(tmp);
    } else {
        gtk_label_set (GTK_LABEL (priv->exp_label), "INVALID UNIT");
    }
}

static void
update_formats_combo(CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    for(; priv->formats_combo_nentries; priv->formats_combo_nentries--) {
        gtk_combo_box_remove_text(priv->formats_combo, 0);
    }

    if (! priv->unit) return;

    const CamUnitFormat *out_fmt = cam_unit_get_output_format(priv->unit);
    GList *output_formats = cam_unit_get_output_formats(priv->unit);
    GList *fiter;
    int selected = -1;
    priv->formats_combo_nentries = 0;
    for(fiter=output_formats; fiter; fiter=fiter->next) {
        CamUnitFormat *fmt = (CamUnitFormat*) fiter->data;
        gtk_combo_box_append_text(priv->formats_combo, 
                fmt->name);

        if(fmt == out_fmt) {
            selected = priv->formats_combo_nentries;
        }
        priv->formats_combo_nentries++;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->formats_combo), selected);
}

CamUnit * 
cam_unit_control_widget_get_unit(CamUnitControlWidget* self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    return priv->unit;
}

int
cam_unit_control_widget_set_unit(
        CamUnitControlWidget *self, CamUnit *unit)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(priv->unit) return -1;
    dbg(DBG_GUI, "UDCW: setting to [%s]\n", 
            cam_unit_get_id(unit));

    if (unit) {
        dbgl(DBG_REF, "ref_sink unit [%s]\n", cam_unit_get_id(unit));
        g_object_ref_sink (unit);
    }

    priv->unit = unit;
    set_frame_label(self);

    // prepare the output formats selection combo box
    if (priv->unit) {
        priv->formats_changed_handler_id = 
            g_signal_connect(G_OBJECT(unit), "output-formats-changed",
                    G_CALLBACK(on_output_formats_changed), self);
        priv->status_changed_handler_id = 
            g_signal_connect(G_OBJECT(unit), "status-changed",
                    G_CALLBACK(on_status_changed), self);
        g_signal_connect(G_OBJECT(priv->formats_combo), "changed",
                G_CALLBACK(on_formats_combo_changed), self);
    }

    update_formats_combo(self);

//    gtk_container_add (GTK_CONTAINER (priv->arrow_bin), hbox);

#if 0
    gtk_table_attach (priv->table, fmtlabel, 0, 1, 0, 1,
            GTK_FILL, 0, 0, 0);
    gtk_widget_show (fmtlabel);
    gtk_table_attach_defaults(priv->table, GTK_WIDGET(priv->formats_combo), 
            1, 3, 0, 1);
#endif

    // prepare the unit controls widgets
    if (priv->unit) {
        GList *controls = cam_unit_list_controls(unit);
        GList *citer;
        for(citer=controls; citer; citer=citer->next) {
            CamUnitControl *ctl = (CamUnitControl*) citer->data;

            dbg(DBG_GUI, "adding widget for [%s] of [%s]\n", 
                    cam_unit_control_get_name(ctl), 
                    cam_unit_get_id(priv->unit));

            CamUnitControlType ctl_type = cam_unit_control_get_control_type(ctl);
            switch(ctl_type) {
                case CAM_UNIT_CONTROL_TYPE_INT:
                    add_integer_control(self, ctl);
                    break;
                case CAM_UNIT_CONTROL_TYPE_FLOAT:
                    add_float_control (self, ctl);
                    break;
                case CAM_UNIT_CONTROL_TYPE_BOOLEAN:
                    add_boolean_control(self, ctl);
                    break;
                case CAM_UNIT_CONTROL_TYPE_ENUM:
                    add_menu_control(self, ctl);
                    break;
                case CAM_UNIT_CONTROL_TYPE_STRING:
                    add_string_control(self, ctl);
                    break;
                default:
                    err("UnitControlWidget:  unrecognized control type %d\n",
                            ctl_type);
                    break;
            }
        }
        g_list_free(controls);

        g_signal_connect(G_OBJECT(unit), "control-value-changed",
                G_CALLBACK(on_control_value_changed), self);
        g_signal_connect(G_OBJECT(unit), "control-parameters-changed",
                G_CALLBACK(on_control_parameters_changed), self);
    }

    //gtk_widget_show_all(GTK_WIDGET(priv->table));

    gtk_expander_set_expanded(priv->expander, FALSE);
    return 0;
}

void 
cam_unit_control_widget_detach(CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(! priv->unit) return;
    dbg(DBG_GUI, "detaching control widget from unit signals\n");

    g_signal_handlers_disconnect_by_func (G_OBJECT (priv->unit),
            G_CALLBACK(on_control_value_changed), self);
    g_signal_handlers_disconnect_by_func (G_OBJECT (priv->unit),
            G_CALLBACK(on_control_parameters_changed), self);
    g_signal_handler_disconnect(priv->unit, 
            priv->status_changed_handler_id);
    g_signal_handler_disconnect(priv->unit, 
            priv->formats_changed_handler_id);
    dbgl(DBG_REF, "unref unit\n");
    g_object_unref (priv->unit);
    priv->unit = NULL;
}

static void
on_output_formats_changed(CamUnit *unit, CamUnitControlWidget *self)
{
    dbg(DBG_GUI, "detected changed output formats for [%s]\n", 
            cam_unit_get_id(unit));

    update_formats_combo(self);
}

static void
on_formats_combo_changed(GtkComboBox *combo, CamUnitControlWidget *self)
{
    int selected = gtk_combo_box_get_active (combo);
    if (selected < 0)
        return;
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);

    GList *output_formats = cam_unit_get_output_formats(priv->unit);
    if (!output_formats)
        return;

    GList *format_entry = g_list_nth (output_formats, selected);
    g_assert (format_entry);
    if (format_entry->data != cam_unit_get_output_format (priv->unit)) {
        CamUnitFormat *cfmt = CAM_UNIT_FORMAT (format_entry->data);
        if (cam_unit_is_streaming (priv->unit)) {
            cam_unit_stream_shutdown (priv->unit);
            cam_unit_stream_init (priv->unit, cfmt);
        }
    }
    g_list_free (output_formats);
}

static void
on_close_button_clicked(GtkButton *bt, CamUnitControlWidget *self)
{
    g_signal_emit(G_OBJECT(self), 
            unit_control_widget_signals[CLOSE_BUTTON_CLICKED_SIGNAL], 0);
}

static void
on_expander_notify(GtkWidget *widget, GParamSpec *param, 
        CamUnitControlWidget *self)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    if(gtk_expander_get_expanded(priv->expander)) {
        gtk_widget_show(GTK_WIDGET(priv->table));
    } else {
        gtk_widget_hide(GTK_WIDGET(priv->table));
    }
}

static void
on_status_changed(CamUnit *unit, CamUnitControlWidget *self)
{
    set_frame_label(self);
    update_formats_combo(self);
}

void
cam_unit_control_widget_set_expanded (CamUnitControlWidget * self,
        gboolean expanded)
{
    CamUnitControlWidgetPriv * priv = CAM_UNIT_CONTROL_WIDGET_GET_PRIVATE(self);
    gtk_expander_set_expanded (GTK_EXPANDER (priv->expander), expanded);
}
