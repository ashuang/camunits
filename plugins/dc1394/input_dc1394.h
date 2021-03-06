#ifndef __input_dc1394_h__
#define __input_dc1394_h__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A driver for FireWire cameras using libdc1394.
 */
enum {
    CAM_DC1394_MENU_OFF=0,
    CAM_DC1394_MENU_AUTO=1,
    CAM_DC1394_MENU_MANUAL=2,
};

enum {
    CAM_DC1394_TRIGGER_OFF=0,
    CAM_DC1394_TRIGGER_MODE_0=1,
    CAM_DC1394_TRIGGER_MODE_1=2,
    CAM_DC1394_TRIGGER_MODE_2=3,
    CAM_DC1394_TRIGGER_MODE_3=4,
    CAM_DC1394_TRIGGER_MODE_4=5,
    CAM_DC1394_TRIGGER_MODE_5=6,
    CAM_DC1394_TRIGGER_MODE_14=7,
    CAM_DC1394_TRIGGER_MODE_15=8,
};

enum {
    CAM_DC1394_TRIGGER_SOURCE_0=0,
    CAM_DC1394_TRIGGER_SOURCE_1=1,
    CAM_DC1394_TRIGGER_SOURCE_2=2,
    CAM_DC1394_TRIGGER_SOURCE_3=3,
    CAM_DC1394_TRIGGER_SOURCE_SOFTWARE=4,
};

#ifdef __cplusplus
}
#endif

#endif
