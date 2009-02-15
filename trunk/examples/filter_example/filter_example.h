#ifndef __my_filter_example_h__
#define __my_filter_example_h__

#include <camunits/unit.h>
#include <camunits/unit_driver.h>

#ifdef __cplusplus
extern "C" {
#endif

CamUnit * my_filter_example_new (void);

CamUnitDriver * my_filter_example_driver_new(void);

#ifdef __cplusplus
}
#endif

#endif
