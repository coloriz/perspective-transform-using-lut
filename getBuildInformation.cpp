#include <iostream>
#include <opencv2/core/utility.hpp>

using namespace std;
using namespace cv;

int main(int argc, char* argv[])
{
    cout << getBuildInformation() << endl;
    return 0;
}
