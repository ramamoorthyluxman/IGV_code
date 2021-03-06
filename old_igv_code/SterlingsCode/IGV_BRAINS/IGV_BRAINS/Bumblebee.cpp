//
//  Bumblebee.cpp
//  IGV_BRAINS
//
//  Created by Sterling A. Baldwin on 12/21/13.
//  Copyright (c) 2013 Sterling A. Baldwin. All rights reserved.
//

#include "Bumblebee.h"
//=============================================================================
// PGR Includes
//=============================================================================
#include "triclops.h"
#include "pgrflycapture.h"
#include "pgrflycapturestereo.h"
#include "pnmutils.h"


//=============================================================================
// PCL Includes
//=============================================================================

#include "/Users/sterling/Downloads/pcl-master/common/include/pcl/pcl_base.h"
#include "/Users/sterling/Downloads/pcl-master/common/include/pcl/point_cloud.h"
#include "/Users/sterling/Downloads/pcl-master/io/include/pcl/io/pcd_io.h"
#include </Users/sterling/Downloads/pcl-master/common/include/pcl/point_types.h"

//=============================================================================
// Some Macros for the Triclops and Flycapture
//=============================================================================

#define _HANDLE_TRICLOPS_ERROR( function, error ) \
{ \
if( error != TriclopsErrorOk ) \
{ \
printf( \
"ERROR: %s reported %s.\n", \
function, \
triclopsErrorToString( error ) ); \
exit( 1 ); \
} \
} \

#define _HANDLE_FLYCAPTURE_ERROR( function, error ) \
{ \
if( error != FLYCAPTURE_OK ) \
{ \
printf( \
"ERROR: %s reported %s.\n", \
function, \
flycaptureErrorToString( error ) ); \
exit( 1 ); \
} \
} \




Bumblebee::Bumblebee()
{
    pcl::PointCloud<pcl::PointXYZRGB>* cloud = new pcl::PointCloud<pcl::PointXYZRGB>;
    initialize_cloud();
}

Bumblebee::~Bumblebee() { delete cloud; }

void Bumblebee::initialize_cloud()
{
    if(cloud->points->size() > 0)
    {
        delete cloud;
        cloud = new pcl::PointCloud<pcl::PointXYZRGB>;
    }
    
    
    TriclopsInput       stereoData;
    TriclopsInput       colorData;
    TriclopsImage16     depthImage16;
    TriclopsImage       monoImage = {0};
    TriclopsColorImage  colorImage = {0};
    TriclopsContext     triclops;
    TriclopsError       te;
    
    
    FlyCaptureContext	   flycapture;
    FlyCaptureImage	   flycaptureImage;
    FlyCaptureInfoEx	   pInfo;
    FlyCapturePixelFormat   pixelFormat;
    FlyCaptureError	   fe;
    
    float	       x, y, z;
    int		       r, g, b;
    FILE*	       pointFile;
    int		       nPoints = 0;
    int		       pixelinc ;
    int		       i, j, k;
    unsigned short*     row;
    unsigned short      disparity;
    int iMaxCols = 0;
    int iMaxRows = 0;
    
    char* szCalFile;
    monoImage = {0};
    colorImage = {0};
    
    
    // Open the camera
    fe = flycaptureCreateContext( &flycapture );
    _HANDLE_FLYCAPTURE_ERROR( "flycaptureCreateContext()", fe );
    
    // Initialize the Flycapture context
    fe = flycaptureInitialize( flycapture, 0 );
    _HANDLE_FLYCAPTURE_ERROR( "flycaptureInitialize()", fe );
    
    // Save the camera's calibration file, and return the path
    fe = flycaptureGetCalibrationFileFromCamera( flycapture, &szCalFile );
    _HANDLE_FLYCAPTURE_ERROR( "flycaptureGetCalibrationFileFromCamera()", fe );
    
    // Create a Triclops context from the cameras calibration file
    te = triclopsGetDefaultContextFromFile( &triclops, szCalFile );
    _HANDLE_TRICLOPS_ERROR( "triclopsGetDefaultContextFromFile()", te );
    
    // Get camera information
    fe = flycaptureGetCameraInfo( flycapture, &pInfo );
    _HANDLE_FLYCAPTURE_ERROR( "flycatpureGetCameraInfo()", fe );
    
    if (pInfo.CameraType == FLYCAPTURE_COLOR)
    {
        pixelFormat = FLYCAPTURE_RAW16;
    }
    else
    {
        pixelFormat = FLYCAPTURE_MONO16;
    }
    
    switch (pInfo.CameraModel)
    {
        case FLYCAPTURE_BUMBLEBEE2:
        {
            unsigned long ulValue;
            flycaptureGetCameraRegister( flycapture, 0x1F28, &ulValue );
            
            if ( ( ulValue & 0x2 ) == 0 )
            {
                // Hi-res BB2
                iMaxCols = 1024;
                iMaxRows = 768;
            }
            else
            {
                // Low-res BB2
                iMaxCols = 640;
                iMaxRows = 480;
            }
        }
            break;
            
        case FLYCAPTURE_BUMBLEBEEXB3:
            iMaxCols = 1280;
            iMaxRows = 960;
            break;
            
        default:
            te = TriclopsErrorInvalidCamera;
            _HANDLE_TRICLOPS_ERROR( "triclopsCheckCameraModel()", te );
            break;
    }
    
    // Start transferring images from the camera to the computer
    fe = flycaptureStartCustomImage(
                                    flycapture, 3, 0, 0, iMaxCols, iMaxRows, 100, pixelFormat);
    _HANDLE_FLYCAPTURE_ERROR( "flycaptureStart()", fe );
    
    // Set up some stereo parameters:
    // Set to 320x240 output images
    te = triclopsSetResolution( triclops, 240, 320 );
    _HANDLE_TRICLOPS_ERROR( "triclopsSetResolution()", te );
    
    // Set disparity range
    te = triclopsSetDisparity( triclops, 0, 100 );
    _HANDLE_TRICLOPS_ERROR( "triclopsSetDisparity()", te );
    
    // Lets turn off all validation except subpixel and surface
    // This works quite well
    te = triclopsSetTextureValidation( triclops, 0 );
    _HANDLE_TRICLOPS_ERROR( "triclopsSetTextureValidation()", te );
    te = triclopsSetUniquenessValidation( triclops, 0 );
    _HANDLE_TRICLOPS_ERROR( "triclopsSetUniquenessValidation()", te );
    
    // Turn on sub-pixel interpolation
    te = triclopsSetSubpixelInterpolation( triclops, 1 );
    _HANDLE_TRICLOPS_ERROR( "triclopsSetSubpixelInterpolation()", te );
    
    // Grab an image from the camera
    fe = flycaptureGrabImage2( flycapture, &flycaptureImage );
    _HANDLE_FLYCAPTURE_ERROR( "flycaptureGrabImage()", fe );
    
    // Extract information from the FlycaptureImage
    int imageCols = flycaptureImage.iCols;
    int imageRows = flycaptureImage.iRows;
    int imageRowInc = flycaptureImage.iRowInc;
    int iSideBySideImages = flycaptureImage.iNumImages;
    unsigned long timeStampSeconds = flycaptureImage.timeStamp.ulSeconds;
    unsigned long timeStampMicroSeconds = flycaptureImage.timeStamp.ulMicroSeconds;
    
    // Create buffers for holding the color and mono images
    unsigned char* rowIntColor =
    new unsigned char[ imageCols * imageRows * iSideBySideImages * 4 ];
    unsigned char* rowIntMono =
    new unsigned char[ imageCols * imageRows * iSideBySideImages ];
    
    // Create a temporary FlyCaptureImage for preparing the stereo image
    FlyCaptureImage tempColorImage;
    FlyCaptureImage tempMonoImage;
    
    tempColorImage.pData = rowIntColor;
    tempMonoImage.pData = rowIntMono;
    
    // Convert the pixel interleaved raw data to row interleaved format
    fe = flycapturePrepareStereoImage( flycapture, flycaptureImage, &tempMonoImage, &tempColorImage  );
    _HANDLE_FLYCAPTURE_ERROR( "flycapturePrepareStereoImage()", fe );
    
    // Pointers to positions in the color buffer that correspond to the beginning
    // of the red, green and blue sections
    unsigned char* redColor = NULL;
    unsigned char* greenColor = NULL;
    unsigned char* blueColor = NULL;
    
    redColor = rowIntColor;
    if (flycaptureImage.iNumImages == 2)
    {
        greenColor = redColor + ( 4 * imageCols );
        blueColor = redColor + ( 4 * imageCols );
    }
    
    if (flycaptureImage.iNumImages == 3)
    {
        greenColor = redColor + ( 4 * imageCols );
        blueColor = redColor + ( 2 * 4 * imageCols );
    }
    
    // Pointers to positions in the mono buffer that correspond to the beginning
    // of the red, green and blue sections
    unsigned char* redMono = NULL;
    unsigned char* greenMono = NULL;
    unsigned char* blueMono = NULL;
    
    redMono = rowIntMono;
    if (flycaptureImage.iNumImages == 2)
    {
        greenMono = redMono + imageCols;
        blueMono = redMono + imageCols;
    }
    
    if (flycaptureImage.iNumImages == 3)
    {
        greenMono = redMono + imageCols;
        blueMono = redMono + ( 2 * imageCols );
    }
    
    // Use the row interleaved images to build up a packed TriclopsInput.
    // A packed triclops input will contain a single image with 32 bpp.
    te = triclopsBuildPackedTriclopsInput(
                                          imageCols,
                                          imageRows,
                                          imageRowInc * 4,
                                          timeStampSeconds,
                                          timeStampMicroSeconds,
                                          redColor,
                                          &colorData );
    _HANDLE_TRICLOPS_ERROR( "triclopsBuildPackedTriclopsInput()", te );
    
    // Use the row interleaved images to build up an RGB TriclopsInput.
    // An RGB triclops input will contain the 3 raw images (1 from each camera).
    te = triclopsBuildRGBTriclopsInput(
                                       imageCols,
                                       imageRows,
                                       imageRowInc,
                                       timeStampSeconds,
                                       timeStampMicroSeconds,
                                       redMono,
                                       greenMono,
                                       blueMono,
                                       &stereoData);
    _HANDLE_TRICLOPS_ERROR( "triclopsBuildRGBTriclopsInput()", te );
    
    // Preprocessing the images
    te = triclopsRectify( triclops, &stereoData );
    _HANDLE_TRICLOPS_ERROR( "triclopsRectify()", te );
    
    // Stereo processing
    te = triclopsStereo( triclops ) ;
    _HANDLE_TRICLOPS_ERROR( "triclopsStereo()", te );
    
    // Retrieve the interpolated depth image from the context
    te = triclopsGetImage16( triclops,
                            TriImg16_DISPARITY,
                            TriCam_REFERENCE,
                            &depthImage16 );
    _HANDLE_TRICLOPS_ERROR( "triclopsGetImage16()", te );
    
    // Rectify the color image if applicable
    if ( pixelFormat == FLYCAPTURE_RAW16 )
    {
        te = triclopsRectifyColorImage( triclops,
                                       TriCam_REFERENCE,
                                       &colorData,
                                       &colorImage );
        _HANDLE_TRICLOPS_ERROR( "triclopsRectifyColorImage()", te );
    }
    else
    {
        te = triclopsGetImage( triclops,
                              TriImg_RECTIFIED,
                              TriCam_REFERENCE,
                              &monoImage );
        _HANDLE_TRICLOPS_ERROR( "triclopsGetImage()", te );
    }
    
    
    pcl::PointXYZRGB::Ptr newPoint (new pcl::PointXYZRGB);
    // Determine the number of pixels spacing per row
    pixelinc = depthImage16.rowinc/2;
    for ( i = 0, k = 0; i < depthImage16.nrows; i++ )
    {
        row     = depthImage16.data + i * pixelinc;
        for ( j = 0; j < depthImage16.ncols; j++, k++ )
        {
            disparity = row[j];
            
            // do not save invalid points
            if ( disparity < 0xFF00 )
            {
                // convert the 16 bit disparity value to floating point x,y,z
                triclopsRCD16ToXYZ( triclops, i, j, disparity, &x, &y, &z );
                
                // look at points within a range
                if ( z < 5.0 )
                {
                    if ( pixelFormat == FLYCAPTURE_RAW16 )
                    {
                        r = (int)colorImage.red[k];
                        g = (int)colorImage.green[k];
                        b = (int)colorImage.blue[k];
                    }
                    else
                    {
                        // For mono cameras, we just assign the same value to RGB
                        r = (int)monoImage.data[k];
                        g = (int)monoImage.data[k];
                        b = (int)monoImage.data[k];
                    }
                    
                    newPoint.x = x;
                    newPoint.y = y;
                    newPoint.z = z;
                    newPoint.r = r;
                    newPoint.g = g;
                    newPoint.b = b;
                    cloud->points.push_back(newPoint);
                    nPoints++;
                }
            }
        }
    }
    
    
    // Close the camera
    fe = flycaptureStop( flycapture );
    _HANDLE_FLYCAPTURE_ERROR( "flycaptureStop()", fe );
    
    // Delete the image buffer, it is not needed once the TriclopsInput
    // has been built
    delete [] rowIntColor;
    redColor = NULL;
    greenColor = NULL;
    blueColor = NULL;
    
    delete [] rowIntMono;
    redMono = NULL;
    greenMono = NULL;
    blueMono = NULL;
    
    fe = flycaptureDestroyContext( flycapture );
    _HANDLE_FLYCAPTURE_ERROR( "flycaptureDestroyContext()", fe );
    
    // Destroy the Triclops context
    te = triclopsDestroyContext( triclops ) ;
    _HANDLE_TRICLOPS_ERROR( "triclopsDestroyContext()", te );
}















