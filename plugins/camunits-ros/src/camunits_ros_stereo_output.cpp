#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <camunits/plugin.h>

#include <ros/ros.h>

#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/fill_image.h>
#include <camera_info_manager/camera_info_manager.h>
#include <image_transport/image_transport.h>

#define DEFAULT_DATA_RATE_UPDATE_INTERVAL_USEC 1000000
#define DATA_RATE_UPDATE_ALPHA 0.7

#define CONTROL_PUBLISH "publish"
#define CONTROL_BASE_NAME "base_name"
#define CONTROL_FRAME_ID "frame_id"
#define CONTROL_DATA_RATE_INFO "data-rate"

#define CONTROL_LEFT_CALIB_FILE "left_calib_file"
#define CONTROL_RIGHT_CALIB_FILE "right_calib_file"

static inline int64_t timestamp_now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

G_BEGIN_DECLS

  typedef struct _RosStereoOutput RosStereoOutput;
  typedef struct _RosStereoOutputClass RosStereoOutputClass;

  // boilerplate
#define ROS_TYPE_OUTPUT  ros_stereo_output_get_type()
#define ROS_STEREO_OUTPUT(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
    ROS_TYPE_OUTPUT, RosStereoOutput))
#define ROS_STEREO_OUTPUT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
    ROS_TYPE_OUTPUT, RosStereoOutputClass ))
#define IS_ROS_STEREO_OUTPUT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    ROS_TYPE_OUTPUT ))
#define IS_ROS_STEREO_OUTPUT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
    (klass), ROS_TYPE_OUTPUT))
#define ROS_STEREO_OUTPUT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
    ROS_TYPE_OUTPUT, RosStereoOutputClass))

  void cam_plugin_initialize(GTypeModule * module);
  CamUnitDriver * cam_plugin_create(GTypeModule * module);

  class RosCameraDriver {
  private:
    ros::NodeHandle privNH_left; // private node handle
    ros::NodeHandle privNH_right; // private node handle
    image_transport::ImageTransport *it_left;
    image_transport::ImageTransport *it_right;
    sensor_msgs::Image left_image_;
    sensor_msgs::Image right_image_;
    sensor_msgs::CameraInfo left_cam_info_;
    sensor_msgs::CameraInfo right_cam_info_;

    CameraInfoManager *cinfo_left;
    CameraInfoManager *cinfo_right;

  public:
    bool publish;
    bool params_updated;

    /** image transport publish interface */
    image_transport::CameraPublisher left_image_pub_;
    image_transport::CameraPublisher right_image_pub_;

  public:

    RosCameraDriver() :
      cinfo_left(NULL), cinfo_right(NULL)
    {
      privNH_left = ros::NodeHandle("camunits/left");
      privNH_right = ros::NodeHandle("camunits/right");
      it_left = new image_transport::ImageTransport(privNH_left);
      it_right = new image_transport::ImageTransport(privNH_right);
      params_updated = true;
    }

    ~RosCameraDriver()
    {
      delete cinfo_left;
      delete cinfo_right;

      delete it_left;
      delete it_right;

    }

    void updateAdvertisements(RosStereoOutput * self);
    void processFrame(RosStereoOutput * self, const CamFrameBuffer *inbuf, const CamUnitFormat *infmt);

  };

  struct _RosStereoOutput {
    CamUnit parent;

    RosCameraDriver * rcd;

    CamUnitControl *publish_ctl;

    CamUnitControl *ros_frame_id_ctl;
    CamUnitControl *ros_base_name_ctl;
    CamUnitControl *data_rate_ctl;

    CamUnitControl *left_calibfile_ctl;
    CamUnitControl *right_calibfile_ctl;

    int64_t bytes_transferred_since_last_data_rate_update;
    int64_t last_data_rate_time;
    int64_t data_rate_update_interval;
    double data_rate;

  };

  struct _RosStereoOutputClass {
    CamUnitClass parent_class;
  };

  GType ros_stereo_output_get_type(void);

  static RosStereoOutput * ros_stereo_output_new(void);
  static void on_input_frame_ready(CamUnit * super, const CamFrameBuffer *inbuf, const CamUnitFormat *infmt);
  static void on_input_format_changed(CamUnit *super, const CamUnitFormat *infmt);
  static int _stream_init(CamUnit * super, const CamUnitFormat * format);
  static int _stream_shutdown(CamUnit * super);
  static gboolean _try_set_control(CamUnit *super, const CamUnitControl *ctl, const GValue *proposed, GValue *actual);
  static void _finalize(GObject * obj);

G_END_DECLS CAM_PLUGIN_TYPE(RosStereoOutput, ros_stereo_output, CAM_TYPE_UNIT)
;

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize(GTypeModule * module)
{
  ros_stereo_output_register_type(module);
}

CamUnitDriver *
cam_plugin_create(GTypeModule * module)
{
  return cam_unit_driver_new_stock_full("ROS", "ros_stereo_output", "Ros Stereo Output", 0,
      (CamUnitConstructor) ros_stereo_output_new, module);
}

static void ros_stereo_output_class_init(RosStereoOutputClass *klass)
{
  klass->parent_class.on_input_frame_ready = on_input_frame_ready;
  klass->parent_class.stream_init = _stream_init;
  klass->parent_class.stream_shutdown = _stream_shutdown;
  klass->parent_class.try_set_control = _try_set_control;

  GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->finalize = _finalize;
}

static void _finalize(GObject * obj)
{
  RosStereoOutput * self = ROS_STEREO_OUTPUT(obj);
  delete self->rcd;
  ros::shutdown();
  G_OBJECT_CLASS(ros_stereo_output_parent_class)->finalize(obj);
}

static void ros_stereo_output_init(RosStereoOutput *self)
{
  // constructor.  Initialize the unit with some reasonable defaults here.
  CamUnit *super = CAM_UNIT(self);

  self->publish_ctl = cam_unit_add_control_boolean(super, CONTROL_PUBLISH, "Publish", 1, 1);

  self->ros_frame_id_ctl = cam_unit_add_control_string(super, CONTROL_FRAME_ID, "camera frame_id", "camunits", 1);

  self->ros_base_name_ctl = cam_unit_add_control_string(super, CONTROL_BASE_NAME, "ros base_name", "image_raw", 1);

  self->left_calibfile_ctl = cam_unit_add_control_string(super, CONTROL_LEFT_CALIB_FILE, "left calib file",
      "left_calib.yaml", 1);
  self->right_calibfile_ctl = cam_unit_add_control_string(super, CONTROL_RIGHT_CALIB_FILE, "right calib file",
      "right_calib.yaml", 1);

  self->data_rate_ctl = cam_unit_add_control_string(super, CONTROL_DATA_RATE_INFO, "Data Rate", "0", 0);
  self->data_rate = 0;
  self->bytes_transferred_since_last_data_rate_update = 0;
  self->last_data_rate_time = 0;
  self->data_rate_update_interval = DEFAULT_DATA_RATE_UPDATE_INTERVAL_USEC;

  //TODO:is there a way to pass in/access the command line arguments?
  int argc = 0;
  char * argv[1] = { (char *) "camunit" };
  ros::init(argc, argv, "camunits");

  self->rcd = new RosCameraDriver();

  g_signal_connect(G_OBJECT(self), "input-format-changed", G_CALLBACK(on_input_format_changed), self);
}

static RosStereoOutput *
ros_stereo_output_new()
{
  return ROS_STEREO_OUTPUT(g_object_new(ROS_TYPE_OUTPUT, NULL));
}

static int _stream_init(CamUnit * super, const CamUnitFormat * fmt)
{
  //  RosStereoOutput *self = ROS_STEREO_OUTPUT(super);

  return 0;
}

static int _stream_shutdown(CamUnit * super)
{
  //  RosStereoOutput *self = ROS_STEREO_OUTPUT(super);

  return 0;
}

void RosCameraDriver::updateAdvertisements(RosStereoOutput * self)
{

  left_image_pub_.shutdown();
  right_image_pub_.shutdown();
  if (publish && cam_unit_control_get_boolean(self->publish_ctl)) {
    const char * base_name = cam_unit_control_get_string(self->ros_base_name_ctl);
    left_image_pub_ = it_left->advertiseCamera(base_name, 1);
    right_image_pub_ = it_right->advertiseCamera(base_name, 1);

    char calib_file[1024];
    sprintf(calib_file, "file://%s", cam_unit_control_get_string(self->left_calibfile_ctl));
    if (cinfo_left != NULL)
      delete cinfo_left;
    cinfo_left = new CameraInfoManager(privNH_left, "left", calib_file);

    if (cinfo_right != NULL)
      delete cinfo_right;
    sprintf(calib_file, "file://%s", cam_unit_control_get_string(self->right_calibfile_ctl));
    cinfo_right = new CameraInfoManager(privNH_right, "right", calib_file);
  }
}

/*
 * convert the camunits formatted image to ROS and publish
 */
void RosCameraDriver::processFrame(RosStereoOutput * self, const CamFrameBuffer *inbuf, const CamUnitFormat *infmt)
{
  if (params_updated) {
    updateAdvertisements(self);
    params_updated = false;
  }
  if (!publish || !cam_unit_control_get_boolean(self->publish_ctl))
    return;

  // put the camunits image into the ROS format
  switch (infmt->pixelformat) {
  case CAM_PIXEL_FORMAT_GRAY:
    left_image_.encoding = right_image_.encoding = sensor_msgs::image_encodings::MONO8;
    break;
  case CAM_PIXEL_FORMAT_BGR:
    left_image_.encoding = right_image_.encoding = sensor_msgs::image_encodings::BGR8;
    break;
  case CAM_PIXEL_FORMAT_RGB:
    left_image_.encoding = right_image_.encoding = sensor_msgs::image_encodings::RGB8;
    break;
  default:
    fprintf(stderr, "ERROR: trying to publish invalid image format\n");
    assert(false);
  }

  left_cam_info_ = cinfo_left->getCameraInfo();
  right_cam_info_ = cinfo_right->getCameraInfo();

  //TODO:Not entirely sure what frame_id_ should be set to...
  const char * frame_id = cam_unit_control_get_string(self->ros_frame_id_ctl);
  left_image_.header.frame_id = frame_id;
  left_cam_info_.header.frame_id = frame_id;
  right_image_.header.frame_id = frame_id;
  right_cam_info_.header.frame_id = frame_id;

  //Split image into left image and right image
  left_cam_info_.header.stamp = right_cam_info_.header.stamp = left_image_.header.stamp = right_image_.header.stamp
      = ros::Time((double) inbuf->timestamp * 1.e-6);

  left_cam_info_.width = right_cam_info_.width = left_image_.width = right_image_.width = infmt->width;
  left_cam_info_.height = right_cam_info_.height = left_image_.height = right_image_.height = infmt->height / 2;
  left_image_.step = right_image_.step = infmt->row_stride;
  int image_size = left_image_.step * left_image_.height;
  left_image_.set_data_size(image_size);
  right_image_.set_data_size(image_size);
  memcpy(&left_image_.data[0], inbuf->data, image_size); // the image of left camera is the first half of the deinterlaced image.
  memcpy(&right_image_.data[0], inbuf->data + image_size, image_size); // the image of right camera is the second half of the deinterlaced image.

  // Publish it via image_transport
  left_image_pub_.publish(left_image_, left_cam_info_);
  right_image_pub_.publish(right_image_, right_cam_info_);

  ros::spinOnce(); //handle any incoming ROS stuff (like parameter updates)...
}

static void on_input_frame_ready(CamUnit *super, const CamFrameBuffer *inbuf, const CamUnitFormat *infmt)
{
  RosStereoOutput *self = ROS_STEREO_OUTPUT(super);

  //convert frame to the ROS format and publish
  self->rcd->processFrame(self, inbuf, infmt);

  int64_t now = timestamp_now();

  //update the dataRate
  self->bytes_transferred_since_last_data_rate_update += inbuf->bytesused;
  if (now > self->last_data_rate_time + self->data_rate_update_interval) {
    int64_t dt_usec = now - self->last_data_rate_time;
    double dt = dt_usec * 1e-6;
    double data_rate_instant = self->bytes_transferred_since_last_data_rate_update * 1e-6 / dt;
    self->data_rate = data_rate_instant * DATA_RATE_UPDATE_ALPHA + (1 - DATA_RATE_UPDATE_ALPHA) * self->data_rate;

    char text[80];
    snprintf(text, sizeof(text), "%5.3f MB/s", self->data_rate);

    self->last_data_rate_time = now;
    self->bytes_transferred_since_last_data_rate_update = 0;

    cam_unit_control_force_set_string(self->data_rate_ctl, text);
  }

  cam_unit_produce_frame(super, inbuf, infmt);

}

static void on_input_format_changed(CamUnit *super, const CamUnitFormat *infmt)
{
  RosStereoOutput *self = ROS_STEREO_OUTPUT(super);
  cam_unit_remove_all_output_formats(CAM_UNIT(self));
  if (!infmt)
    return;
  switch (infmt->pixelformat) {
  //valid formats:
  case CAM_PIXEL_FORMAT_GRAY:
  case CAM_PIXEL_FORMAT_BGR:
  case CAM_PIXEL_FORMAT_RGB:
    cam_unit_add_output_format(CAM_UNIT(self), infmt->pixelformat, NULL, infmt->width, infmt->height, infmt->row_stride);
    self->rcd->publish = true;
    self->rcd->updateAdvertisements(self);
    break;

  default: //TODO: handle other formats?
    self->rcd->publish = false;
    self->rcd->updateAdvertisements(self);
    return;
  }
}

static gboolean _try_set_control(CamUnit *super, const CamUnitControl *ctl, const GValue *proposed, GValue *actual)
{
  RosStereoOutput * self = ROS_STEREO_OUTPUT(super);

  g_value_copy(proposed, actual);

  self->rcd->params_updated = true;
  return TRUE;
}
