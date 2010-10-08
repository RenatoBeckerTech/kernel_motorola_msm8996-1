
#include "../sysfs.h"

/* Accelerometer types of attribute */
#define IIO_DEV_ATTR_ACCEL_OFFSET(_mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(accel_offset, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_X_OFFSET(_mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(accel_x_offset, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_Y_OFFSET(_mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(accel_y_offset, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_Z_OFFSET(_mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(accel_z_offset, _mode, _show, _store, _addr)

#define IIO_CONST_ATTR_ACCEL_SCALE(_string)		\
	IIO_CONST_ATTR(accel_scale, _string)

#define IIO_DEV_ATTR_ACCEL_SCALE(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_scale, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_X_SCALE(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_x_scale, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_Y_SCALE(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_y_scale, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_Z_SCALE(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_z_scale, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_CALIBBIAS(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_calibbias, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_X_CALIBBIAS(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_x_calibbias, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_Y_CALIBBIAS(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_y_calibbias, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_Z_CALIBBIAS(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_z_calibbias, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_CALIBSCALE(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_calibscale, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_X_CALIBSCALE(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_x_calibscale, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_Y_CALIBSCALE(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_y_calibscale, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_Z_CALIBSCALE(_mode, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(accel_z_calibscale, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL(_show, _addr)			\
	IIO_DEVICE_ATTR(accel_raw, S_IRUGO, _show, NULL, _addr)

#define IIO_DEV_ATTR_ACCEL_X(_show, _addr)			\
	IIO_DEVICE_ATTR(accel_x_raw, S_IRUGO, _show, NULL, _addr)

#define IIO_DEV_ATTR_ACCEL_Y(_show, _addr)			\
	IIO_DEVICE_ATTR(accel_y_raw, S_IRUGO, _show, NULL, _addr)

#define IIO_DEV_ATTR_ACCEL_Z(_show, _addr)			\
	IIO_DEVICE_ATTR(accel_z_raw, S_IRUGO, _show, NULL, _addr)

/* Thresholds are somewhat chip dependent - may need quite a few defs here */
/* For unified thresholds (shared across all directions */

/**
 * IIO_DEV_ATTR_ACCEL_THRESH: unified threshold
 * @_mode: read/write
 * @_show: read detector threshold value
 * @_store: write detector threshold value
 * @_addr: driver specific data, typically a register address
 *
 * This one is for cases where as single threshold covers all directions
 **/
#define IIO_DEV_ATTR_ACCEL_THRESH(_mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(thresh, _mode, _show, _store, _addr)

/**
 * IIO_DEV_ATTR_ACCEL_THRESH_X: independant direction threshold, x axis
 * @_mode: readable / writable
 * @_show: read x axis detector threshold value
 * @_store: write x axis detector threshold value
 * @_addr: device driver dependant, typically a register address
 **/
#define IIO_DEV_ATTR_ACCEL_THRESH_X(_mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(thresh_accel_x, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_THRESH_Y(_mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(thresh_accel_y, _mode, _show, _store, _addr)

#define IIO_DEV_ATTR_ACCEL_THRESH_Z(_mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(thresh_accel_z, _mode, _show, _store, _addr)

/**
 * IIO_EVENT_ATTR_ACCEL_X_HIGH: threshold event, x acceleration
 * @_show: read x acceleration high threshold
 * @_store: write x acceleration high threshold
 * @_mask: device dependant, typically a bit mask
 * @_handler: the iio_handler associated with this attribute
 **/
#define IIO_EVENT_ATTR_ACCEL_X_HIGH(_show, _store, _mask, _handler)	\
	IIO_EVENT_ATTR(accel_x_high, _show, _store, _mask, _handler)

/**
 * IIO_EVENT_ATTR_ACCEL_X_HIGH_SH: threshold event, x accel high, shared handler
 * @_evlist: event list used to share the handler
 * @_show: attribute read
 * @_store: attribute write
 * @_mask: driver specific data, typically a bit mask
 **/
#define IIO_EVENT_ATTR_ACCEL_X_HIGH_SH(_evlist, _show, _store, _mask)	\
	IIO_EVENT_ATTR_SH(accel_x_high, _evlist, _show, _store, _mask)

/**
 * IIO_EVENT_CODE_ACCEL_X_HIGH - event code for x axis high accel threshold
 **/
#define IIO_EVENT_ATTR_ACCEL_Y_HIGH(_show, _store, _mask, _handler)	\
	IIO_EVENT_ATTR(accel_y_high, _show, _store, _mask, _handler)

#define IIO_EVENT_ATTR_ACCEL_Y_HIGH_SH(_evlist, _show, _store, _mask)	\
	IIO_EVENT_ATTR_SH(accel_y_high, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_Z_HIGH(_show, _store, _mask, _handler)	\
	IIO_EVENT_ATTR(accel_z_high, _show, _store, _mask, _handler)

#define IIO_EVENT_ATTR_ACCEL_Z_HIGH_SH(_evlist, _show, _store, _mask)	\
	IIO_EVENT_ATTR_SH(accel_z_high, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_X_LOW(_show, _store, _mask, _handler)	\
	IIO_EVENT_ATTR(accel_x_low, _show, _store, _mask, _handler)

#define IIO_EVENT_ATTR_ACCEL_X_LOW_SH(_evlist, _show, _store, _mask)	\
	IIO_EVENT_ATTR_SH(accel_x_low, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_Y_LOW(_show, _store, _mask, _handler) \
	IIO_EVENT_ATTR(accel_y_low, _show, _store, _mask, _handler)

#define IIO_EVENT_ATTR_ACCEL_Y_LOW_SH(_evlist, _show, _store, _mask)\
	IIO_EVENT_ATTR_SH(accel_y_low, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_Z_LOW(_show, _store, _mask, _handler)	\
	IIO_EVENT_ATTR(accel_z_low, _show, _store, _mask, _handler)

#define IIO_EVENT_ATTR_ACCEL_Z_LOW_SH(_evlist, _show, _store, _mask)	\
	IIO_EVENT_ATTR_SH(accel_z_low, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_FREE_FALL_DETECT(_show, _store, _mask, _handler)	\
	IIO_EVENT_ATTR(free_fall, _show, _store, _mask, _handler)

#define IIO_EVENT_ATTR_FREE_FALL_DETECT_SH(_evlist, _show, _store, _mask) \
	IIO_EVENT_ATTR_SH(free_fall, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_X_ROC_HIGH_SH(_evlist, _show, _store, _mask) \
	IIO_EVENT_ATTR_SH(accel_x_roc_high, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_X_ROC_LOW_SH(_evlist, _show, _store, _mask) \
	IIO_EVENT_ATTR_SH(accel_x_roc_low, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_Y_ROC_HIGH_SH(_evlist, _show, _store, _mask) \
	IIO_EVENT_ATTR_SH(accel_y_roc_high, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_Y_ROC_LOW_SH(_evlist, _show, _store, _mask) \
	IIO_EVENT_ATTR_SH(accel_y_roc_low, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_Z_ROC_HIGH_SH(_evlist, _show, _store, _mask) \
	IIO_EVENT_ATTR_SH(accel_z_roc_high, _evlist, _show, _store, _mask)

#define IIO_EVENT_ATTR_ACCEL_Z_ROC_LOW_SH(_evlist, _show, _store, _mask) \
	IIO_EVENT_ATTR_SH(accel_z_roc_low, _evlist, _show, _store, _mask)
