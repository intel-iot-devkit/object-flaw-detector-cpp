/*
* Copyright (c) 2018 Intel Corporation.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
* LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
* OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cmath>
#include <string>
#include <iostream>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <sys/stat.h>

/* 
 * Using influxdb.hpp file from "https://github.com/orca-zhang/influxdb-cpp".
 * Pull the file and place it in current working directory.
 */


using namespace cv;
using namespace std;

#define OBJECT_AREA_MIN 9000
#define OBJECT_AREA_MAX 50000
#define MAX_DEFECT_TYPES 3

// Lower and Upper value of color range of the object
#define LOW_H 0
#define LOW_S 0
#define LOW_V 47
#define HIGH_H 179
#define HIGH_S 255
#define HIGH_V 255

int KeyPressed; // Ascii value for the key pressed
int count_object = 0;
float one_pixel_length = 0.0;
char object_count[200];
char object_height[100]; // Length of the object
char object_width[100];  // Width of the object
std::string output_string;
std::vector<float> measurement;

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
    PCA pca(data_points, Mat(), PCA::DATA_AS_ROW);
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

std::vector<std::string> detectOrientation(Mat &frame, const vector<vector<Point>> contours, Rect object)
{
    std::vector<std::string> defects;
    std::string defect_flag;
    double area = 0, angle = 0;
    static int orientation_num = 0;
    Mat img = frame.clone();
    for (size_t i = 0; i < contours.size(); ++i)
    {
        // Calculate the area of each contour
        area = contourArea(contours[i]);

        // Ignore contours that are too small to be the object of interest
        if (area < OBJECT_AREA_MIN)
            continue;

        Mat data_points = createBuffer(contours[i]);

        // Find the orientation of each contour
        angle = getOrientation(data_points);

        // If angle is less than 0.5 then we conclude that no orientation defect is present
        if (angle < 0.5)
        {
            // Set defect_flag to true when the object does not contain orientation defect
            defect_flag = "true";
        }
        else
        {
            cout << "Orientation defect detected in object " << count_object << endl;
            output_string = output_string + "Orientation" + " ";
            sprintf(object_count, "Object Number : %d", count_object);
            putText(img, object_height, Point(5, 80), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
            putText(img, object_width, Point(5, 110), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
            putText(img, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
            putText(img, output_string, Point(5, 140), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
            orientation_num += 1;
            imwrite(format("orientation/object_%d.png", count_object), img(Rect(object.tl(), object.br())));
            imshow("Out", img);
            KeyPressed = waitKey(2000);
            if (KeyPressed == 113 || KeyPressed == 81) // Ascii value of Q = 81 and q = 113
                exit(0);
        }
        break;
    }
    defects.push_back(defect_flag);
    return defects;
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

std::vector<std::string> detectColor(Mat &frame, Rect object)
{
    std::vector<std::string> defects;
    std::string defect_flag;
    bool color_flag;
    double area = 0;
    static int color_num = 0;
    Mat imgHSV, img_thresholded, img;
    vector<Vec4i> hierarchy;
    vector<vector<Point>> contours;

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
    findContours(img_thresholded, contours, hierarchy, RETR_LIST, CHAIN_APPROX_NONE);

    for (size_t i = 0; i < contours.size(); ++i)
    {
        area = contourArea(contours[i]);
        if (area > 2000 && area < 10000)
        {
            drawContours(img, contours, i, Scalar(0, 0, 255), 2, 8, hierarchy, 0);
            sprintf(object_count, "Object Number : %d", count_object);
           // putText(frame, object_width, Point(5, 110), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
            color_num += 1;
            defect_flag = "false";
            color_flag = "true";
        }
        else if (color_flag != true)
        {
            defect_flag = "true";
        }
    }

    if (color_flag == true)
    {
        output_string = output_string + "Color" + " ";
        cout << "Color defect detected in object " << count_object << endl;
        putText(img, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
        putText(img, output_string, Point(5, 140), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
        putText(img, object_height, Point(5, 80), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
        putText(img, object_width, Point(5, 110), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
        imwrite(format("color/object_%d.png", count_object), img(Rect(object.tl(), object.br())));
        imshow("Out", img);
        KeyPressed = waitKey(2000);
        if (KeyPressed == 113 || KeyPressed == 81) // Ascii value of Q = 81 and q = 113
            exit(0);
    }
    defects.push_back(defect_flag);
    return defects;
}

/**************************************************** Crack detection **************************************************
** Step 1: Convert the image to gray scale
** Step 2: Blur the gray image to remove the noises
** Step 3: Find the edges on the blurred image to get the contours of possible cracks
** Step 4: Filter the contours to get the contour of the crack
** Step 5: Draw the contour on the original image for visualization
** Step 6: Save the image of the object in "crack" folder if crack defect is present
*************************************************************************************************************************/

std::vector<std::string> detectCrack(Mat &frame, Rect object)
{
    std::vector<std::string> defects;
    std::string defect_flag= "false";
    char object_count[200];
    double area = 0;
    Mat detected_edges, img;
    static int crack_num = 0;
    int low_threshold = 130, kernel_size = 3, ratio = 3;
    vector<Vec4i> hierarchy;
    vector<vector<Point>> contours;

    // Convert the captured frame from BGR to GRAY
    cvtColor(frame, img, COLOR_BGR2GRAY);
    blur(img, img, Size(7, 7));

    // Find the edges
    Canny(img, detected_edges, low_threshold, low_threshold * ratio, kernel_size);

    // Find the contours
    findContours(detected_edges, contours, hierarchy, RETR_LIST, CHAIN_APPROX_SIMPLE);

    // Draw contours
    if (contours.size() != 0)
    {
        for (size_t i = 0; i < contours.size(); i++)
        {
            area = contourArea(contours[i]);
            if (area > 20 || area < 9)
            {  
                sprintf(object_count, "Object Number : %d", count_object);
                putText(frame, object_height, Point(5, 80), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
                putText(frame, object_width, Point(5, 110), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
                putText(frame, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
                drawContours(frame, contours, i, Scalar(0, 255, 0), 2, 8, hierarchy, 0);
            }
            else
            {
                // Set defect_flag to true when the object does not contain any cracks
                defect_flag = "true";
            }
        }
        if (defect_flag == "false")
        {   
            cout << "Crack detected in object " << count_object << endl;
            output_string = output_string + "Crack" + " ";
            crack_num += 1;
            imwrite(format("crack/object_%d.png", count_object), frame(Rect(object.tl(), object.br())));
            putText(frame, output_string, Point(5, 140), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
            imshow("Out", frame);
            KeyPressed = waitKey(2000);
            if (KeyPressed == 113 || KeyPressed == 81) // Ascii value of Q = 81 and q = 113
                exit(0);
        }
    }
    else
    {
        defect_flag = "true";
    }
    defects.push_back(defect_flag);
    return defects;
}

/** Write the data to influxDB **/
int writeToInfluxDB(int count_object, int is_crack_defect, int is_orientation_defect, int is_color_defect)
{
    string server_response;
    influxdb_cpp::server_info server_info("127.0.0.1", 8086, "Defect");

    int ret = influxdb_cpp::builder()
                  .meas("Defect")
                  .field("objectNumber", count_object)
                  .field("crackDefect", is_crack_defect)
                  .field("orientationDefect", is_orientation_defect)
                  .field("colorDefect", is_color_defect)
                  .post_http(server_info, &server_response);

    if (ret != 0)
    {
        cout << "Error uploading data to influxdb!" << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/** Calculate euclidean distance between two points **/
double calculate_distance(Point pts1, Point pts2)
{
    double dist;

    double x = pts1.x - pts2.x;
    double y = pts1.y - pts2.y;

    // Calculating euclidean distance
    dist = pow(x, 2) + pow(y, 2);
    dist = sqrt(dist);

    return dist;
}

// Returns the rounded value
double round(double value, int place)
{
    return floor(value * pow(10, place) + 0.5) / pow(10, place);
}

// Return the Length and Width of the object
std::vector<float> find_dimensions(vector<cv::Point> pts)
{
    std::vector<float> dimension;
    double length, width, length1, width1;
    Point tltr, blbr, tlbl, trbr;

    // Calculate Euclidean ditance
    length = int(calculate_distance(pts[0], pts[1]));
    width = int(calculate_distance(pts[1], pts[2]));

    // Rounding the values to two decimal place
    length1 = round(length * one_pixel_length * 10, 2);
    width1 = round(width * one_pixel_length * 10, 2);

    if (length1 > width1)
    {
        dimension.push_back(length1);
        dimension.push_back(width1);
    }
    else
    {
        dimension.push_back(width1);
        dimension.push_back(length1);
    }
    return dimension;
}

int main(int argc, char *argv[])
{
    bool is_orientation_defect, is_color_defect, is_crack_defect;
    char object_count[200], filepath[50];
    const char *dir_names[] = {"crack", "color", "orientation", "no_defect"};
    int frame_count = 0, num_of_dir = 4, status = 0;
    Mat frame, dst, detected_edges, img_hsv, img_thresholded, frame_color, frame_orientation, frame_crack, frame_nodefect;
    int width_of_video = 0, height_of_video = 0, opt = 0, field = 0, dist = 0;
    vector<Vec4i> hierarchy;
    vector<vector<Point>> contours;
    std::vector<std::string> orientation_defect, color_defect, crack_defect;
    float diagonal_length_of_image_plane = 0.0, diagonal_length_in_pixel = 0.0, radians = 0.0;

    measurement.push_back(0.0);
    measurement.push_back(0.0);

    Rect object;
    DIR *dir;
    struct dirent *ent;
    struct stat st = {0};
    string server_response;
    VideoCapture capture;

    // Create directories to save images of objects
    for (int num = 0; num < num_of_dir; num++)
    {
        // Check if the directory exists, clean the directory if it does
        if (stat(dir_names[num], &st) == 0)
        {
            if ((dir = opendir(dir_names[num])) != NULL)
            {
                while ((ent = readdir(dir)) != NULL)
                {
                    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                        continue;

                    sprintf(filepath, "%s/%s", dir_names[num], ent->d_name);
                    status = remove(filepath);
                    if (status != 0)
                    {
                        cout << "Unable to delete the file " << ent->d_name << std::endl;
                        perror("\n\tError");
                        return EXIT_FAILURE;
                    }
                }
                closedir(dir);
            }
            else
                cout << "Error opening the directory" << endl;
        }
        // Create the directory if it does not exist
        else if ((mkdir(dir_names[num], 00777)) == -1)
        {
            cout << "Error in creating " << dir_names[num] << "directory" << endl;
            return EXIT_FAILURE;
        }
    }

    // Check for command line arguments
    if (argc < 2)
    {
        cout << "Usage: " << endl;
        cout << "# To give the video input from file " << endl;
        cout << "./product-flaw-detector -i <VideoFile>" << endl
             << endl;
        cout << "# To give the video input from camera" << endl;
        cout << "./product-flaw-detector -i cam " << endl;
        cout << "To give focal length and distance " << endl;
        cout << "./product-flaw-detector -i <VideoFile> -f <value> -d <value>" << endl;
        return EXIT_FAILURE;
    }

    // Parsing Command Line arguments
    while ((opt = getopt(argc, argv, ":f:d:i:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            if (strcmp(optarg, "cam") == 0)
            {
                cout << "Input from camera " << endl;
                capture.open(0);
            }
            else if (strcmp(optarg, "CAM") == 0)
            {
                cout << "Input from camera " << endl;
                capture.open(0);
            }
            else
            {
                cout << "Loading the video " << optarg << endl;
                capture.open(optarg);
            }
            break;
        case 'f':
            field = atoi(optarg);
            break;
        case 'd':
            dist = atoi(optarg);
            break;
        }
    }

    if (field > 0 and dist > 0)
    {
        width_of_video = capture.get(3);
        height_of_video = capture.get(4);
        // Convert degrees to radians
        radians = (field / 2) * 0.0174533;
        //Calculate the diagonal length of image in millimeters using field of view of camera and distance between object and camera.
        diagonal_length_of_image_plane = (abs(2 * (dist / 10) * tan(radians)));
        // Calculate diagonal length of image in pixel
        diagonal_length_in_pixel = sqrt(pow(width_of_video, 2) + pow(height_of_video, 2));
        // Convert one pixel value in millimeters
        one_pixel_length = (diagonal_length_of_image_plane / diagonal_length_in_pixel);
    }
    /*  If distance between camera and object and field of view of camera
        are not provided, then 96 pixels per inch is considered.
        pixel_lengh = 2.54 cm (1 inch) / 96 pixels */
    if (one_pixel_length == 0)
        one_pixel_length = 0.0264583333;

    // Check if video is loaded successfully
    if (!capture.isOpened())
    {
        cout << "Problem loading the video!!!" << endl;
        return EXIT_FAILURE;
    }

    // Create the database in influxDB named "Defect"
    influxdb_cpp::server_info server_info("127.0.0.1", 8086);
    influxdb_cpp::create_db(server_response, "Defect", server_info);

    sprintf(object_count, "Object Number : %d", count_object);
    for (;;)
    {
        Rect maxArea = Rect(Point(0, 0), Point(1, 1));

        // Read the frame from the stream
        capture >> frame;

        if (frame.empty())
        {
            cout << "Video stream ended" << endl;
            break;
        }
        frame_count++;

        // Check every 40th frame (Number chosen based on the frequency of object on conveyor belt)
        if (frame_count % 40 == 0)
        {
            measurement[0] = 0;
            measurement[1] = 0;
            is_orientation_defect = true;
            is_color_defect = true;
            is_crack_defect = true;

            // Convert RGB image to HSV color space
            cvtColor(frame, img_hsv, COLOR_RGB2HSV);

            // Thresholding of an Image in a color range
            inRange(img_hsv, Scalar(LOW_H, LOW_S, LOW_V), Scalar(HIGH_H, HIGH_S, HIGH_V), img_thresholded);

            // Morphological opening (remove small objects from the foreground)
            erode(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
            dilate(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

            // Morphological closing (fill small holes in the foreground)
            dilate(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
            erode(img_thresholded, img_thresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

            // Find the contours on the image
            findContours(img_thresholded, contours, hierarchy, RETR_LIST, CHAIN_APPROX_NONE);
            vector<RotatedRect> minRect(contours.size());

            // Find the contour with maximum area
            for (size_t contour = 0; contour < contours.size(); contour++)
            {
                object = boundingRect(contours[contour]);
                Point2f rect_points[4];
                std::vector<cv::Point> pts;
                if (object.width * object.height > OBJECT_AREA_MIN && OBJECT_AREA_MAX > object.width * object.height)
                { 
                    minRect[contour] = minAreaRect(contours[contour]);

                    minRect[contour].points(rect_points);
                    for (int point = 0; point < 4; point++)
                    {
                        int x = ceil(rect_points[point].x); // Coordinate of x
                        int y = ceil(rect_points[point].y); // Coordinate of y
                        pts.push_back(Point(x, y));
                    }

                    measurement = find_dimensions(pts);
                    sprintf(object_height, "Length (mm) = %.2f", measurement[0]);
                    sprintf(object_width,  "Width (mm)  = %.2f", measurement[1]);
                    frame_crack = frame.clone();
                    frame_orientation = frame.clone();
                    frame_color = frame.clone();
                    frame_nodefect = frame.clone();
                    
                    // Save only the object instead of the complete frame
                    count_object++;
                    sprintf(object_count, "Object Number : %d", count_object);
                    
                    // Check the occurence of defects in object, if the function returns true, corresponding defect is not present in the object
                    output_string.clear();
                    output_string = "Defect : ";

                    orientation_defect = detectOrientation(frame_orientation, contours, object);
                    if (strncmp(orientation_defect[0].c_str(), "true", 4) == 0)
                    {
                        is_orientation_defect = false;
                    }

                    color_defect = detectColor(frame_color, object);
                    if (strncmp(color_defect[0].c_str(), "true", 4) == 0)
                    {
                        is_color_defect = false;
                    }

                    crack_defect = detectCrack(frame_crack, object);
                    if (strcmp(crack_defect[0].c_str(), "true") == 0)
                    {
                        is_crack_defect = false;
                    }

                    // Display and save the object with no defect
                    if (is_orientation_defect == false && is_color_defect == false && is_crack_defect == false)
                    {
                        output_string = output_string + "No Defect" + " ";
                        cout << "No defect detected in object " << count_object << endl;
                        sprintf(object_count, "Object Number : %d", count_object);
                        putText(frame_nodefect, object_height, Point(5, 80), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
                        putText(frame_nodefect, object_width, Point(5, 110), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
                        putText(frame_nodefect, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
                        putText(frame_nodefect, output_string, Point(5, 140), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
                        imwrite(format("no_defect/object_%d.png", count_object), frame(Rect(object.tl(), object.br())));
                        imshow("Out", frame_nodefect);
                        KeyPressed = waitKey(2000);
                        if (KeyPressed == 113 || KeyPressed == 81) // Ascii value of Q = 81 and q = 113
                            exit(0);
                    }

                    status = writeToInfluxDB(count_object, is_crack_defect, is_orientation_defect, is_color_defect);
                    if (status == EXIT_FAILURE)
                    {
                        return EXIT_FAILURE;
                    }
                    cout << object_height << " " << object_width << endl;
                    KeyPressed = waitKey(25);
                    if (KeyPressed == 113 || KeyPressed == 81) // Ascii value of Q = 81 and q = 113
                        exit(0);
                }
            }
        }
        sprintf(object_height, "Length (mm) = %.2f", measurement[0]);
        sprintf(object_width, "Width (mm)  = %.2f", measurement[1]);
        putText(frame, "Press q to quit", Point(410, 50), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255), 2);
        putText(frame, object_count, Point(5, 50), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
        putText(frame, object_height, Point(5, 80), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
        putText(frame, object_width, Point(5, 110), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
        putText(frame, "Defect : ", Point(5, 140), FONT_HERSHEY_DUPLEX, 0.75, Scalar(255, 255, 255), 2);
        imshow("Out", frame);
        KeyPressed = waitKey(25);
        if (KeyPressed == 113 || KeyPressed == 81) // Ascii value of Q = 81 and q = 113
            exit(0);
        hierarchy.clear();
        contours.clear();
    }
    return EXIT_SUCCESS;
}
