#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
using namespace cv;
using namespace std;
int main(int argc, char** argv)
{
	Mat src, dst;
	src = imread("D:/image.jpg", IMREAD_COLOR); // Read the file
	dst = src.clone();
	if (src.empty()) // Check for invalid input
	{
		cout << "Could not open or find the image" << std::endl;
		return -1;
	}
	medianBlur(src, dst, 5);
	namedWindow("Source", WINDOW_AUTOSIZE); // Create a window for display.
	imshow("Source", src); // Show our image inside it.
	namedWindow("Blurred", WINDOW_AUTOSIZE); // Create a window for display.
	imshow("Blurred", dst); // Show our image inside it.
	waitKey(0); // Wait for a keystroke in the window
	return 0;
}