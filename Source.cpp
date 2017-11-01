#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cmath>
#include <iostream>
#include <windows.h>

using namespace cv;
using namespace std;

//Convert to grayscale and find edges using canny
Mat cannyImage(const Mat source)
{
	Mat gray;
	cvtColor(source, gray, CV_BGR2GRAY);

	Mat result;
	Canny(gray, result, 100, 200, 5);

	return result;
}

//Handle situation when the image didn't load properly
bool handleEmptyImage(Mat source)
{
	if (source.empty())
	{
		cout << "Image didn't load properly" << endl;
		cout << "Shutting down application" << endl;
		Sleep(2000);
		return true;
	}
	return false;
}

//Show first image, image with edges detected and changed image
void showImages(Mat source, Mat edges, Mat result)
{
	imshow("Source", source);
	imshow("Edges", edges);
	imshow("Result", result);
}

//Find shapes in a given set of curves and generate changed image
void findShapes(Mat result, vector<vector<Point>> curves)
{
	cout << "Curves found: " << curves.size() << endl;
	vector<Point> approx_curve;
	for (int i = 0; i < curves.size(); i++)
	{
		approxPolyDP(Mat(curves[i]), approx_curve, arcLength(Mat(curves[i]), true)*0.02, true);
		if (fabs(contourArea(curves[i])) < 100 || !isContourConvex(approx_curve)) // ignore if curve is not convex or its relatively small
			continue;
		if (approx_curve.size() == 3)
		{
			cout << "Found triangle" << endl;
		}
	}
}

int main()
{
	string path = "C:/input/h.png";
	Mat sourceImage = imread(path);
	Mat resultImage = sourceImage.clone();
	if (handleEmptyImage(sourceImage))
		return -1;
	Mat edgesImage = cannyImage(sourceImage);
	
	vector<vector<Point>> curves;
	vector<Point> approx_curve;
	findContours(edgesImage, curves, RETR_TREE, CHAIN_APPROX_SIMPLE);

	findShapes(resultImage, curves);

	showImages(sourceImage, edgesImage, resultImage);
	waitKey(0);
	return 0;
}
