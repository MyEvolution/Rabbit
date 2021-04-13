#ifndef RABBIT_RESIDUAL_H
#define RABBIT_RESIDUAL_H
/*
define several types of residual: point 2 point, point 2 line, point 2 plane
The code is based on A-LOAM.
Also, we will define pose 2 pose, based on sophus.
*/

#include <ceres/ceres.h>
#include <ceres/rotation.h>
#include <eigen3/Eigen/Dense>
#include <pcl/kdtree/kdtree_flann.h>
#include "Util.h"
#include <sophus/interpolate.hpp>
#include <ceres/local_parameterization.h>
namespace rabbit
{
    struct Point2LineFactor
    {
        Point2LineFactor(Vec3 curr_point_, Vec3 last_point_a_,
                        Vec3 last_point_b_, double s_)
            : curr_point(curr_point_), last_point_a(last_point_a_), last_point_b(last_point_b_), s(s_) {}
        // l2c is the transformation on SE3, transform current point cloud to last point cloud
        template <typename T>
        bool operator()(const T *c2l, T *residual) const
        {
            Eigen::Map<Sophus::SE3<T> const> const curr2last(c2l);
            Sophus::SE3<T> proj2last= Sophus::interpolate(Sophus::SE3<T>(), Sophus::SE3<T>(curr2last), T(s));
            // undistorted points
            Eigen::Matrix<T, 3, 1> lpa{T(last_point_a.x()), T(last_point_a.y()), T(last_point_a.z())};
            Eigen::Matrix<T, 3, 1> lpb{T(last_point_b.x()), T(last_point_b.y()), T(last_point_b.z())};
            Eigen::Matrix<T, 3, 1> cp{T(curr_point.x()), T(curr_point.y()), T(curr_point.z())};
            Eigen::Matrix<T, 3, 1> lp = proj2last * cp;
            
            //Eigen::Quaternion<T> q_last_curr{q[3], T(s) * q[0], T(s) * q[1], T(s) * q[2]};
            // Eigen::Quaternion<T> q_last_curr{q[3], q[0], q[1], q[2]};
            // Eigen::Quaternion<T> q_identity{T(1), T(0), T(0), T(0)};
            // project to current frame


            // q_last_curr = q_identity.slerp(T(s), q_last_curr);
            // Eigen::Matrix<T, 3, 1> t_last_curr{T(s) * t[0], T(s) * t[1], T(s) * t[2]};

            Eigen::Matrix<T, 3, 1> nu = (lp - lpa).cross(lp - lpb);
            Eigen::Matrix<T, 3, 1> de = lpa - lpb;

            residual[0] = nu.x() / de.norm();
            residual[1] = nu.y() / de.norm();
            residual[2] = nu.z() / de.norm();

            return true;
        }

        static ceres::CostFunction *Create(const Vec3 curr_point_, const Vec3 last_point_a_,
                                        const Vec3 last_point_b_, const double s_)
        {
            // first parameter is the dimension of residual, second parameter is the dimension of input, and there could be a third parameter (dimension of the second input).
            // see http://ceres-solver.org/nnls_modeling.html
            // the square error is implicit.
            return (new ceres::AutoDiffCostFunction<
                    Point2LineFactor, 3, SE3::num_parameters>(
                new Point2LineFactor(curr_point_, last_point_a_, last_point_b_, s_)));
        }

        Vec3 curr_point, last_point_a, last_point_b;
        double s;
    };

    struct Point2PlaneFactor
    {
        Point2PlaneFactor(Vec3 curr_point_, Vec3 last_point_j_,
                        Vec3 last_point_l_, Vec3 last_point_m_, double s_)
            : curr_point(curr_point_), last_point_j(last_point_j_), last_point_l(last_point_l_),
            last_point_m(last_point_m_),  s(s_)
        {
            ljm_norm = (last_point_j - last_point_l).cross(last_point_j - last_point_m);
            ljm_norm.normalize();
        }

        template <typename T>
        bool operator()(const T *c2l, T *residual) const
        {
            Eigen::Map<Sophus::SE3<T> const> const curr2last(c2l);
            Sophus::SE3<T> proj2last= Sophus::interpolate(Sophus::SE3<T>(), Sophus::SE3<T>(curr2last), T(s));

            Eigen::Matrix<T, 3, 1> cp{T(curr_point.x()), T(curr_point.y()), T(curr_point.z())};
            Eigen::Matrix<T, 3, 1> lpj{T(last_point_j.x()), T(last_point_j.y()), T(last_point_j.z())};
            Eigen::Matrix<T, 3, 1> ljm{T(ljm_norm.x()), T(ljm_norm.y()), T(ljm_norm.z())};
            Eigen::Matrix<T, 3, 1> lp = proj2last * cp;

            residual[0] = (lp - lpj).dot(ljm);

            return true;
        }

        static ceres::CostFunction *Create(const Vec3 curr_point_, const Vec3 last_point_j_,
                                        const Vec3 last_point_l_, const Vec3 last_point_m_,
                                        const double s_)
        {
            return (new ceres::AutoDiffCostFunction<
                    Point2PlaneFactor, 1, SE3::num_parameters>(
                new Point2PlaneFactor(curr_point_, last_point_j_, last_point_l_, last_point_m_, s_)));
        }

        Vec3 curr_point, last_point_j, last_point_l, last_point_m;
        Vec3 ljm_norm;
        double s;
    };

    struct Point2PlaneNormFactor
    {

        Point2PlaneNormFactor(Vec3 curr_point_, Vec3 plane_unit_norm_,
                            double negative_OA_dot_norm_) : curr_point(curr_point_), plane_unit_norm(plane_unit_norm_),
                                                            negative_OA_dot_norm(negative_OA_dot_norm_) {}

        template <typename T>
        bool operator()(const T *c2l, T *residual) const
        {
            Eigen::Map<Sophus::SE3<T> const> const curr2last(c2l);

            Eigen::Matrix<T, 3, 1> cp{T(curr_point.x()), T(curr_point.y()), T(curr_point.z())};
            Eigen::Matrix<T, 3, 1> point_w = curr2last * cp;
            Eigen::Matrix<T, 3, 1> norm(T(plane_unit_norm.x()), T(plane_unit_norm.y()), T(plane_unit_norm.z()));
            residual[0] = norm.dot(point_w) + T(negative_OA_dot_norm);
            return true;
        }

        static ceres::CostFunction *Create(const Vec3 curr_point_, const Vec3 plane_unit_norm_,
                                        const double negative_OA_dot_norm_)
        {
            return (new ceres::AutoDiffCostFunction<
                    Point2PlaneNormFactor, 1, SE3::num_parameters>(
                new Point2PlaneNormFactor(curr_point_, plane_unit_norm_, negative_OA_dot_norm_)));
        }

        Vec3 curr_point;
        Vec3 plane_unit_norm;
        double negative_OA_dot_norm;
    };


    struct Point2PointFactor
    {

        Point2PointFactor(Vec3 curr_point_, Vec3 closed_point_) 
                            : curr_point(curr_point_), closed_point(closed_point_){}

        template <typename T>
        bool operator()(const T *c2l, T *residual) const
        {
            Eigen::Map<Sophus::SE3<T> const> const curr2last(c2l);
            Eigen::Matrix<T, 3, 1> cp{T(curr_point.x()), T(curr_point.y()), T(curr_point.z())};
            Eigen::Matrix<T, 3, 1> point_w = curr2last * cp;
            residual[0] = point_w.x() - T(closed_point.x());
            residual[1] = point_w.y() - T(closed_point.y());
            residual[2] = point_w.z() - T(closed_point.z());
            return true;
        }

        static ceres::CostFunction *Create(const Vec3 curr_point_, const Vec3 closed_point_)
        {
            return (new ceres::AutoDiffCostFunction<
                    Point2PointFactor, 3, SE3::num_parameters>(
                new Point2PointFactor(curr_point_, closed_point_)));
        }

        Vec3 curr_point;
        Vec3 closed_point;
    };
    struct Pose2PoseFactor
    {

        Pose2PoseFactor( SE3 curr_pose_, SE3 last_pose_) 
                            : curr_pose(curr_pose_), last_pose(last_pose_){}

    template <class T>
    bool operator()(T const* const c2l, T* res) const {
        Eigen::Map<Sophus::SE3<T> const> const curr2last(c2l);
        Eigen::Map<Eigen::Matrix<T, 6, 1> > residuals(res);
        // We are able to mix Sophus types with doubles and Jet types without
        // needing to cast to T.
        residuals = (curr_pose.cast<T>() *  curr2last * last_pose.cast<T>().inverse() ).log();
        return true;
    }

        static ceres::CostFunction *Create(const SE3 curr_pose_, const SE3 last_pose_)
        {
            return (new ceres::AutoDiffCostFunction<
                    Pose2PoseFactor, SE3::DoF, SE3::num_parameters>(
                new Pose2PoseFactor(curr_pose_, last_pose_)));
        }
        SE3 curr_pose;
        SE3 last_pose;
    };

    class LocalParameterizationSE3 : public ceres::LocalParameterization 
    {
        public:
        virtual ~LocalParameterizationSE3() {}

        // SE3 plus operation for Ceres
        //
        //  T * exp(x)
        //
        virtual bool Plus(double const* T_raw, double const* delta_raw,
                            double* T_plus_delta_raw) const {
            Eigen::Map<SE3 const> const T(T_raw);
            Eigen::Map<Vec6 const> const delta(delta_raw);
            Eigen::Map<SE3> T_plus_delta(T_plus_delta_raw);
            T_plus_delta = T * SE3::exp(delta);
            return true;
        }

        // Jacobian of SE3 plus operation for Ceres
        //
        // Dx T * exp(x)  with  x=0
        //
        virtual bool ComputeJacobian(double const* T_raw,
                                    double* jacobian_raw) const {
            Eigen::Map<SE3 const> T(T_raw);
            Eigen::Map<Eigen::Matrix<double, 7, 6, Eigen::RowMajor>> jacobian(
                jacobian_raw);
            jacobian = T.Dx_this_mul_exp_x_at_0();
            return true;
        }

        virtual int GlobalSize() const { return SE3::num_parameters; }

        virtual int LocalSize() const { return SE3::DoF; }
    };
}
#endif