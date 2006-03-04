#include <time.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>

#include "xklavier_private.h"

static GObjectClass *parent_class = NULL;

static XklEngine *the_engine = NULL;

gint xkl_debug_level = 0;

static XklLogAppender log_appender = xkl_default_log_appender;

const gchar *xkl_last_error_message;

enum {
	PROP_0,
	PROP_DISPLAY,
	PROP_BACKEND_NAME,
	PROP_FEATURES,
	PROP_MAX_NUM_GROUPS,
	PROP_NUM_GROUPS,
	PROP_DEFAULT_GROUP,
	PROP_SECONDARY_GROUPS_MASK,
	PROP_INDICATORS_HANDLING,
};

void
xkl_engine_set_indicators_handling(XklEngine * engine,
				   gboolean whether_handle)
{
	xkl_engine_priv(engine, handle_indicators) = whether_handle;
}

gboolean
xkl_engine_get_indicators_handling(XklEngine * engine)
{
	return xkl_engine_priv(engine, handle_indicators);
}

void
xkl_set_debug_level(int level)
{
	xkl_debug_level = level;
}

void
xkl_engine_set_group_per_toplevel_window(XklEngine * engine,
					 gboolean is_set)
{
	xkl_engine_priv(engine, group_per_toplevel_window) = is_set;
}

gboolean
xkl_engine_is_group_per_toplevel_window(XklEngine * engine)
{
	return xkl_engine_priv(engine, group_per_toplevel_window);
}

static void
xkl_engine_set_switch_to_secondary_group(XklEngine * engine, gboolean val)
{
	CARD32 propval = (CARD32) val;
	Display *dpy = xkl_engine_get_display(engine);
	XChangeProperty(dpy,
			xkl_engine_priv(engine, root_window),
			xkl_engine_priv(engine,
					atoms)[XKLAVIER_ALLOW_SECONDARY],
			XA_INTEGER, 32, PropModeReplace,
			(unsigned char *) &propval, 1);
	XSync(dpy, False);
}

void
xkl_engine_allow_one_switch_to_secondary_group(XklEngine * engine)
{
	xkl_debug(150,
		  "Setting allow_one_switch_to_secondary_group flag\n");
	xkl_engine_set_switch_to_secondary_group(engine, TRUE);
}

gboolean
xkl_engine_is_one_switch_to_secondary_group_allowed(XklEngine * engine)
{
	gboolean rv = FALSE;
	unsigned char *propval = NULL;
	Atom actual_type;
	int actual_format;
	unsigned long bytes_remaining;
	unsigned long actual_items;
	int result;

	result =
	    XGetWindowProperty(xkl_engine_get_display(engine),
			       xkl_engine_priv(engine, root_window),
			       xkl_engine_priv(engine, atoms)
			       [XKLAVIER_ALLOW_SECONDARY], 0L, 1L, False,
			       XA_INTEGER, &actual_type, &actual_format,
			       &actual_items, &bytes_remaining, &propval);

	if (Success == result) {
		if (actual_format == 32 && actual_items == 1) {
			rv = (gboolean) * (Bool *) propval;
		}
		XFree(propval);
	}

	return rv;
}

void
xkl_engine_one_switch_to_secondary_group_performed(XklEngine * engine)
{
	xkl_debug(150,
		  "Resetting allow_one_switch_to_secondary_group flag\n");
	xkl_engine_set_switch_to_secondary_group(engine, FALSE);
}

void
xkl_engine_set_default_group(XklEngine * engine, gint group)
{
	xkl_engine_priv(engine, default_group) = group;
}

gint
xkl_engine_get_default_group(XklEngine * engine)
{
	return xkl_engine_priv(engine, default_group);
}

void
xkl_engine_set_secondary_groups_mask(XklEngine * engine, guint mask)
{
	xkl_engine_priv(engine, secondary_groups_mask) = mask;
}

guint
xkl_engine_get_secondary_groups_mask(XklEngine * engine)
{
	return xkl_engine_priv(engine, secondary_groups_mask);
}

void
xkl_set_log_appender(XklLogAppender func)
{
	log_appender = func;
}

gint
xkl_engine_start_listen(XklEngine * engine, guint what)
{
	xkl_engine_priv(engine, listener_type) = what;

	if (!
	    (xkl_engine_priv(engine, features) &
	     XKLF_REQUIRES_MANUAL_LAYOUT_MANAGEMENT)
&& (what & XKLL_MANAGE_LAYOUTS))
		xkl_debug(0,
			  "The backend does not require manual layout management - "
			  "but it is provided by the application");

	xkl_engine_resume_listen(engine);
	xkl_engine_load_window_tree(engine);
	XFlush(xkl_engine_get_display(engine));
	return 0;
}

gint
xkl_engine_stop_listen(XklEngine * engine)
{
	xkl_engine_pause_listen(engine);
	return 0;
}

XklEngine *
xkl_engine_get_instance(Display * display)
{
	if (the_engine != NULL) {
		g_object_ref(G_OBJECT(the_engine));
		return the_engine;
	}

	if (!display) {
		xkl_debug(10, "xkl_init : display is NULL ?\n");
		return NULL;
	}

	the_engine =
	    XKL_ENGINE(g_object_new
		       (xkl_engine_get_type(), "display", display, NULL));

	return the_engine;
}

gboolean
xkl_engine_grab_key(XklEngine * engine, gint keycode, guint modifiers)
{
	gboolean ret_code;
	gchar *keyname;
	Display *dpy = xkl_engine_get_display(engine);

	if (xkl_debug_level >= 100) {
		keyname =
		    XKeysymToString(XKeycodeToKeysym(dpy, keycode, 0));
		xkl_debug(100, "Listen to the key %d/(%s)/%d\n", keycode,
			  keyname, modifiers);
	}

	if (0 == keycode)
		return FALSE;

	xkl_engine_priv(engine, last_error_code) = Success;

	ret_code =
	    XGrabKey(dpy, keycode, modifiers,
		     xkl_engine_priv(engine, root_window), TRUE,
		     GrabModeAsync, GrabModeAsync);
	XSync(dpy, False);

	xkl_debug(100, "XGrabKey recode %d/error %d\n",
		  ret_code, xkl_engine_priv(engine, last_error_code));

	ret_code = (xkl_engine_priv(engine, last_error_code) == Success);

	if (!ret_code)
		xkl_last_error_message = "Could not grab the key";

	return ret_code;
}

gboolean
xkl_engine_ungrab_key(XklEngine * engine, gint keycode, guint modifiers)
{
	if (0 == keycode)
		return FALSE;

	return Success == XUngrabKey(xkl_engine_get_display(engine),
				     keycode, 0,
				     xkl_engine_priv(engine, root_window));
}

gint
xkl_engine_get_next_group(XklEngine * engine)
{
	gint n = xkl_engine_get_num_groups(engine);
	return (xkl_engine_priv(engine, curr_state).group + 1) % n;
}

gint
xkl_engine_get_prev_group(XklEngine * engine)
{
	gint n = xkl_engine_get_num_groups(engine);
	return (xkl_engine_priv(engine, curr_state).group + n - 1) % n;
}

gint
xkl_engine_get_current_window_group(XklEngine * engine)
{
	XklState state;
	if (xkl_engine_priv(engine, curr_toplvl_win) == (Window) NULL) {
		xkl_debug(150, "cannot restore without current client\n");
	} else
	    if (xkl_engine_get_toplevel_window_state
		(engine, xkl_engine_priv(engine, curr_toplvl_win),
		 &state)) {
		return state.group;
	} else
		xkl_debug(150,
			  "Unbelievable: current client " WINID_FORMAT
			  ", '%s' has no group\n",
			  xkl_engine_priv(engine, curr_toplvl_win),
			  xkl_get_debug_window_title(engine,
						     xkl_engine_priv
						     (engine,
						      curr_toplvl_win)));
	return 0;
}

void
xkl_engine_set_transparent(XklEngine * engine, Window win,
			   gboolean transparent)
{
	Window toplevel_win;
	xkl_debug(150,
		  "setting transparent flag %d for " WINID_FORMAT "\n",
		  transparent, win);

	if (!xkl_engine_find_toplevel_window(engine, win, &toplevel_win)) {
		xkl_debug(150, "No toplevel window!\n");
		toplevel_win = win;
/*    return; */
	}

	xkl_engine_set_toplevel_window_transparent(engine, toplevel_win,
						   transparent);
}

gboolean
xkl_engine_is_window_transparent(XklEngine * engine, Window win)
{
	Window toplevel_win;

	if (!xkl_engine_find_toplevel_window(engine, win, &toplevel_win))
		return FALSE;
	return xkl_engine_is_toplevel_window_transparent(engine,
							 toplevel_win);
}

/**
 * Loads the tree recursively.
 */
gboolean
xkl_engine_load_window_tree(XklEngine * engine)
{
	Window focused;
	int revert;
	gboolean retval = TRUE, have_toplevel_win;

	if (xkl_engine_priv(engine, listener_type) &
	    XKLL_MANAGE_WINDOW_STATES)
		retval =
		    xkl_engine_load_subtree(engine,
					    xkl_engine_priv(engine,
							    root_window),
					    0, &xkl_engine_priv(engine,
								curr_state));

	XGetInputFocus(xkl_engine_get_display(engine), &focused, &revert);

	xkl_debug(160, "initially focused: " WINID_FORMAT ", '%s'\n",
		  focused, xkl_get_debug_window_title(engine, focused));

	have_toplevel_win =
	    xkl_engine_find_toplevel_window(engine, focused,
					    &xkl_engine_priv(engine,
							     curr_toplvl_win));

	if (have_toplevel_win) {
		gboolean have_state =
		    xkl_engine_get_toplevel_window_state(engine,
							 xkl_engine_priv
							 (engine,
							  curr_toplvl_win),
							 &xkl_engine_priv
							 (engine,
							  curr_state));
		xkl_debug(160,
			  "initial toplevel: " WINID_FORMAT
			  ", '%s' %s state %d/%X\n",
			  xkl_engine_priv(engine, curr_toplvl_win),
			  xkl_get_debug_window_title(engine,
						     xkl_engine_priv
						     (engine,
						      curr_toplvl_win)),
			  (have_state ? "with" : "without"),
			  (have_state ?
			   xkl_engine_priv(engine, curr_state).group : -1),
			  (have_state ?
			   xkl_engine_priv(engine,
					   curr_state).indicators : -1));
	} else {
		xkl_debug(160,
			  "Could not find initial app. "
			  "Probably, focus belongs to some WM service window. "
			  "Will try to survive:)");
	}

	return retval;
}

void
_xkl_debug(const gchar file[], const gchar function[], gint level,
	   const gchar format[], ...)
{
	va_list lst;

	if (level > xkl_debug_level)
		return;

	va_start(lst, format);
	if (log_appender != NULL)
		(*log_appender) (file, function, level, format, lst);
	va_end(lst);
}

void
xkl_default_log_appender(const gchar file[], const gchar function[],
			 gint level, const gchar format[], va_list args)
{
	time_t now = time(NULL);
	fprintf(stdout, "[%08ld,%03d,%s:%s/] \t", now, level, file,
		function);
	vfprintf(stdout, format, args);
}

/**
 * Just selects some events from the window.
 */
void
xkl_engine_select_input(XklEngine * engine, Window win, gulong mask)
{
	if (xkl_engine_priv(engine, root_window) == win)
		xkl_debug(160,
			  "Someone is looking for %lx on root window ***\n",
			  mask);

	XSelectInput(xkl_engine_get_display(engine), win, mask);
}

void
xkl_engine_select_input_merging(XklEngine * engine, Window win,
				gulong mask)
{
	XWindowAttributes attrs;
	gulong oldmask = 0L, newmask;
	memset(&attrs, 0, sizeof(attrs));
	if (XGetWindowAttributes
	    (xkl_engine_get_display(engine), win, &attrs))
		oldmask = attrs.your_event_mask;

	newmask = oldmask | mask;
	if (newmask != oldmask)
		xkl_engine_select_input(engine, win, newmask);
}

void
xkl_engine_try_call_state_func(XklEngine * engine,
			       XklStateChange change_type,
			       XklState * old_state)
{
	gint group = xkl_engine_priv(engine, curr_state).group;
	gboolean restore = old_state->group == group;

	xkl_debug(150,
		  "change_type: %d, group: %d, secondary_group_mask: %X, allowsecondary: %d\n",
		  change_type, group, xkl_engine_priv(engine,
						      secondary_groups_mask),
		  xkl_engine_is_one_switch_to_secondary_group_allowed
		  (engine));

	if (change_type == GROUP_CHANGED) {
		if (!restore) {
			if ((xkl_engine_priv(engine, secondary_groups_mask)
			     & (1 << group)) != 0
			    &&
			    !xkl_engine_is_one_switch_to_secondary_group_allowed
			    (engine)) {
				xkl_debug(150, "secondary -> go next\n");
				group = xkl_engine_get_next_group(engine);
				xkl_engine_lock_group(engine, group);
				return;	/* we do not need to revalidate */
			}
		}
		xkl_engine_one_switch_to_secondary_group_performed(engine);
	}

	g_signal_emit_by_name(engine, "X-state-changed", change_type,
			      xkl_engine_priv(engine, curr_state).group,
			      restore);

}

void
xkl_engine_ensure_vtable_inited(XklEngine * engine)
{
	char *p;
	if (xkl_engine_priv(engine, backend_id) == NULL) {
		xkl_debug(0, "ERROR: XKL VTable is NOT initialized.\n");
		/* force the crash! */
		p = NULL;
		*p = '\0';
	}
}

const gchar *
xkl_engine_get_backend_name(XklEngine * engine)
{
	return xkl_engine_priv(engine, backend_id);
}

guint
xkl_engine_get_features(XklEngine * engine)
{
	return xkl_engine_priv(engine, features);
}

void
xkl_engine_reset_all_info(XklEngine * engine, const gchar reason[])
{
	xkl_debug(150, "Resetting all the cached info, reason: [%s]\n",
		  reason);
	xkl_engine_ensure_vtable_inited(engine);
	if (!xkl_engine_vcall(engine, if_cached_info_equals_actual)
	    (engine)) {
		xkl_engine_vcall(engine, free_all_info) (engine);
		xkl_engine_vcall(engine, load_all_info) (engine);
	} else
		xkl_debug(100,
			  "NOT Resetting the cache: same configuration\n");
}

/**
 * Calling through vtable
 */
const gchar **
xkl_engine_groups_get_names(XklEngine * engine)
{
	xkl_engine_ensure_vtable_inited(engine);
	return xkl_engine_vcall(engine, get_groups_names) (engine);
}

guint
xkl_engine_get_num_groups(XklEngine * engine)
{
	xkl_engine_ensure_vtable_inited(engine);
	return xkl_engine_vcall(engine, get_num_groups) (engine);
}

void
xkl_engine_lock_group(XklEngine * engine, int group)
{
	xkl_engine_ensure_vtable_inited(engine);
	xkl_engine_vcall(engine, lock_group) (engine, group);
}

gint
xkl_engine_pause_listen(XklEngine * engine)
{
	xkl_engine_ensure_vtable_inited(engine);
	return xkl_engine_vcall(engine, pause_listen) (engine);
}

gint
xkl_engine_resume_listen(XklEngine * engine)
{
	xkl_engine_ensure_vtable_inited(engine);
	xkl_debug(150, "listenerType: %x\n",
		  xkl_engine_priv(engine, listener_type));
	if (xkl_engine_vcall(engine, resume_listen) (engine))
		return 1;

	xkl_engine_select_input_merging(engine,
					xkl_engine_priv(engine,
							root_window),
					SubstructureNotifyMask |
					PropertyChangeMask);

	xkl_engine_vcall(engine,
			 get_server_state) (engine,
					    &xkl_engine_priv(engine,
							     curr_state));
	return 0;
}

gboolean
xkl_engine_load_all_info(XklEngine * engine)
{
	xkl_engine_ensure_vtable_inited(engine);
	return xkl_engine_vcall(engine, load_all_info) (engine);
}

void
xkl_engine_free_all_info(XklEngine * engine)
{
	xkl_engine_ensure_vtable_inited(engine);
	xkl_engine_vcall(engine, free_all_info) (engine);
}

guint
xkl_engine_get_max_num_groups(XklEngine * engine)
{
	xkl_engine_ensure_vtable_inited(engine);
	return xkl_engine_vcall(engine, get_max_num_groups) (engine);
}

XklEngine *
xkl_get_the_engine()
{
	return the_engine;
}

G_DEFINE_TYPE(XklEngine, xkl_engine, G_TYPE_OBJECT)

static GObject *
xkl_engine_constructor(GType type,
		       guint n_construct_properties,
		       GObjectConstructParam * construct_properties)
{
	GObject *obj;

	{
		/* Invoke parent constructor. */
		XklEngineClass *klass;
		klass =
		    XKL_ENGINE_CLASS(g_type_class_peek(XKL_TYPE_ENGINE));
		obj =
		    parent_class->constructor(type, n_construct_properties,
					      construct_properties);
	}

	XklEngine *engine = XKL_ENGINE(obj);

	Display *display =
	    (Display *) g_value_peek_pointer(construct_properties[0].
					     value);

	xkl_engine_priv(engine, display) = display;

	int scr;

	xkl_engine_priv(engine, default_error_handler) =
	    XSetErrorHandler((XErrorHandler) xkl_process_error);

	scr = DefaultScreen(display);
	xkl_engine_priv(engine, root_window) = RootWindow(display, scr);

	xkl_engine_priv(engine, skip_one_restore) = FALSE;
	xkl_engine_priv(engine, default_group) = -1;
	xkl_engine_priv(engine, secondary_groups_mask) = 0L;
	xkl_engine_priv(engine, prev_toplvl_win) = 0;

	xkl_engine_priv(engine, atoms)[WM_NAME] =
	    XInternAtom(display, "WM_NAME", False);
	xkl_engine_priv(engine, atoms)[WM_STATE] =
	    XInternAtom(display, "WM_STATE", False);
	xkl_engine_priv(engine, atoms)[XKLAVIER_STATE] =
	    XInternAtom(display, "XKLAVIER_STATE", False);
	xkl_engine_priv(engine, atoms)[XKLAVIER_TRANSPARENT] =
	    XInternAtom(display, "XKLAVIER_TRANSPARENT", False);
	xkl_engine_priv(engine, atoms)[XKLAVIER_ALLOW_SECONDARY] =
	    XInternAtom(display, "XKLAVIER_ALLOW_SECONDARY", False);

	xkl_engine_one_switch_to_secondary_group_performed(engine);

	gint rv = -1;
	xkl_debug(150, "Trying all backends:\n");
#ifdef ENABLE_XKB_SUPPORT
	xkl_debug(150, "Trying XKB backend\n");
	rv = xkl_xkb_init(engine);
#endif
#ifdef ENABLE_XMM_SUPPORT
	if (rv != 0) {
		xkl_debug(150, "Trying XMM backend\n");
		rv = xkl_xmm_init(engine);
	}
#endif
	if (rv == 0) {
		xkl_debug(150, "Actual backend: %s\n",
			  xkl_engine_get_backend_name(engine));
	} else {
		xkl_debug(0, "All backends failed, last result: %d\n", rv);
		xkl_engine_priv(engine, display) = NULL;
		g_object_unref(G_OBJECT(engine));
		return NULL;
	}

	if (!xkl_engine_load_all_info(engine)) {
		g_object_unref(G_OBJECT(engine));
		return NULL;
	}

	return obj;
}

static void
xkl_engine_init(XklEngine * engine)
{
	engine->priv = g_new0(XklEnginePrivate, 1);
}

static void
xkl_engine_set_property(GObject * object,
			guint property_id,
			const GValue * value, GParamSpec * pspec)
{
}

static void
xkl_engine_get_property(GObject * object,
			guint property_id,
			GValue * value, GParamSpec * pspec)
{
	XklEngine *engine = XKL_ENGINE(object);

	switch (property_id) {
	case PROP_DISPLAY:
		g_value_set_pointer(value, xkl_engine_get_display(engine));
		break;
	case PROP_BACKEND_NAME:
		g_value_set_string(value,
				   xkl_engine_priv(engine, backend_id));
		break;
	case PROP_FEATURES:
		g_value_set_flags(value,
				  xkl_engine_priv(engine, features));
		break;
	case PROP_MAX_NUM_GROUPS:
		g_value_set_uint(value,
				 xkl_engine_vcall(engine,
						  get_max_num_groups)
				 (engine));
		break;
	case PROP_NUM_GROUPS:
		g_value_set_uint(value,
				 xkl_engine_vcall(engine, get_num_groups)
				 (engine));
		break;
	case PROP_DEFAULT_GROUP:
		g_value_set_uint(value,
				 xkl_engine_priv(engine, default_group));
		break;
	case PROP_SECONDARY_GROUPS_MASK:
		g_value_set_uint(value,
				 xkl_engine_priv(engine,
						 secondary_groups_mask));
		break;
	case PROP_INDICATORS_HANDLING:
		g_value_set_boolean(value,
				    xkl_engine_priv(engine,
						    handle_indicators));
		break;
	}
}

static void
xkl_engine_finalize(GObject * obj)
{
	XklEngine *engine = (XklEngine *) obj;

	XSetErrorHandler((XErrorHandler)
			 xkl_engine_priv(engine, default_error_handler));

	xkl_engine_free_all_info(engine);

	gpointer backend = xkl_engine_priv(engine, backend);
	if (backend != NULL)
		g_free(backend);
	g_free(engine->priv);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
xkl_engine_class_init(XklEngineClass * klass)
{
	static GFlagsValue feature_flags[] = {
		{0x01, "XKLF_CAN_TOGGLE_INDICATORS", NULL},
		{0x02, "XKLF_CAN_OUTPUT_CONFIG_AS_ASCII", NULL},
		{0x04, "XKLF_CAN_OUTPUT_CONFIG_AS_BINARY", NULL},
		{0x08, "XKLF_MULTIPLE_LAYOUTS_SUPPORTED", NULL},
		{0x10, "XKLF_REQUIRES_MANUAL_LAYOUT_MANAGEMENT", NULL},
		{0, NULL, NULL}
	};
	static GEnumValue state_change_values[] = {
		{0, "GROUP_CHANGED", NULL},
		{1, "INDICATORS_CHANGED", NULL},
		{0, NULL, NULL}
	};

	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	parent_class = g_type_class_peek_parent(object_class);

	object_class->constructor = xkl_engine_constructor;
	object_class->finalize = xkl_engine_finalize;
	object_class->set_property = xkl_engine_set_property;
	object_class->get_property = xkl_engine_get_property;

	GParamSpec *display_param_spec = g_param_spec_pointer("display",
							      "Display",
							      "X Display pointer",
							      G_PARAM_CONSTRUCT_ONLY
							      |
							      G_PARAM_READWRITE);

	GParamSpec *backend_name_param_spec =
	    g_param_spec_string("backendName",
				"backendName",
				"Backend name",
				NULL,
				G_PARAM_READABLE);

	GType features_type = g_flags_register_static("XklEngineFeatures",
						      feature_flags);

	GType state_change_type =
	    g_enum_register_static("XklEngineStateChangeType",
				   state_change_values);

	GParamSpec *features_param_spec = g_param_spec_flags("features",
							     "Features",
							     "Backend features",
							     features_type,
							     0,
							     G_PARAM_READABLE);
	GParamSpec *max_num_groups_param_spec =
	    g_param_spec_uint("max-num-ngroups",
			      "maxNumGroups",
			      "Max number of groups",
			      0, 0x100, 0,
			      G_PARAM_READABLE);

	GParamSpec *num_groups_param_spec = g_param_spec_uint("num-groups",
							      "numGroups",
							      "Current number of groups",
							      0, 0x100, 0,
							      G_PARAM_READABLE);

	GParamSpec *default_group_param_spec =
	    g_param_spec_uint("default-group",
			      "defaultGroup",
			      "Default group",
			      0, 0x100, 0,
			      G_PARAM_READABLE);

	GParamSpec *secondary_groups_mask_param_spec =
	    g_param_spec_uint("secondary-groups-mask",
			      "secondaryGroupsMask",
			      "Secondary groups mask",
			      0, 0x100, 0,
			      G_PARAM_READABLE);

	GParamSpec *indicators_handling_param_spec =
	    g_param_spec_boolean("indicators-handling",
				 "indicatorsHandling",
				 "Whether engine should handle indicators",
				 FALSE,
				 G_PARAM_READABLE);

	g_object_class_install_property(object_class,
					PROP_DISPLAY, display_param_spec);
	g_object_class_install_property(object_class,
					PROP_BACKEND_NAME,
					backend_name_param_spec);
	g_object_class_install_property(object_class, PROP_FEATURES,
					features_param_spec);
	g_object_class_install_property(object_class, PROP_MAX_NUM_GROUPS,
					max_num_groups_param_spec);
	g_object_class_install_property(object_class, PROP_NUM_GROUPS,
					num_groups_param_spec);
	g_object_class_install_property(object_class, PROP_DEFAULT_GROUP,
					default_group_param_spec);
	g_object_class_install_property(object_class,
					PROP_SECONDARY_GROUPS_MASK,
					secondary_groups_mask_param_spec);
	g_object_class_install_property(object_class,
					PROP_INDICATORS_HANDLING,
					indicators_handling_param_spec);


	g_signal_new("X-config-changed", XKL_TYPE_ENGINE,
		     G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET(XklEngineClass,
							config_notify),
		     NULL, NULL, NULL, G_TYPE_NONE, 0);

	g_signal_new("new-toplevel-window", XKL_TYPE_ENGINE,
		     G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET(XklEngineClass,
							new_window_notify),
		     NULL, NULL, NULL, G_TYPE_INT, 2, G_TYPE_LONG,
		     G_TYPE_LONG);

	g_signal_new("X-state-changed", XKL_TYPE_ENGINE,
		     G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET(XklEngineClass,
							state_notify),
		     NULL, NULL, NULL, G_TYPE_NONE, 3, state_change_type,
		     G_TYPE_INT, G_TYPE_BOOLEAN);

	/* 2 Windows passed */
	/* static stuff initialized */

	const gchar *sdl = g_getenv("XKL_DEBUG");

	if (sdl != NULL) {
		xkl_set_debug_level(atoi(sdl));
	}
}
