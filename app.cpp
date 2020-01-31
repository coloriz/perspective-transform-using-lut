#include <chrono>
#include <cstdio>
#include <memory>
#include <regex>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "common.hpp"

using namespace std;
using namespace cv;



bool parse_args(int argc, char** argv, 
                string& lut_method, string& image_path, 
                Point2f& tl, Point2f& tr, Point2f& br, Point2f& bl,
                Size& resolution,
                bool& no_gui,
                int& repeat);


int main(int argc, char** argv)
{
    string lut_method, image_path;
    Point2f tl, tr, br, bl;
    Size resolution;
    bool no_gui;
    int repeat;

    if (!parse_args(argc, argv, lut_method, image_path, tl, tr, br, bl, resolution, no_gui, repeat))
    {
        return EXIT_FAILURE;
    }

    Mat image = imread(image_path);
    if (image.empty())
    {
        printf("Failed to load the image!\n");
        return EXIT_FAILURE;
    }
    cvtColor(image, image, COLOR_BGR2BGRA);
    
    Mat screen = Mat::zeros(resolution.height, resolution.width, CV_8UC4);
    uint* screen_buffer = reinterpret_cast<uint*>(screen.data);

    vector<Point2f> points = { tl, tr, br, bl };
    Mat trans_mat = ins::get_transform_matrix(points);
    unique_ptr<ins::LUT> lut = nullptr;
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
        printf("Unrecognizable method name! : %s\n", lut_method.c_str());
        return EXIT_FAILURE;
    }

    printf("%s\n", generated_class_info.c_str());

    for (int i = 0; i < repeat; i++)
    {
        auto start = chrono::high_resolution_clock::now();

        lut->apply(reinterpret_cast<uint*>(image.data));

        auto end = chrono::high_resolution_clock::now();
        auto duration_ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        auto duration_us = chrono::duration_cast<chrono::microseconds>(end - start).count();
        printf("Operations took %lld ms, %lld us\n", duration_ms, duration_us);

        if (!no_gui)
        {
            imshow("screen", screen);
            if ((waitKey(1) & 0xFF) == 27) break;
        }
    }

    return EXIT_SUCCESS;
}


bool parse_args(int argc, char** argv, 
                string& lut_method, string& image_path, 
                Point2f& tl, Point2f& tr, Point2f& br, Point2f& bl,
                Size& resolution,
                bool& no_gui,
                int& repeat)
{
    const string keys =
        "{h help     |         | print this message and exit. }"
        "{@method    |<none>   | the method to be run. }"
        "{@image     |<none>   | image to be transformed. }"
        "{@TL        |<none>   | desired coordinates of top-left corner. format: x,y }"
        "{@TR        |<none>   | desired coordinates of top-right corner. format: x,y }"
        "{@BR        |<none>   | desired coordinates of bottom-right corner. format: x,y }"
        "{@BL        |<none>   | desired coordinates of bottom-left corner. format: x,y }"
        "{resolution |1920x1080| the size of screen. format: WxH }"
        "{no-gui     |         | }"
        "{repeat     |100      | the number of times to run the method. }";

    CommandLineParser parser(argc, argv, keys);
    parser.about(
        "Run a performance assessment of perspective transform using LUT.\n"
        "\n"
        "Following methods are currently available:\n"
        "        plain           plain 1D LUT with for-loop\n"
        "        parallel        multi-threaded for-loop; each thread applies LUT on their sub-region\n"
#ifdef __arm__
        "        plain-o1        plain 1D LUT with general purpose registers and LDM STM instructions\n"
        "        parallel-o1     multi-threaded optimized for-loop; same optimization scheme as plain-o1\n"
#endif
    );

    if (parser.has("help"))
    {
        parser.printMessage();
        return false;
    }

    lut_method = parser.get<string>("@method");
    image_path = parser.get<string>("@image");

    regex coordinates_pattern(R"~((\d+),(\d+))~");
    smatch matches;

    string tmps = parser.get<string>("@TL");
    if (regex_match(tmps, matches, coordinates_pattern))
        tl = Point2f(stoi(matches[1].str()), stoi(matches[2].str()));
    else
    {
        printf("Error: failed to parse @TL=%s\n", tmps.c_str());
        return false;
    }

    tmps = parser.get<string>("@TR");
    if (regex_match(tmps, matches, coordinates_pattern))
        tr = Point2f(stoi(matches[1].str()), stoi(matches[2].str()));
    else
    {
        printf("Error: failed to parse @TR=%s\n", tmps.c_str());
        return false;
    }

    tmps = parser.get<string>("@BR");
    if (regex_match(tmps, matches, coordinates_pattern))
        br = Point2f(stoi(matches[1].str()), stoi(matches[2].str()));
    else
    {
        printf("Error: failed to parse @BR=%s\n", tmps.c_str());
        return false;
    }

    tmps = parser.get<string>("@BL");
    if (regex_match(tmps, matches, coordinates_pattern))
        bl = Point2f(stoi(matches[1].str()), stoi(matches[2].str()));
    else
    {
        printf("Error: failed to parse @BL=%s\n", tmps.c_str());
        return false;
    }

    regex resolution_pattern(R"~((\d+)[x|X](\d+))~");
    tmps = parser.get<string>("resolution");
    if (regex_match(tmps, matches, resolution_pattern))
        resolution = Size(stoi(matches[1].str()), stoi(matches[2].str()));
    else
    {
        printf("Error: failed to parse [resolution]=%s\n", tmps.c_str());
        return false;
    }

    no_gui = parser.has("no-gui");

    repeat = parser.get<int>("repeat");

    if (!parser.check())
    {
        parser.printErrors();
        return false;
    }

    return true;
}