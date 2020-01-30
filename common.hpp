#pragma once

#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

constexpr auto DISPLAY_W = 1920;
constexpr auto DISPLAY_H = 1080;


namespace ins
{


Mat get_transform_matrix(vector<Point2f> desired_points);


class LUT
{
protected:
    unique_ptr<uint*[]> lookup_table;
    int table_size;
    LUT(Mat transform_matrix, int width, int height, uint* datastart);
public:
    virtual ~LUT() {}
    virtual void apply(const uint* image_data) = 0;
};


class PlainLUT : public LUT
{
public:
    PlainLUT(Mat transform_matrix, int width, int height, uint* datastart);
    void apply(const uint* image_data) override;
};


class ParallelLUT : public LUT
{
private:
    int n_threads;
public:
    ParallelLUT(Mat transform_matrix, int width, int height, uint* datastart);
    void apply(const uint* image_data) override;
};


#ifdef __arm__
class LoadStoreMultipleLUT : public LUT
{
public:
    LoadStoreMultipleLUT(Mat transform_matrix, int width, int height, uint* datastart);
    void apply(const uint* image_data) override;
};


class ParallelLoadStoreMultipleLUT : public LUT
{
private:
    int n_threads;
public:
    ParallelLoadStoreMultipleLUT(Mat transform_matrix, int width, int height, uint* datastart);
    void apply(const uint* image_data) override;
};
#endif


}