#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "common.hpp"

using namespace std;
using namespace cv;

void print_usage(char* app_name)
{
    printf(
        "\n"
        "Usage:\t %s [method name] [image path]\n"
        "\n"
        "Example:\n"
        "\n"
        "\t$ %s parallel test.jpg\n"
        "\n"
        "Currently available methods:\n"
        "  plain\t\tplain 1D LUT with for-loop;\n"
        "  parallel\tmulti-threaded for-loop; each thread applies LUT on their sub-region\n"
#ifdef __arm__
        "  plain-o1\tplain 1D LUT with general purpose registers and LDM STM instructions\n"
        "  parallel-o1\tmulti-threaded optimized for-loop; same optimization scheme as plain-o1\n"
#endif
        "\n", app_name, app_name);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Method name and test image path must be specified!\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    Mat image = imread(argv[2]);
    if (image.empty())
    {
        printf("Failed to load the image!\n");
        return EXIT_FAILURE;
    }
    cvtColor(image, image, COLOR_BGR2BGRA);
    
    Mat screen = Mat::zeros(DISPLAY_H, DISPLAY_W, CV_8UC4);
    uint* screen_buffer = reinterpret_cast<uint*>(screen.data);

    vector<Point2f> points = { Point2f(242, 172), Point2f(1655, 71), Point2f(1714, 955), Point2f(255, 921) };
    Mat trans_mat = ins::get_transform_matrix(points);
    unique_ptr<ins::LUT> lut = nullptr;
    string lut_method = argv[1];
    string generated_class_info = "LUT method : ";
    if (lut_method.compare("plain") == 0)
    {
        generated_class_info += "PlainLUT";
        lut = make_unique<ins::PlainLUT>(trans_mat, DISPLAY_W, DISPLAY_H, screen_buffer);
    }
    else if (lut_method.compare("parallel") == 0)
    {
        generated_class_info += "ParallelLUT";
        lut = make_unique<ins::ParallelLUT>(trans_mat, DISPLAY_W, DISPLAY_H, screen_buffer);
    }
#ifdef __arm__
    else if (lut_method.compare("plain-o1") == 0)
    {
        generated_class_info += "LoadStoreMultipleLUT";
        lut = make_unique<ins::LoadStoreMultipleLUT>(trans_mat, DISPLAY_W, DISPLAY_H, screen_buffer);
    }
    else if (lut_method.compare("parallel-o1") == 0)
    {
        generated_class_info += "ParallelLoadStoreMultipleLUT";
        lut = make_unique<ins::ParallelLoadStoreMultipleLUT>(trans_mat, DISPLAY_W, DISPLAY_H, screen_buffer);
    }
#endif
    else
    {
        printf("Unrecognizable method name!\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    printf("%s\n", generated_class_info.c_str());

    for (int i = 0; i < 100; i++)
    {
        auto start = chrono::high_resolution_clock::now();

        lut->apply(reinterpret_cast<uint*>(image.data));

        auto end = chrono::high_resolution_clock::now();
        auto duration_ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        auto duration_us = chrono::duration_cast<chrono::microseconds>(end - start).count();
        printf("Operations took %lld ms, %lld us\n", duration_ms, duration_us);

#ifndef __arm__
        imshow("screen", screen);
        if ((waitKey(1) & 0xFF) == 27) break;
#endif
    }

    return EXIT_SUCCESS;
}
