// clang-format off
//
// Created by goksu on 4/6/19.
//

#include <algorithm>
#include <vector>
#include "rasterizer.hpp"
#include <opencv2/opencv.hpp>
#include <math.h>

const bool is_enable_msaa = true;
const unsigned int nums = 2; // sample range 2x2
constexpr unsigned int sample_nums = nums * nums;

rst::pos_buf_id rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions)
{
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices)
{
    auto id = get_next_id();
    ind_buf.emplace(id, indices);

    return {id};
}

rst::col_buf_id rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols)
{
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
{
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}

/**
 * 向量a,b是否在向量c的同侧
*/
bool is_same_side(Vector3f c, Vector3f a, Vector3f b) {
    return c.cross(a).dot(c.cross(b)) >= 0;
}

// 问题2 参数类型是 int cpp会隐式转换类型，导致了 msaa 没有生效
static bool insideTriangle(const float x, const float y, const Vector3f* _v)
{
    // TODO : Implement this function to check if the point (x, y) is inside the triangle represented by _v[0], _v[1], _v[2]
    const auto a = _v[0];
    const auto b = _v[1];
    const auto c = _v[2];
    const auto p = Eigen::Vector3f(x, y, 1);

    return is_same_side(b - a, p - a, c - a) && is_same_side(c - b, p - b, a - b) && is_same_side(a - c, p - c, b - c);
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v)
{
    float c1 = (x*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*y + v[1].x()*v[2].y() - v[2].x()*v[1].y()) / (v[0].x()*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*v[0].y() + v[1].x()*v[2].y() - v[2].x()*v[1].y());
    float c2 = (x*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*y + v[2].x()*v[0].y() - v[0].x()*v[2].y()) / (v[1].x()*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*v[1].y() + v[2].x()*v[0].y() - v[0].x()*v[2].y());
    float c3 = (x*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*y + v[0].x()*v[1].y() - v[1].x()*v[0].y()) / (v[2].x()*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*v[2].y() + v[0].x()*v[1].y() - v[1].x()*v[0].y());
    return {c1,c2,c3};
}

void rst::rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer, Primitive type)
{
    auto& buf = pos_buf[pos_buffer.pos_id];
    auto& ind = ind_buf[ind_buffer.ind_id];
    auto& col = col_buf[col_buffer.col_id];

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (auto& i : ind)
    {
        Triangle t;
        Eigen::Vector4f v[] = {
                mvp * to_vec4(buf[i[0]], 1.0f),
                mvp * to_vec4(buf[i[1]], 1.0f),
                mvp * to_vec4(buf[i[2]], 1.0f)
        };
        //Homogeneous division
        for (auto& vec : v) {
            vec /= vec.w();
        }
        //Viewport transformation
        for (auto & vert : v)
        {
            vert.x() = 0.5*width*(vert.x()+1.0);
            vert.y() = 0.5*height*(vert.y()+1.0);
            vert.z() = vert.z() * f1 + f2;
        }

        for (int i = 0; i < 3; ++i)
        {
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
        }

        auto col_x = col[i[0]];
        auto col_y = col[i[1]];
        auto col_z = col[i[2]];

        t.setColor(0, col_x[0], col_x[1], col_x[2]);
        t.setColor(1, col_y[0], col_y[1], col_y[2]);
        t.setColor(2, col_z[0], col_z[1], col_z[2]);

        rasterize_triangle(t);
    }
}

//Screen space rasterization
void rst::rasterizer::rasterize_triangle(const Triangle& t) {
    auto v = t.toVector4();

    // TODO : Find out the bounding box of current triangle.
    // 计算三角形包围盒， 此时坐标已经是屏幕坐标
    // 问题1. 没有对屏幕坐标取整
    const auto x_min = std::floor(std::min(v[0].x(), std::min(v[1].x(), v[2].x())));
    const auto y_min = std::floor(std::min(v[0].y(), std::min(v[1].y(), v[2].y())));
    const auto x_max = std::ceil(std::max(v[0].x(), std::max(v[1].x(), v[2].x())));
    const auto y_max = std::ceil(std::max(v[0].y(), std::max(v[1].y(), v[2].y())));


    // iterate through the pixel and find if the current pixel is inside the triangle
    for(unsigned int x = x_min; x <= x_max; ++x) {
        for(unsigned int y = y_min; y <= y_max; ++y) {
                if(is_enable_msaa) {
                    auto color = Eigen::Vector3f(0.0f, 0.0f, 0.0f);
                    auto z_interpolated = 0.0f;
                    auto is_set_pixel = 0;
                    for(auto offset_x = 0; offset_x < nums; ++offset_x) {
                        for(auto offset_y = 0; offset_y < nums; ++offset_y) {
                            const auto sample_offset_x = (offset_x + 0.5f) / nums;
                            const auto sample_offset_y = (offset_y + 0.5f) / nums;
                            const auto sample_x = x + sample_offset_x;
                            const auto sample_y = y + sample_offset_y;

                            // FIXME 子采样错误
                            if(insideTriangle(sample_x, sample_y, t.v)) {
                                auto sample_index = get_subsample_index(x, y, offset_x, offset_y);
                                color += subsample_frame_buf[sample_index];

                                auto[alpha, beta, gamma] = computeBarycentric2D(sample_x, sample_y, t.v);
                                float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
                                float sample_z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
                                sample_z_interpolated *= w_reciprocal;
                                z_interpolated += sample_z_interpolated;


                                if(sample_z_interpolated < subsample_depth_buf[sample_index]) {
                                    is_set_pixel += 1;
                                    subsample_depth_buf[sample_index] = sample_z_interpolated;
                                    // 上一帧此采样点颜色 + 此处三角形颜色
                                    color += subsample_frame_buf[sample_index];
                                    color += t.getColor();

                                    subsample_frame_buf[sample_index] = t.getColor();
                                }
                            }
                        }
                    }
                    // TODO : set the current pixel (use the set_pixel function) to the color of the triangle (use getColor function) if it should be painted.
                    if(is_set_pixel) {
                        set_pixel(Vector3f(x, y, z_interpolated), color / (sample_nums));
                    }
                } else {
                    if(insideTriangle(x + 0.5, y + 0.5, t.v)) {
                        // If so, use the following code to get the interpolated z value.
                        auto[alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
                        float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
                        float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
                        z_interpolated *= w_reciprocal;

                        if(z_interpolated < depth_buf[get_index(x, y)]) {
                            depth_buf[get_index(x, y)] = z_interpolated;
                            // TODO : set the current pixel (use the set_pixel function) to the color of the triangle (use getColor function) if it should be painted.
                            set_pixel(Vector3f(x, y, z_interpolated), t.getColor());
                        }
                    }
                }
        }
    }
}

void rst::rasterizer::set_model(const Eigen::Matrix4f& m)
{
    model = m;
}

void rst::rasterizer::set_view(const Eigen::Matrix4f& v)
{
    view = v;
}

void rst::rasterizer::set_projection(const Eigen::Matrix4f& p)
{
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff)
{
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
    }

    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
    }

    if((buff & rst::Buffers::AntiAliasing) == rst::Buffers::AntiAliasing && (buff & rst::Buffers::Depth) == rst::Buffers::Depth) {
        std::fill(subsample_depth_buf.begin(), subsample_depth_buf.end(), std::numeric_limits<float>::infinity());
        std::fill(subsample_frame_buf.begin(), subsample_frame_buf.end(), Eigen::Vector3f(0.0f, 0.0f, 0.0f));
    }
}

// 获取子采样点的索引
int rst::rasterizer::get_subsample_index(int x, int y, int offset_x, int offset_y) {
    return (height - 1 - y) * width * sample_nums + x * sample_nums + offset_y * nums + offset_x;
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h)
{
    frame_buf.resize(w * h);
    depth_buf.resize(w * h);

    // 子采样点
    subsample_frame_buf.resize(w * h * sample_nums);
    subsample_depth_buf.resize(w * h * sample_nums);
}

int rst::rasterizer::get_index(int x, int y)
{
    return (height-1-y)*width + x;
}

void rst::rasterizer::set_pixel(const Eigen::Vector3f& point, const Eigen::Vector3f& color)
{
    //old index: auto ind = point.y() + point.x() * width;
    auto ind = (height-1-point.y())*width + point.x();
    frame_buf[ind] = color;

}

// clang-format on
