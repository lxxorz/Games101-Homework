#include "Triangle.hpp"
#include "rasterizer.hpp"
#include <eigen3/Eigen/Eigen>
#include <iostream>
#include <opencv2/opencv.hpp>

constexpr double MY_PI = 3.1415926;

Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos)
{
    Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f translate;
    translate << 1, 0, 0, -eye_pos[0],
        0, 1, 0, -eye_pos[1],
        0, 0, 1, -eye_pos[2],
        0, 0, 0, 1;

    view = translate * view;

    return view;
}

/**
 * @brief 角度转弧度
 */
float to_radian(float degree)
{
    return degree * MY_PI / 180.0f;
}

Eigen::Matrix4f get_model_matrix(float angle)
{

    Eigen::Matrix4f model = Eigen::Matrix4f::Identity();

    // TODO: Implement this function
    // Create the model matrix for rotating the triangle around the Z axis.
    // Then return it.
    /**
     * https://excalidraw.com/#json=rHuSwx7q2NQGeUN0bQ4zN,FHz70z-axN1yAuIBi6AFUA
     */
    auto a = to_radian(angle);
    // 绕z轴的旋转矩阵
    model << cos(a), -sin(a), 0, 0,
        sin(a), cos(a), 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1;

    return model;
}

Eigen::Matrix4f get_orth_projection_matrix(const float left, const float right, const float bottom, const float top, const float near, const float far)
{
    Eigen::Matrix4f projection = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f translate = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f scale = Eigen::Matrix4f::Identity();
    // 将 frstum 移动到原点
    translate << 1, 0, 0, -(left + right) / 2,
        0, 1, 0, -(bottom + top) / 2,
        0, 0, 1, -(near + far) / 2,
        0, 0, 0, 1;

    // 归一化
    scale << 2 / (right - left), 0, 0, 0,
        0, 2 / (top - bottom), 0, 0,
        0, 0, 2 / (near - far), 0,
        0, 0, 0, 1;

    return scale * translate;
}

Eigen::Matrix4f get_projection_matrix(const float fov, const float aspect_ratio,
                                      const float near, const float far)
{
    // Students will implement this function
    // TODO: Implement this function
    // Create the projection matrix for the given parameters.
    // Then return it.

    // https://excalidraw.com/#json=GUbUTWFTJqo86gNORpPiZ,MRBqp0vwGOSNwSSXGk7_DA

    // 透视投影向正交投影转换
    const float n = -near;
    const float f = -far;

    Eigen::Matrix4f pers_to_orth = Eigen::Matrix4f::Identity();
    pers_to_orth << n, 0, 0, 0,
        0, n, 0, 0,
        0, 0, n + f, -n * f,
        0, 0, 1, 0;

    const float a = to_radian(fov);
    const float top = abs(n) * tan(a / 2.0f);
    const float bottom = -top;
    const float left = -top * aspect_ratio;
    const float right = -left;

    std::cout << left << "," << right << "," << bottom << "," << top << "," << n << "," << f << std::endl;
    const auto orth = get_orth_projection_matrix(left, right, bottom, top, n, f);
    return orth * pers_to_orth;
}

int main(int argc, const char **argv)
{
    float angle = 0;
    bool command_line = false;
    std::string filename = "output.png";

    if (argc >= 3)
    {
        command_line = true;
        angle = std::stof(argv[2]); // -r by default
        if (argc == 4)
        {
            filename = std::string(argv[3]);
        }
    }

    rst::rasterizer r(700, 700);

    Eigen::Vector3f eye_pos = {0, 0, 5};

    std::vector<Eigen::Vector3f> pos{{2, 0, -2}, {0, 2, -2}, {-2, 0, -2}};

    std::vector<Eigen::Vector3i> ind{{0, 1, 2}};

    auto pos_id = r.load_positions(pos);
    auto ind_id = r.load_indices(ind);

    int key = 0;
    int frame_count = 0;

    if (command_line)
    {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        // 朝向z轴负方向
        r.set_projection(get_projection_matrix(45, 1, 0.1, 50));

        r.draw(pos_id, ind_id, rst::Primitive::Triangle);
        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);

        cv::imwrite(filename, image);

        return 0;
    }

    while (key != 27)
    {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45, 1, 0.1, 50));

        r.draw(pos_id, ind_id, rst::Primitive::Triangle);

        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);
        cv::imshow("image", image);
        key = cv::waitKey(10);

        // std::cout << "frame count: " << frame_count++ << '\n';

        if (key == 'a')
        {
            angle += 10;
        }
        else if (key == 'd')
        {
            angle -= 10;
        }
        else if (key == 's')
        {
            eye_pos[2] -= 1;
        }
        else if (key == 'w')
        {
            eye_pos[2] += 1;
        }
    }

    return 0;
}
