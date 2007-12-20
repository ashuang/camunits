#ifndef __my_filter_example_h__
#define __my_filter_example_h__

#include <glib-object.h>

#include <libcam/unit.h>

G_BEGIN_DECLS

// This file demonstrates how to create a simple CamUnit subclass.  When
// implementing a custom MyUnit, replace "FilterExample", "FILTER_EXAMPLE",
// and "filter_example" with your own names.  You'll also want to pick a
// different namespace (i.e. prefix) from "My"

typedef struct _MyFilterExample MyFilterExample;
typedef struct _MyFilterExampleClass MyFilterExampleClass;

// boilerplate.
#define MY_TYPE_FILTER_EXAMPLE  my_filter_example_get_type()
#define MY_FILTER_EXAMPLE(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        MY_TYPE_FILTER_EXAMPLE, MyFilterExample))
#define MY_FILTER_EXAMPLE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            MY_TYPE_FILTER_EXAMPLE, MyFilterExampleClass ))
#define IS_MY_FILTER_EXAMPLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            MY_TYPE_FILTER_EXAMPLE ))
#define IS_MY_FILTER_EXAMPLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), MY_TYPE_FILTER_EXAMPLE))
#define MY_FILTER_EXAMPLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            MY_TYPE_FILTER_EXAMPLE, MyFilterExampleClass))

// object definition struct.  member variables go in here
struct _MyFilterExample {
    // the first member must always be the superclass struct
    CamUnit parent;

    // add one member variable
    CamUnitControl *patch_intensity_control;
};

// class definition.  This is pretty much a vtable, and you will rarely need to
// change it
struct _MyFilterExampleClass {
    CamUnitClass parent_class;
};

GType my_filter_example_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
MyFilterExample * my_filter_example_new();

CamUnitDriver * my_filter_example_driver_new (void);

G_END_DECLS

#endif
