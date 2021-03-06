#include <iostream>
#include <chrono>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

constexpr auto DISPLAY_WIDTH = 1920;
constexpr auto DISPLAY_HEIGHT = 1080;
vector<Point2f> points = { Point2f(242, 172), Point2f(1655, 71), Point2f(1714, 955), Point2f(255, 921) };

Mat get_transform_matrix(std::vector<Point2f> desired_points)
{
    std::vector<Point2f> image_rect;
    image_rect.push_back(Point2f(0, 0));
    image_rect.push_back(Point2f(DISPLAY_WIDTH, 0));
    image_rect.push_back(Point2f(DISPLAY_WIDTH, DISPLAY_HEIGHT));
    image_rect.push_back(Point2f(0, DISPLAY_HEIGHT));

    return getPerspectiveTransform(image_rect, desired_points);
}

int* generate_lookup_table(Mat transform_matrix)
{
    vector<Point2f> coordinates_map;
    for (int y = 0; y < DISPLAY_HEIGHT; y++)
    {
        for (int x = 0; x < DISPLAY_WIDTH; x++)
        {
            coordinates_map.push_back(Point2f((float)x, (float)y));
        }
    }

    perspectiveTransform(coordinates_map, coordinates_map, transform_matrix);
    int* lookup_table = new int[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
    {
        Point2i point = Point2i((int)roundf(coordinates_map[i].x), (int)roundf(coordinates_map[i].y));
        lookup_table[i] = point.y * DISPLAY_WIDTH + point.x;
    }

    return lookup_table;
}

int main(int argc, char** argv)
{
    int stripe = getNumThreads();
    cout << "stripe = " << stripe << endl;
    Mat test_img = imread("ZtcjR.jpg");
    cvtColor(test_img, test_img, COLOR_BGR2BGRA);
    Mat trans_mat = get_transform_matrix(points);
    int* lut = generate_lookup_table(trans_mat);
    cout << "lookup table generated" << endl;

    Mat canvas = Mat::zeros(DISPLAY_HEIGHT, DISPLAY_WIDTH, CV_8UC4);
    uint* canvas_data = (uint*)canvas.data;
    uint* test_img_data = (uint*)test_img.data;

    // pre-compute canvas address
    uint** lut_precomp = new uint*[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
    {
        lut_precomp[i] = canvas_data + lut[i];
    }

    for (int i = 0; i < 100; i++)
    {
        auto start = chrono::high_resolution_clock::now();

        parallel_for_(Range(0, DISPLAY_WIDTH * DISPLAY_HEIGHT), [&](const Range& range){
            for (int r = range.start; r < range.end; r++)
            {
                *(lut_precomp[r]) = test_img_data[r];
            }
        }, stripe);

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        cout << "1d LUT took " << duration << " ms" << endl;

        imshow("1d LUT", canvas);
        if ((waitKey(1) & 0xFF) == 27) break;
    }

    delete[] lut;
    delete[] lut_precomp;

    return 0;
}