/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <sys/stat.h>

using namespace cv;
using namespace std;

#define OBJECT_AREA 9000
#define MAX_DEFECT_TYPES 3

// Lower and upper value of color range of the object
#define LOW_H 0
#define LOW_S 0
#define LOW_V 47
#define HIGH_H 179
#define HIGH_S 255
#define HIGH_V 255

int count_object = 0;
char object_count[200];

// Create dataset for PCA Analysis
Mat createBuffer(const vector<Point> &contour_points)
{
    Mat data_points = Mat(contour_points.size(), 2, CV_64FC1);
    for (int row = 0; row < data_points.rows; row++)
    {
        data_points.at<double>(row, 0) = contour_points[row].x;
        data_points.at<double>(row, 1) = contour_points[row].y;
    }

    return data_points;
}

// Get the orientation of the object
double getOrientation(Mat data_points)
{
    // Perform PCA Analysis on the data points
    PCA pca(data_points, Mat(), CV_PCA_DATA_AS_ROW);
    Point2d eigen_vector;
    eigen_vector = Point2d(pca.eigenvectors.at<double>(0, 0),
                           pca.eigenvectors.at<double>(0, 1));

    // Get the orientation in radians
    double angle = atan2(eigen_vector.y, eigen_vector.x);
    return angle;
}

/*********************************************** Orientation detection **************************************************
** Step 1: Filter the contours based on the area to get the object of interest
** Step 2: Invoke getOrientation() and pass the contour of the object as an argument
**         getOrientation() function performs the PCA analysis 
** Step 3: Save the image of the object in "orientation" folder if orientation defect is present
*************************************************************************************************************************/

bool detectOrientation(Mat &frame, const vector<vector<Point> > contours, Rect object)
{
    char object_defect[200];
    bool defect_flag = false;
    double area = 0, angle = 0;
    static int orientation_num = 0;
    Mat img = frame.clone();
    for (size_t i = 0; i < contours.size(); ++i)
    {
        // Calculate the area of each contour
        area = contourArea(contours[i]);

        // Ignore contours that are too small to be the object of interest
        if (area < OBJECT_AREA)
            continue;

        Mat data_points = createBuffer(contours[i]);

        // Find the orientation of each contour
        angle = getOrientation(data_points);

        // If angle is less than 0.5 then we conclude that no orientation defect is present
        if (angle < 0.5)
        {
            // Set defect_flag to true when the object does not contain orientation defect
            defect_flag = true;
        }
        else
        {
            cout << "Orientation defect detected in object " << count_object << endl;
            sprintf(object_defect, "Defect : Orientation");
            sprintf(object_count, "Object Number : %d", count_object);
            putText(img, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
            putText(img, object_defect, Point(5, 90), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
            orientation_num += 1;
            imwrite(format("orientation/object_%d.jpg", count_object), img(Rect(object.tl() - Point(5, 5), object.br() + Point(5, 5))));
            imshow("Out", img);
            waitKey(2000);
        }
        break;
    }
    return defect_flag;
}

/*********************************************** Color defect detection **************************************************
** Step 1: Increase the brightness of the image
** Step 2: Convert the image to HSV Format. 
**         HSV color space gives more information about the colors of the image. It helps to identify distinct colors in the image.   
** Step 3: Threshold the image based on the color using "inRange" function.
**         Pass range of the color, which is considered as a defect for the object, as one of the argument to inRange function,
**         to create a mask
** Step 4: Morphological opening and closing is done on the mask to remove noises and fill the gaps
** Step 5: Find the contours on the mask image. Contours are filtered based on the area to get the contours of defective area.
**         Contour of the defective area is then drawn on the original image to visualize
** Step 6: Save the image of the object in "color" folder if color defect is present
*************************************************************************************************************************/

bool detectColor(Mat &frame, Rect object)
{
    bool defect_flag = false, color_flag = false;
    char object_defect[200];
    double area = 0;
    static int color_num = 0;
    Mat imgHSV, img_thresholded, img;
    vector<Vec4i> hierarchy;
    vector<vector<Point> > contours;

    img = frame.clone();

    // Increase the brightness of the image
    frame.convertTo(imgHSV, -1, 1, 20);

    // Convert the captured frame from BGR to HSV
    cvtColor(imgHSV, imgHSV, COLOR_BGR2HSV);

    // Threshold the image
    inRange(imgHSV, Scalar(0, 0, 0), Scalar(174, 73, 255), img_thresholded);

    // Morphological opening (remove small objects from the foreground)
    erode(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
    dilate(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

    // Morphological closing (fill small holes in the foreground)
    dilate(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
    erode(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
    findContours(img_thresholded, contours, hierarchy, CV_RETR_LIST, CV_CHAIN_APPROX_NONE);

    for (size_t i = 0; i < contours.size(); ++i)
    {
        area = contourArea(contours[i]);
        if (area > 2000 && area < 10000)
        {
            drawContours(img, contours, i, Scalar(0, 0, 255), 2, 8, hierarchy, 0);
            sprintf(object_defect, "Defect : Color");
            sprintf(object_count, "Object Number : %d", count_object);
            putText(img, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
            putText(img, object_defect, Point(5, 130), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
            color_num += 1;
            defect_flag = false;
            color_flag = true;
        }
        else if (color_flag != true)
        {
            defect_flag = true;
        }
    }

    if (color_flag == true)
    {
        cout << "Color defect detected in object " << count_object << endl;
        imwrite(format("color/object_%d.jpg", count_object), img(Rect(object.tl() - Point(5, 5), object.br() + Point(5, 5))));
        imshow("Out", img);
        waitKey(2000);
    }
    return defect_flag;
}

/**************************************************** Crack detection **************************************************
** Step 1: Convert the image to gray scale
** Step 2: Blur the gray image to remove the noises
** Step 3: Find the edges on the blurred image to get the contours of possible cracks
** Step 4: Filter the contours to get the contour of the crack
** Step 5: Draw the contour on the original image for visualization
** Step 6: Save the image of the object in "crack" folder if crack defect is present
*************************************************************************************************************************/

bool detectCrack(Mat &frame, Rect object)
{
    bool defect_flag = false;
    char object_defect[200], object_count[200];
    double area = 0;
    Mat detected_edges, img;
    static int crack_num = 0;
    int low_threshold = 130, kernel_size = 3, ratio = 3;
    vector<Vec4i> hierarchy;
    vector<vector<Point> > contours;

    // Convert the captured frame from BGR to GRAY
    cvtColor(frame, img, CV_BGR2GRAY);
    blur(img, img, Size(7, 7));

    // Find the edges
    Canny(img, detected_edges, low_threshold, low_threshold * ratio, kernel_size);

    // Find the contours
    findContours(detected_edges, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

    // Draw contours
    if (contours.size() != 0)
    {
        for (int i = 0; i < contours.size(); i++)
        {
            area = contourArea(contours[i]);
            if (area > 20 || area < 9)
            {
                cout << "Crack detected in object " << count_object << endl;
                sprintf(object_defect, "Defect : Crack");
                sprintf(object_count, "Object Number : %d", count_object);
                putText(frame, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
                putText(frame, object_defect, Point(5, 130), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
                drawContours(frame, contours, i, Scalar(0, 255, 0), 1, 8, hierarchy, 0);
                crack_num += 1;
                imwrite(format("crack/object_%d.jpg", count_object), frame(Rect(object.tl() - Point(5, 5), object.br() + Point(5, 5))));
                imshow("Out", frame);
                waitKey(2000);
                break;
            }
            else
            {
                // Set defect_flag to true when the object does not contain any cracks
                defect_flag = true;
            }
        }
    }
    else
    {
        defect_flag = true;
    }
    return defect_flag;
}

int main(int argc, char *argv[])
{
    char object_defect[200], object_count[200];
    const char *dir_names[] = {"crack", "color", "orientation", "no_defect"};
    int frame_count = 0, nodefect_count = 0, num_of_dir = 4;
    Mat frame, dst, detected_edges, img_hsv, img_thresholded;
    vector<Vec4i> hierarchy;
    vector<vector<Point> > contours;
    Rect object;

    struct stat st = {0};
    VideoCapture capture;

    // Create directories to save images of objects
    for (int num = 0; num < num_of_dir; num++)
    {
        // Check if the directory exists, create one if it does not
        if (stat(dir_names[num], &st) == -1)
            if ((mkdir(dir_names[num], 00777)) == -1)
            {
                cout << "Error in creating " << dir_names[num] << "directory" << endl;
                return 0;
            }
    }

    // Check for command line arguments
    if (argc != 2)
    {
        cout << "Usage: " << endl;
        cout << "# To give the video input from file " << endl;
        cout << "./product-flaw-detector <VideoFile>" << endl
             << endl;
        cout << "# To give the video input from camera" << endl;
        cout << "./product-flaw-detector cam " << endl;
        return 0;
    }
    if (strcmp(argv[1], "cam") == 0)
    {
        cout << "Input from camera " << endl;
        capture.open(0);
    }
    else
    {
        cout << "Loading the video " << argv[1] << endl;
        capture.open(argv[1]);
    }
    // Check if video is loaded successfully
    if (!capture.isOpened())
    {
        cout << "Problem loading the video!!!" << endl;
        return 1;
    }

    sprintf(object_count, "Object Number : %d", count_object);
    for (;;)
    {
        Rect maxArea = Rect(Point(0, 0), Point(1, 1));
        // Read the frame from the stream
        capture >> frame;
        if (frame.empty())
        {
            break;
        }
        frame_count++;
        nodefect_count = 0;
        // Check every 40th frame (Number chosen based on the frequency of object on conveyor belt)
        if (frame_count % 40 == 0)
        {
            // Convert RGB image to HSV color space
            cvtColor(frame, img_hsv, CV_RGB2HSV);

            // Thresholding of an Image in a color range
            inRange(img_hsv, Scalar(LOW_H, LOW_S, LOW_V), Scalar(HIGH_H, HIGH_S, HIGH_V), img_thresholded);

            // Morphological opening (remove small objects from the foreground)
            erode(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
            dilate(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

            // Morphological closing (fill small holes in the foreground)
            dilate(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
            erode(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

            // Find the contours on the image
            findContours(img_thresholded, contours, hierarchy, CV_RETR_LIST, CV_CHAIN_APPROX_NONE);

            // Find the contour with maximum area
            for (size_t i = 0; i < contours.size(); i++)
            {
                object = boundingRect(contours[i]);
                if ((object.width * object.height) > (maxArea.width * maxArea.height))
                {
                    maxArea = object;
                }
            }
            object = maxArea;

            // Save only the object instead of the complete frame
            if (object.width * object.height > OBJECT_AREA)
            {
                count_object++;
                sprintf(object_count, "Object Number : %d", count_object);
            }
            else
            {
                continue;
            }

            if (detectOrientation(frame, contours, object) == true)
            {
                nodefect_count++;
            }
            if (detectColor(frame, object) == true)
            {
                nodefect_count++;
            }
            if (detectCrack(frame, object) == true)
            {
                nodefect_count++;
            }

            // Display and save the object with no defect
            if (nodefect_count == MAX_DEFECT_TYPES)
            {
                cout << "No defect detected in object " << count_object << endl;
                sprintf(object_count, "Object Number : %d", count_object);
                sprintf(object_defect, "No Defect");
                putText(frame, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
                putText(frame, object_defect, Point(5, 90), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
                imwrite(format("no_defect/object_%d.jpg", count_object), frame(Rect(object.tl() - Point(5, 5), object.br() + Point(5, 5))));
                imshow("Out", frame);
                waitKey(1000);
            }
        }
        putText(frame, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
        imshow("Out", frame);
        waitKey(20);
        hierarchy.clear();
        contours.clear();
    }
    return 0;
}
