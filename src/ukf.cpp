#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 2.8;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1.1;

  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  /**
   * End DO NOT MODIFY section for measurement noise values
   */

  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */

  // initially set to false, set to true in first call of ProcessMeasurement
  is_initialized_ = false;

  // set state dimension
  n_x_ = x_.size();

  // set augmented dimension
  n_aug_ = n_x_ + 2;

  // Number of Sigma Points
  n_sig_ = 2 * n_aug_ + 1;

  // define spreading parameter
  lambda_ = 3 - n_aug_;

  // Weights of sigma points
  weights_ = VectorXd(n_sig_);

  // predicted sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_, n_sig_);

}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * Switch between lidar and radar measurements.
   */

  if(!is_initialized_) {
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      double rho = meas_package.raw_measurements_(0);
      double phi = meas_package.raw_measurements_(1);
      double rhodot = meas_package.raw_measurements_(2);
      double vx = rhodot * cos(phi);
      double vy = rhodot * sin(phi);
      // initialize state.
      x_ << rho * cos(phi), rho * sin(phi), sqrt(vx * vx + vy * vy), 0, 0;

      // state covariance matrix
      P_ << std_radr_*std_radr_, 0, 0, 0, 0,
            0, std_radr_*std_radr_, 0, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 0, std_radphi_*std_radphi_, 0,
            0, 0, 0, 0, std_radphi_*std_radphi_;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      // initialize state.
      double lidar_px = meas_package.raw_measurements_(0);
      double lidar_py = meas_package.raw_measurements_(1);
      x_ << lidar_px, lidar_py, 0, 0, 0;

      // state covariance matrix
      P_ << std_laspx_*std_laspx_, 0, 0, 0, 0,
            0, std_laspy_*std_laspy_, 0, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 0, 1, 0,
            0, 0, 0, 0, 1;
    }

    time_us_ = meas_package.timestamp_;

    is_initialized_ = true;
  }

  // Time difference between measurements
  double dt = (meas_package.timestamp_ - time_us_) / 1000000.0;
  time_us_ = meas_package.timestamp_;

  // Run prediction
  Prediction(dt);

  // Update Measurements
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
  {
    UpdateRadar(meas_package);
  }
  else if (meas_package.sensor_type_ == MeasurementPackage::LASER)
  {
    UpdateLidar(meas_package);
  }
}

void UKF::Prediction(double delta_t) {
  /**
   * Estimate the object's location. Modify the state vector, x_.
   * Predict sigma points, the state, and the state covariance matrix.
   */

  // create augmented mean vector
  VectorXd x_aug    = VectorXd(n_aug_);

  // create augmented state covariance
  MatrixXd P_aug    = MatrixXd(n_aug_, n_aug_);

  // create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, n_sig_);

  // create augmented mean state
  x_aug.fill(0.0);
  x_aug.head(n_x_) = x_;
  x_aug.tail(2) << 0, 0;
  // create augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug.bottomRightCorner(2, 2) << std_a_*std_a_, 0,
                                   0, std_yawdd_*std_yawdd_;

  // create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

  // create augmented sigma points
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i < n_aug_; i++)
  {
    Xsig_aug.col(i + 1)          = x_aug + sqrt(lambda_+ n_aug_) * L.col(i);
    Xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_+ n_aug_) * L.col(i);
  }

  // predict sigma points
  for (int i = 0; i < n_sig_; i++)
  {
    // extract values for better readability
    double p_x      = Xsig_aug(0,i);
    double p_y      = Xsig_aug(1,i);
    double v        = Xsig_aug(2,i);
    double yaw      = Xsig_aug(3,i);
    double yawd     = Xsig_aug(4,i);
    double nu_a     = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    // predicted state values
    double px_p, py_p;

    // avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v / yawd * ( sin(yaw + yawd * delta_t) - sin(yaw));
        py_p = p_y + v / yawd * ( cos(yaw) - cos(yaw + yawd * delta_t) );
    }
    else {
        px_p = p_x + v * delta_t * cos(yaw);
        py_p = p_y + v * delta_t * sin(yaw);
    }

    double v_p    = v;
    double yaw_p  = yaw + yawd * delta_t;
    double yawd_p = yawd;

    // add noise
    px_p   = px_p + 0.5  * nu_a * delta_t * delta_t * cos(yaw);
    py_p   = py_p + 0.5  * nu_a * delta_t * delta_t * sin(yaw);
    v_p    = v_p  + nu_a * delta_t;

    yaw_p  = yaw_p  + 0.5 * nu_yawdd * delta_t * delta_t;
    yawd_p = yawd_p + nu_yawdd * delta_t;

    // write predicted sigma points
    Xsig_pred_.col(i) << px_p, py_p, v_p, yaw_p, yawd_p;
  }

  // set weights
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights_(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; ++i) {  // 2n+1 weights
    double weight = 0.5/(n_aug_+lambda_);
    weights_(i) = weight;
  }

  // predicted state mean
  x_.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {  //iterate over sigma points
    x_ += weights_(i) * Xsig_pred_.col(i);
  }

  // predicted state covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {  //iterate over sigma points
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P_ += weights_(i) * x_diff * x_diff.transpose() ;
  }
}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * Use lidar data to update the belief about the object's position.
   * Modify the state vector, x_, and covariance, P_.
   */

  // set measurement dimension, lidar can measure px and py
  int n_z = 2;

  // create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, n_sig_);

  // mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);

  Zsig.fill(0.0);
  z_pred.fill(0.0);

  for (int i = 0; i < n_sig_; i++) {
    // extract values for better readibility
    double px = Xsig_pred_(0,i);
    double py = Xsig_pred_(1,i);

    Zsig.col(i) << px, py;

    z_pred += weights_(i) * Zsig.col(i);
  }

  // measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {

    VectorXd z_diff = Zsig.col(i) - z_pred;
    S += weights_(i) * z_diff * z_diff.transpose();
  }

  // add measurement noise covariance matrix
  MatrixXd R = MatrixXd(2,2);
  R << std_laspx_*std_laspx_,                     0,
                           0, std_laspy_*std_laspy_;
  S += R;

  // create vector for incoming lidar measurement
  VectorXd z = VectorXd(n_z);
  z << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1);

  // create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    VectorXd z_diff = Zsig.col(i) - z_pred;

    Tc += weights_(i) * x_diff * z_diff.transpose();
  }

  // calculate Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  // residual
  VectorXd z_diff = z - z_pred;

  // calculate NIS
  nis_laser_.push_back(z_diff.transpose() * S.inverse() * z_diff);

  // update state mean and covariance matrix
  x_ += K * z_diff;
  P_ -= K * S * K.transpose();
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * Use radar data to update the belief about the object's
   * position. Modify the state vector, x_, and covariance, P_.
   */

  // set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 3;

  // create example matrix with predicted sigma points in state space
  MatrixXd Zsig = MatrixXd(n_z, n_sig_);

  // create vector for mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);

  // transform sigma points into measurement space
  for (int i = 0; i < n_sig_; i++) {
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v   = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v_x = cos(yaw) * v;
    double v_y = sin(yaw) * v;

    // measurement model
    Zsig.col(i) << sqrt(p_x*p_x + p_y*p_y), //r
                   atan2(p_y,p_x), //phi
                   (p_x*v_x + p_y*v_y ) / sqrt(p_x*p_x + p_y*p_y); //r_dot

    z_pred += weights_(i) * Zsig.col(i);
  }


  // measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  // add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);
  R <<    std_radr_*std_radr_, 0, 0,
          0, std_radphi_*std_radphi_, 0,
          0, 0,std_radrd_*std_radrd_;
  S = S + R;

  // create  vector for incoming radar measurement
  VectorXd z = VectorXd(n_z);
  z << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1), meas_package.raw_measurements_(2);

  // create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  Tc.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  // Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  // residual
  VectorXd z_diff = z - z_pred;

  // angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  // calculate NIS
  nis_radar_.push_back(z_diff.transpose() * S.inverse() * z_diff);

  // update state mean and covariance matrix
  x_ += K * z_diff;
  P_ -= K * S * K.transpose();
}

void UKF::printNIS() {
  // Print in CSV format for easy plotting
  std::cout << "\n\nNum,Radar,Lidar" << std::endl;
  for (int i = 0; i<nis_radar_.size(); i++)
  {
    std::cout << i << "," << nis_radar_[i]  << "," << nis_laser_[i] << std::endl;
  }
}
