#include "Odometry.h"
#include "Utils/IO.h"
#include "Visualization.h"
using namespace rabbit;
using namespace util;
int main(int argc, char **argv)
{
    if(argc == 3)
    {
        std::vector<std::string> seq; 
        GetPCDSequence(argv[1], seq);
        std::cout<<"Find "<< seq.size()<<" point clouds."<<std::endl;        
        visualization::Visualizer visualizer;        
        Frame last_frame;
        PointCloud pcd;
        PointCloud global_pcd;
        PointCloudRGB pcd_rgb, global_pcd_rgb;
        LidarOdometry lo;
        SE3 global_T;
        SE3 T;
        std::string method = argv[2];
        for(size_t i = 0; i != seq.size(); ++i)
        {
            LoadFile(seq[i], pcd);
            Frame current_frame;
            current_frame.SetPCD(pcd);
            // std::cout<<Frame::lidar_ring_n<<std::endl;
            if(method == "loam" || method == "loammapping") current_frame.ComputeLOAMFeature();
            if(method == "icpn") current_frame.ComputeNormal();
            if(i > 0)
            {
                if( method == "icp")
                lo.ICP(last_frame, current_frame, T);
                else if(method == "gicp")
                lo.GICP(last_frame, current_frame, T);
                else if(method == "ndt")
                lo.NDT(last_frame, current_frame, T);
                else if(method == "gicpomp")
                lo.GICPOMP(last_frame, current_frame, T);
                else if(method == "ndtomp")
                lo.NDTOMP(last_frame, current_frame, T);
                else if(method == "loam")
                lo.Loam(last_frame, current_frame, T);
                else if(method == "loammapping")
                lo.LoamMapping(last_frame, current_frame, T);
                else if(method == "icpn")
                lo.ICPN(last_frame, current_frame, T);
                global_T = T * global_T;
                current_frame.pose = global_T;
            }
            pcl::transformPointCloud (pcd, pcd, global_T.inverse().matrix());
            if(i % 5 == 0)
            {
                FilterPCD(global_pcd, global_pcd, 0.5, 0.5, 0.5);
                ColorizePointCloud(pcd, Vec3f(0, 1, 0) ,pcd_rgb);
                ColorizePointCloud(global_pcd, global_pcd_rgb);
                global_pcd_rgb += pcd_rgb;
                visualizer.SetPCD(global_pcd_rgb);
                visualizer.ShowOnce();
                global_pcd += pcd;
            }
            std::cout<<"frame id: "<<i<<global_T.log().transpose()<<std::endl;
            last_frame = current_frame;
        }
        WritePLY("./odometry_test_"+method+".ply", global_pcd_rgb);
    }
    else if(argc == 4)
    {
        PointCloud source, target;
        Frame::ground_extraction = true;
        Frame sf, tf;
        LoadFile(argv[1], source);
        LoadFile(argv[2], target);
        sf.SetPCD(source);
        tf.SetPCD(target);   
        double inlier_ratio = 0.1;
        LidarOdometry lo;
        //         Mat4 mat_T;
        //         mat_T <<     0.954353,  -0.298511, -0.0100912,    1.07876,
        //   0.298381,   0.954364,  -0.012597,   -4.72456,
        //   0.013391,  0.0090109,    0.99987, -0.0864058,
        //          0,          0,          0,          1;

        // std::cout<<mat_T<<std::endl;
        SE3 T;
        sf.ComputeLOAMFeature();
        tf.ComputeLOAMFeature();
        std::string method = argv[3];
        if( method == "icp")
        lo.ICP(sf, tf, T, inlier_ratio);
        else if(method == "gicp")
        lo.GICP(sf, tf, T, inlier_ratio);
        else if(method == "ndt")
        lo.NDT(sf, tf, T, inlier_ratio);
        else if(method == "gicpomp")
        lo.GICPOMP(sf, tf, T, inlier_ratio);
        else if(method == "ndtomp")
        lo.NDTOMP(sf, tf, T, inlier_ratio);
        else if(method == "icpn")
        {
            sf.ComputeNormal();
            tf.ComputeNormal();
            lo.ICPN(sf, tf, T, inlier_ratio);
        }
        else if(method == "loam")
        {   
        
            lo.Loam(sf, tf, T, inlier_ratio);
        }
        else if(method == "loammapping")
        {
            sf.ComputeLOAMFeature();
            tf.ComputeLOAMFeature();
            lo.LoamMapping(sf, tf, T, inlier_ratio);
        }
        else
        {
            std::cout<<"unsupported method"<<std::endl;
            return -1;
        }
        std::cout<<T.matrix()<<std::endl;
        visualization::Visualizer visualizer;
        PointCloudRGB s_rgb, t_rgb, origin_s_rgb, final_rgb;
        ColorizePointCloud(source, Vec3f(1, 0, 0), s_rgb);
        ColorizePointCloud(source, Vec3f(0, 0, 1), origin_s_rgb);
        ColorizePointCloud(target, Vec3f(0, 1, 0), t_rgb);
        pcl::transformPointCloud (s_rgb, s_rgb, T.matrix());
        final_rgb += s_rgb;
        final_rgb += t_rgb;
        // final_rgb += origin_s_rgb;
        visualizer.SetPCD(final_rgb);
        visualizer.Show();
        return 0;
    }
    else
    {
        std::cout<<"usage: OdometryTest [source_pcd] [target_pcd] [method] or \n\tOdometryTest [pcd_folder]  [method] "<<std::endl;
        return 0;
    }
}