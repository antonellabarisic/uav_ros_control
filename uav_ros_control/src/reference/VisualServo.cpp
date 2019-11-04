//
// Created by robert on 20. 09. 2019..
//

#include <uav_ros_control/reference/VisualServo.h>
#include <math.h>

// Define all parameter paths here
#define VS_P_GAIN_X_PARAM          "reference/p_gain_x"
#define VS_I_GAIN_X_PARAM          "reference/i_gain_x"
#define VS_D_GAIN_X_PARAM          "reference/d_gain_x"
#define VS_I_CLAMP_X_PARAM         "reference/i_clamp_x"
#define VS_OFFSET_X_1_PARAM        "reference/offset_x_1"
#define VS_OFFSET_X_2_PARAM        "reference/offset_x_2"
#define VS_DEADZONE_X_PARAM        "reference/deadzone_x"
#define VS_LANDING_RANGE_X_PARAM   "reference/landing_range_x"

#define VS_P_GAIN_Y_PARAM          "reference/p_gain_y"
#define VS_I_GAIN_Y_PARAM          "reference/p_gain_y"
#define VS_D_GAIN_Y_PARAM          "reference/d_gain_y"
#define VS_I_CLAMP_Y_PARAM         "reference/i_clamp_y"
#define VS_OFFSET_Y_1_PARAM        "reference/offset_y_1"
#define VS_OFFSET_Y_2_PARAM        "reference/offset_y_2"
#define VS_DEADZONE_Y_PARAM        "reference/deadzone_y"
#define VS_LANDING_RANGE_Y_PARAM   "reference/landing_range_y"

#define VS_P_GAIN_Z_PARAM          "reference/p_gain_z"
#define VS_I_GAIN_Z_PARAM          "reference/p_gain_z"
#define VS_D_GAIN_Z_PARAM          "reference/d_gain_z"
#define VS_I_CLAMP_Z_PARAM         "reference/i_clamp_z"
#define VS_OFFSET_Z_1_PARAM        "reference/offset_z_1"
#define VS_OFFSET_Z_2_PARAM        "reference/offset_z_2"
#define VS_DEADZONE_Z_PARAM        "reference/deadzone_z"
#define VS_LANDING_RANGE_Z_PARAM   "reference/landing_range_z"

#define VS_P_GAIN_YAW_PARAM        "reference/p_gain_yaw"
#define VS_I_GAIN_YAW_PARAM        "reference/i_gain_yaw"
#define VS_I_CLAMP_YAW_PARAM       "reference/i_clamp_yaw"
#define VS_I_DEADZONE_YAW_PARAM    "reference/i_deadzone_yaw"
#define VS_D_GAIN_YAW_PARAM        "reference/d_gain_yaw"
#define VS_LANDING_RANGE_YAW_PARAM "reference/lending_range_yaw"

#define VS_P_GAIN_DIST_PARAM       "reference/p_gain_dist"
#define VS_I_GAIN_DIST_PARAM       "reference/i_gain_dist"
#define VS_D_GAIN_DIST_PARAM       "reference/d_gain_dist"
#define VS_I_CLAMP_DIST_PARAM      "reference/i_clamp_dist"
#define VS_DEADZONE_DIST_PARAM     "reference/deadzone_dist"

#define VS_MOVE_SATURATION_PARAM   "reference/move_saturation"
#define VS_YAW_DIFFERENCE_PARAM    "reference/yaw_difference"

#define VS_LANDING_SPEED_PARAM     "reference/landing_speed"
#define VS_IS_BRICK_LAYING_PARAM   "reference/is_brick_laying"

namespace uav_reference {

VisualServo::VisualServo(ros::NodeHandle& nh) {

  // Define Publishers
  _pubXError = nh.advertise<std_msgs::Float32>("visual_servo/x_error", 1);
  _pubYError = nh.advertise<std_msgs::Float32>("visual_servo/y_error", 1);
  _pubIsEnabledTopic = nh.advertise<std_msgs::Bool>("visual_servo/is_enabled", 1);
  _pubMoveLeft = nh.advertise<std_msgs::Float32>("move_left", 1);
  _pubChangeYaw = nh.advertise<std_msgs::Float32>("change_yaw", 1);
  _pubMoveForward = nh.advertise<std_msgs::Float32>("move_forward", 1);
  _pubUavYawDebug = nh.advertise<std_msgs::Float32>("debug/Uav_yaw", 1);
  _pubYawErrorDebug = nh.advertise<std_msgs::Float32>("debug/yaw_error", 1);
  _pubChangeYawDebug = nh.advertise<std_msgs::Float32>("debug/yaw_change", 1); // Advertised again for user friendliness
  _pubUavRollDebug = nh.advertise<std_msgs::Float32>("debug/uav_roll", 1);
  _pubUavPitchDebug = nh.advertise<std_msgs::Float32>("debug/uav_pitch", 1);

  _pubNewSetpoint =
      nh.advertise<trajectory_msgs::MultiDOFJointTrajectoryPoint>("position_hold/trajectory", 1);

  // Define Subscribers
  _subOdom =
      nh.subscribe("odometry", 1, &uav_reference::VisualServo::odomCb, this);
  _subXError =
      nh.subscribe("x_error", 1, &uav_reference::VisualServo::xErrorCb, this);
  _subYError =
      nh.subscribe("y_error", 1, &uav_reference::VisualServo::yErrorCb, this);
  _subYawError =
      nh.subscribe("yaw_error", 1, &uav_reference::VisualServo::yawErrorCb, this);
  _subNContours =
      nh.subscribe("n_contours", 1, &uav_reference::VisualServo::nContoursCb, this);
  _subXOffset =
      nh.subscribe("x_offset", 1, &uav_reference::VisualServo::xOffsetCb, this);
  _subYOffset =
      nh.subscribe("y_offset", 1, &uav_reference::VisualServo::yOffsetCb, this);
  _subVisualServoProcessValuesMsg =
      nh.subscribe("VisualServoProcessValueTopic", 1, &uav_reference::VisualServo::VisualServoProcessValuesCb, this);

  // Setup dynamic reconfigure

  _VSParamCallback = boost::bind(&VisualServo::visualServoParamsCb, this, _1, _2);
  _VSConfigServer.setCallback(_VSParamCallback);

  _serviceStartVisualServo = nh.advertiseService(
      "visual_servo",
      &uav_reference::VisualServo::startVisualServoServiceCb,
      this);


  _new_point.transforms = std::vector<geometry_msgs::Transform>(1);
  _new_point.velocities = std::vector<geometry_msgs::Twist>(1);
  _new_point.accelerations = std::vector<geometry_msgs::Twist>(1);

  _x_frozen = false;
  _y_frozen = false;
  _yaw_frozen = false;
}

VisualServo::~VisualServo() {}

bool uav_reference::VisualServo::startVisualServoServiceCb(std_srvs::SetBool::Request &request,
                                                           std_srvs::SetBool::Response &response) {
  if (request.data) {
    if (_n_contours) {
      if (!isVisualServoEnabled()) ROS_INFO("UAV VisualServo - enabling visual servo.");
      _visualServoEnabled = true;
      response.message = "Visual servo enabled.";
    }
    else {
      ROS_ERROR("The color filter reports no contours. Cannot enable visual servo.");
      response.message = "The color filter reports no contours. Cannot enable visual servo.";
    }
  }
  else {
    if(isVisualServoEnabled()) ROS_INFO("UAV VisualServo - disabling visual servo.");
    _visualServoEnabled = false;
    _yaw_PID.resetIntegrator();
    _x_axis_PID.resetIntegrator();
    _y_axis_PID.resetIntegrator();
    response.message = "Visual servo disabled.";
  }
  response.success = _visualServoEnabled;
  return true;
}

void VisualServo::visualServoParamsCb(uav_ros_control::VisualServoParametersConfig &configMsg,
                                                     uint32_t level) {
  ROS_WARN("VisualServo::parametersCallback");

  if (configMsg.groups.general_parameters.enable) {
      _setBoolRequest.data = true;
      startVisualServoServiceCb(_setBoolRequest, _setBoolResponse);
  }
  else {
      _setBoolRequest.data = false;
      startVisualServoServiceCb(_setBoolRequest, _setBoolResponse);
  }

  _offset_x= configMsg.groups.x_axis.offset_x;
  _deadzone_x = configMsg.groups.x_axis.deadzone_x;

  _offset_y= configMsg.groups.y_axis.offset_y;
  _deadzone_y = configMsg.groups.y_axis.deadzone_y;

  _deadzone_yaw  = configMsg.groups.yaw_control.deadzone_yaw;

  _x_axis_PID.set_kp(configMsg.groups.x_axis.k_p_x);
  _x_axis_PID.set_ki(configMsg.groups.x_axis.k_i_x);
  _x_axis_PID.set_kd(configMsg.groups.x_axis.k_d_x);
  _x_axis_PID.set_lim_high(configMsg.groups.x_axis.saturation_x);
  _x_axis_PID.set_lim_low(-configMsg.groups.x_axis.saturation_x);

  if (!configMsg.groups.x_axis.x_armed) {
    _x_axis_PID.set_kp(0);
    _x_axis_PID.set_ki(0);
    _x_axis_PID.set_kd(0);
    _x_axis_PID.resetIntegrator();
  }

  _y_axis_PID.set_kp(configMsg.groups.y_axis.k_p_y);
  _y_axis_PID.set_ki(configMsg.groups.y_axis.k_i_y);
  _y_axis_PID.set_kd(configMsg.groups.y_axis.k_d_y);
  _y_axis_PID.set_lim_high(configMsg.groups.y_axis.saturation_y);
  _y_axis_PID.set_lim_low(-configMsg.groups.y_axis.saturation_y);

  if (!configMsg.groups.y_axis.y_armed) {
    _y_axis_PID.set_kp(0);
    _y_axis_PID.set_ki(0);
    _y_axis_PID.set_kd(0);
    _y_axis_PID.resetIntegrator();
  }

  _yaw_PID.set_kp(configMsg.groups.yaw_control.k_p_yaw);
  _yaw_PID.set_ki(configMsg.groups.yaw_control.k_i_yaw);
  _yaw_PID.set_kd(configMsg.groups.yaw_control.k_d_yaw);
  _yaw_PID.set_lim_high(configMsg.groups.yaw_control.saturation_yaw);
  _yaw_PID.set_lim_low(-configMsg.groups.yaw_control.saturation_yaw);

  if (!configMsg.groups.yaw_control.yaw_armed) {
    _yaw_PID.set_kp(0);
    _yaw_PID.set_ki(0);
    _yaw_PID.set_kd(0);
    _yaw_PID.resetIntegrator();
  }

  _compensate_roll_and_pitch = configMsg.groups.general_parameters.compensate_roll_and_pitch;
  _camera_fov = configMsg.groups.general_parameters.camera_fov * M_PI / 180.0;
}

void VisualServo::odomCb(const nav_msgs::OdometryConstPtr& odom) {

    _qx = odom->pose.pose.orientation.x;
    _qy = odom->pose.pose.orientation.y;
    _qz = odom->pose.pose.orientation.z;
    _qw = odom->pose.pose.orientation.w;

    _uavRoll = atan2( 2*(_qw * _qx + _qy * _qz), 1 - 2 * (_qx*_qx + _qy*_qy) );
    _uavPitch = asin( 2*(_qw*_qy - _qx*_qz) );

    _floatMsg.data = _uavRoll;
    _pubUavRollDebug.publish(_floatMsg);
    _floatMsg.data = _uavPitch;
    _pubUavPitchDebug.publish(_floatMsg);
}

void VisualServo::xErrorCb(const std_msgs::Float32 &data) {
  _error_x = data.data;

  if (_compensate_roll_and_pitch){
      _error_x += tan(_uavRoll)/tan(_camera_fov);
  }

  _floatMsg.data = _error_x - _offset_x;
  _pubXError.publish(_floatMsg);
}

void VisualServo::yErrorCb(const std_msgs::Float32 &data) {
  _error_y = data.data;

  if(_compensate_roll_and_pitch) {
      _error_y += tan(_uavPitch)/tan(_camera_fov);
  }

  _floatMsg.data = _error_y - _offset_y;
  _pubYError.publish(_floatMsg);
}

void VisualServo::yawErrorCb(const std_msgs::Float32 &data) {
  _error_yaw = -data.data;
  _pubYawErrorDebug.publish(data);
}

void VisualServo::nContoursCb(const std_msgs::Int32 &data) {
  _n_contours = data.data;
  if (!_n_contours){
      if (isVisualServoEnabled()){
          ROS_ERROR("Lost sight of object!");
          _setBoolRequest.data = false;
          startVisualServoServiceCb(_setBoolRequest, _setBoolResponse);
      }
  }
}

void VisualServo::VisualServoProcessValuesCb(const uav_ros_control_msgs::VisualServoProcessValues &msg) {
    if (msg.x == 0.0) {
        _x_frozen = true;
    }
    if (!_x_frozen) {
        _uavPos[0] = msg.x;
    }

    if (msg.y == 0.0) {
        _y_frozen = true;
    }
    if (!_y_frozen) {
        _uavPos[1] = msg.y;
    }

    if (msg.yaw == 0.0) {
        _yaw_frozen = true;
    }
    if (!_yaw_frozen) {
        _uavYaw = msg.yaw;
    }
    _floatMsg.data = _uavYaw;
    _pubUavYawDebug.publish(_floatMsg);

    _uavPos[2] = msg.z;
}

void VisualServo::xOffsetCb(const std_msgs::Float32 &msg) {
    _offset_x = msg.data;
}

void VisualServo::yOffsetCb(const std_msgs::Float32 &msg){
    _offset_y = msg.data;
}

void VisualServo::updateSetpoint() {

  double move_forward = 0.0;
  double move_left = 0.0;
  double change_yaw = 0.0;

  if (!_x_frozen) move_left = _x_axis_PID.compute(_offset_x, _error_x, 1 / _rate);
  if (!_y_frozen) move_forward  = _y_axis_PID.compute(_offset_y, _error_y, 1 / _rate);
  if (!_yaw_frozen) change_yaw = _yaw_PID.compute(0, _error_yaw, 1 / _rate);

  _floatMsg.data = change_yaw;
  _pubChangeYawDebug.publish(_floatMsg);

  _setpointPosition[0] = _uavPos[0] + move_forward * cos(_uavYaw);
  _setpointPosition[0] -= move_left * sin(_uavYaw);
  _setpointPosition[1] = _uavPos[1] + move_forward * sin(_uavYaw);
  _setpointPosition[1] += move_left * cos(_uavYaw);
  _setpointPosition[2] = _uavPos[2];

  _setpointYaw = _uavYaw + change_yaw;

  _moveLeftMsg.data = move_left;
  _changeYawMsg.data = change_yaw;
  _moveForwardMsg.data = move_forward;

  _pubMoveLeft.publish(_moveLeftMsg);
  _pubChangeYaw.publish(_changeYawMsg);
  _pubMoveForward.publish(_moveForwardMsg);

}

void VisualServo::publishNewSetpoint() {

  tf2::Quaternion q;
  q.setEulerZYX(_setpointYaw, 0.0, 0.0);

  _new_point.transforms[0].translation.x = _setpointPosition[0];
  _new_point.transforms[0].translation.y = _setpointPosition[1];
  _new_point.transforms[0].translation.z = _setpointPosition[2];
  _new_point.transforms[0].rotation.x = q.getX();
  _new_point.transforms[0].rotation.y = q.getY();
  _new_point.transforms[0].rotation.z = q.getZ();
  _new_point.transforms[0].rotation.w = q.getW();

  _pubNewSetpoint.publish(_new_point);
}

void VisualServo::publishStatus() {
    _boolMsg.data = isVisualServoEnabled();
    _pubIsEnabledTopic.publish(_boolMsg);
}

bool VisualServo::isVisualServoEnabled() {
  return _visualServoEnabled;
}

void runDefault(VisualServo& visualServoRefObj, ros::NodeHandle& nh) {
  double rate = 50;
  visualServoRefObj.setRate(rate);
  ros::Rate loopRate(rate);

  while (ros::ok()) {
    ros::spinOnce();
    if (visualServoRefObj.isVisualServoEnabled()) {
      visualServoRefObj.updateSetpoint();
      visualServoRefObj.publishNewSetpoint();
    }
    visualServoRefObj.publishStatus();
    loopRate.sleep();
  }
}
} // namespace uav_reference