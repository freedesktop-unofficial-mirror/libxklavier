#ifndef __XKL_CONFIG_REC_H__
#define __XKL_CONFIG_REC_H__

#include <glib-object.h>
#include <libxklavier/xkl_engine.h>

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

	typedef struct _XklConfigRec XklConfigRec;
	typedef struct _XklConfigRecClass XklConfigRecClass;

#define XKL_TYPE_CONFIG_REC             (xkl_config_rec_get_type ())
#define XKL_CONFIG_REC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), XKL_TYPE_CONFIG_REC, XklConfigRec))
#define XKL_CONFIG_REC_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), XKL_CONFIG_REC,  XklConfigRecClass))
#define XKL_IS_CONFIG_REC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XKL_TYPE_CONFIG_REC))
#define XKL_IS_CONFIG_REC_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), XKL_TYPE_CONFIG_REC))
#define XKL_CONFIG_REC_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), XKL_TYPE_CONFIG_REC, XklConfigRecClass))

/*
 * Basic configuration params
 */
	struct _XklConfigRec {
/*
 * The superclass object
 */
		GObject parent;
/*
 * The keyboard model
 */
		gchar *model;
/*
 * The array of keyboard layouts
 */
		gchar **layouts;
/*
 * The array of keyboard layout variants (if any)
 */
		gchar **variants;
/*
 * The array of keyboard layout options
 */
		gchar **options;
	};

/*
 * The XklConfigRec class, derived from GObject
 */
	struct _XklConfigRecClass {
		/*
		 * The superclass
		 */
		GObjectClass parent_class;
	};

/**
 * xkl_config_rec_get_type:
 *
 * Get type info for XConfigRec
 *
 * Returns: GType for XConfigRec
 */
	extern GType xkl_config_rec_get_type(void);

/**
 * xkl_config_rec_new:
 *
 * Create new XklConfigRec
 *
 * Returns: new instance
 */
	extern XklConfigRec *xkl_config_rec_new(void);

/**
 * xkl_config_rec_activate:
 * @data: valid XKB configuration
 * @engine: the engine
 *
 * Activates some XKB configuration
 * description. Can be NULL
 *
 * Returns: TRUE on success
 */
	extern gboolean xkl_config_rec_activate(const XklConfigRec * data,
						XklEngine * engine);

/**
 * xkl_config_rec_get_from_server:
 * @data: buffer for XKB configuration
 * @engine: the engine
 *
 * Loads the current XKB configuration (from X server)
 *
 * Returns: TRUE on success
 */
	extern gboolean xkl_config_rec_get_from_server(XklConfigRec * data,
						       XklEngine * engine);

/**
 * xkl_config_rec_get_from_backup:
 * @data: buffer for XKB configuration
 * @engine: the engine
 *
 * Loads the current XKB configuration (from backup)
 *
 * Returns: TRUE on success
 */
	extern gboolean xkl_config_rec_get_from_backup(XklConfigRec * data,
						       XklEngine * engine);

/**
 * xkl_config_rec_write_to_file:
 * @file_name: name of the file to create
 * @data: valid XKB configuration
 * description. Can be NULL
 * @binary: flag indicating whether the output file should be binary
 * @engine: the engine
 *
 * Writes some XKB configuration into XKM/XKB/... file
 *
 * Returns: TRUE on success
 */
	extern gboolean xkl_config_rec_write_to_file(XklEngine * engine,
						     const gchar *
						     file_name,
						     const XklConfigRec *
						     data,
						     const gboolean
						     binary);

/**
 * xkl_config_rec_get_from_root_window_property:
 * @rules_atom_name: atom name of the root window property to read
 * @rules_file_out: pointer to hold the file name
 * @config_out: buffer to hold the result
 * @engine: the engine
 *
 * Gets the XKB configuration from any root window property
 *
 * Returns: TRUE on success
 */
	extern gboolean
	    xkl_config_rec_get_from_root_window_property(XklConfigRec *
							 config_out,
							 Atom
							 rules_atom_name,
							 gchar **
							 rules_file_out,
							 XklEngine *
							 engine);

/**
 * xkl_config_rec_set_to_root_window_property:
 * @rules_atom_name: atom name of the root window property to write
 * @rules_file: rules file name
 * @config: configuration to save 
 * @engine: the engine
 *
 * Saves the XKB configuration into any root window property
 *
 * Returns: TRUE on success
 */
	extern gboolean xkl_config_rec_set_to_root_window_property(const
								   XklConfigRec
								   *
								   config,
								   Atom
								   rules_atom_name,
								   gchar *
								   rules_file,
								   XklEngine
								   *
								   engine);

/**
 * xkl_backup_names_prop:
 * @engine: the engine
 *
 * Backups current XKB configuration into some property - 
 * if this property is not defined yet.
 *
 * Returns: TRUE on success
 */
	extern gboolean xkl_backup_names_prop(XklEngine * engine);

/**
 * xkl_restore_names_prop:
 * @engine: the engine
 *
 * Restores XKB from the property saved by xkl_backup_names_prop
 *
 * Returns: TRUE on success
 */
	extern gboolean xkl_restore_names_prop(XklEngine * engine);

/**
 * xkl_config_rec_reset:
 * @data: record to reset
 *
 * Resets the record (equal to Destroy and Init)
 */
	extern void xkl_config_rec_reset(XklConfigRec * data);

/**
 * xkl_config_rec_equals:
 * @data1: record to compare
 * @data2: another record
 *
 * Compares two records
 *
 * Returns: TRUE if records are same
 */
	extern gboolean xkl_config_rec_equals(XklConfigRec * data1,
					      XklConfigRec * data2);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif