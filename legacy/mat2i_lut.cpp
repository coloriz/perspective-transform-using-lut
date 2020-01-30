#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

enum State { SELECT, PREPARE, RUN };
constexpr auto DISPLAY_WIDTH = 1920;
constexpr auto DISPLAY_HEIGHT = 1080;
constexpr auto WINDOW_NAME = "Original";
State state = SELECT;
std::vector<cv::Point2f> points;

void on_mouse_event(int event, int x, int y, int flags, void* userdata)
{
    if (event == cv::EVENT_LBUTTONDOWN)
    {
        points.push_back(cv::Point2f((float)x, (float)y));
        cv::Mat canvas = *(cv::Mat*)userdata;
        cv::circle(canvas, cv::Point(x, y), 3, cv::Scalar::all(255), cv::FILLED, cv::LINE_AA);
        if (points.size() == 4)
        {
            cout << points << endl;
            state = PREPARE;
        }
    }
}

cv::Mat get_transform_matrix(std::vector<cv::Point2f> desired_points)
{
    std::vector<cv::Point2f> image_rect;
    image_rect.push_back(cv::Point2f(0, 0));
    image_rect.push_back(cv::Point2f(DISPLAY_WIDTH, 0));
    image_rect.push_back(cv::Point2f(DISPLAY_WIDTH, DISPLAY_HEIGHT));
    image_rect.push_back(cv::Point2f(0, DISPLAY_HEIGHT));

    return cv::getPerspectiveTransform(image_rect, desired_points);
}

cv::Mat2i generate_lookup_table(cv::Mat transform_matrix)
{
    std::vector<cv::Point2f> coordinates_map;
    for (int y = 0; y < DISPLAY_HEIGHT; y++)
    {
        for (int x = 0; x < DISPLAY_WIDTH; x++)
        {
            coordinates_map.push_back(cv::Point2f((float)x, (float)y));
        }
    }

    cv::perspectiveTransform(coordinates_map, coordinates_map, transform_matrix);
    cv::Mat2i lookup_table(DISPLAY_HEIGHT, DISPLAY_WIDTH);

    for (int i = 0; i < DISPLAY_HEIGHT; i++)
    {
        for (int j = 0; j < DISPLAY_WIDTH; j++)
        {
            int map_index = i * DISPLAY_WIDTH + j;
            lookup_table(i, j) = cv::Vec2i((int)std::roundf(coordinates_map[map_index].y), (int)std::roundf(coordinates_map[map_index].x));
        }
    }

    return lookup_table;
}

int main(int argc, char** argv)
{
    Mat canvas = Mat::zeros(Size(DISPLAY_WIDTH, DISPLAY_HEIGHT), CV_8UC4);
    Mat test_img = imread("ZtcjR.jpg");
    cvtColor(test_img, test_img, COLOR_BGR2BGRA);

    namedWindow(WINDOW_NAME);
    setMouseCallback(WINDOW_NAME, on_mouse_event, &canvas);

    Mat2i lut;

    while (true)
    {
        switch (state)
        {
        case SELECT:
            imshow(WINDOW_NAME, canvas);
            break;
        case PREPARE:
        {
            imshow(WINDOW_NAME, canvas);
            Mat matrix = get_transform_matrix(points);
            lut = generate_lookup_table(matrix);
            state = RUN;
            break;
        }
        case RUN:
        {
            Mat lut_ver = Mat::zeros(DISPLAY_HEIGHT, DISPLAY_WIDTH, CV_8UC4);
            auto start = chrono::high_resolution_clock::now();

            for (int i = 0; i < DISPLAY_HEIGHT; i++)
            {
                for (int j = 0; j < DISPLAY_WIDTH; j++)
                {
                    lut_ver.at<Vec4b>(lut(i, j)) = test_img.at<Vec4b>(i, j);
                }
            }
            auto end = chrono::high_resolution_clock::now();
            auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
            cout << "Mat2i LUT took " << duration << " ms" << endl;

            imshow("Using LUT", lut_ver);
            break;
        }
        default:
            break;
        }

        if ((waitKey(1) & 0xFF) == 27) break;
    }

    return 0;
}
