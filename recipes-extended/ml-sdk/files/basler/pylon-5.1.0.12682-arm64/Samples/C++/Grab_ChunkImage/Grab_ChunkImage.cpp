// Grab_ChunkImage.cpp
/*
    Note: Before getting started, Basler recommends reading the Programmer's Guide topic
    in the pylon C++ API documentation that gets installed with pylon.
    If you are upgrading to a higher major version of pylon, Basler also
    strongly recommends reading the Migration topic in the pylon C++ API documentation.

    Basler cameras provide chunk features: The cameras can generate certain information about each image,
    e.g. frame counters, time stamps, and CRC checksums, that is appended to the image data as data "chunks".
    This sample illustrates how to enable chunk features, how to grab images, and how to process the appended
    data. When the camera is in chunk mode, it transfers data blocks that are partitioned into chunks. The first
    chunk is always the image data. When chunk features are enabled, the image data chunk is followed by chunks
    containing the information generated by the chunk features.
*/

// Include files to use the pylon API
#include <pylon/PylonIncludes.h>
#ifdef PYLON_WIN_BUILD
#    include <pylon/PylonGUI.h>
#endif

// Namespace for using pylon objects.
using namespace Pylon;

#if defined( USE_1394 )
// Settings for using Basler IEEE 1394 cameras.
#include <pylon/1394/Basler1394InstantCamera.h>
typedef Pylon::CBasler1394InstantCamera Camera_t;
typedef Pylon::CBasler1394ImageEventHandler ImageEventHandler_t; // Or use Camera_t::ImageEventHandler_t
typedef Pylon::CBasler1394GrabResultPtr GrabResultPtr_t; // Or use Camera_t::GrabResultPtr_t
using namespace Basler_IIDC1394CameraParams;
#elif defined ( USE_GIGE )
// Settings for using Basler GigE cameras.
#include <pylon/gige/BaslerGigEInstantCamera.h>
typedef Pylon::CBaslerGigEInstantCamera Camera_t;
typedef Pylon::CBaslerGigEImageEventHandler ImageEventHandler_t; // Or use Camera_t::ImageEventHandler_t
typedef Pylon::CBaslerGigEGrabResultPtr GrabResultPtr_t; // Or use Camera_t::GrabResultPtr_t
using namespace Basler_GigECameraParams;
#elif defined( USE_USB )
// Settings for using Basler USB cameras.
#include <pylon/usb/BaslerUsbInstantCamera.h>
typedef Pylon::CBaslerUsbInstantCamera Camera_t;
typedef Pylon::CBaslerUsbImageEventHandler ImageEventHandler_t; // Or use Camera_t::ImageEventHandler_t
typedef Pylon::CBaslerUsbGrabResultPtr GrabResultPtr_t; // Or use Camera_t::GrabResultPtr_t
using namespace Basler_UsbCameraParams;
#else
#error Camera type is not specified. For example, define USE_GIGE for using GigE cameras.
#endif

// Namespace for using cout.
using namespace std;

// Example of a device specific handler for image events.
class CSampleImageEventHandler : public ImageEventHandler_t
{
public:
    virtual void OnImageGrabbed( Camera_t& camera, const GrabResultPtr_t& ptrGrabResult)
    {
        // The chunk data is attached to the grab result and can be accessed anywhere.

        // Generic parameter access:
        // This shows the access via the chunk data node map. This method is available for all grab result types.
        GenApi::CIntegerPtr chunkTimestamp( ptrGrabResult->GetChunkDataNodeMap().GetNode( "ChunkTimestamp"));

        // Access the chunk data attached to the result.
        // Before accessing the chunk data, you should check to see
        // if the chunk is readable. When it is readable, the buffer
        // contains the requested chunk data.
        if ( IsReadable( chunkTimestamp))
            cout << "OnImageGrabbed: TimeStamp (Result) accessed via node map: " << chunkTimestamp->GetValue() << endl;

        // Native parameter access:
        // When using the device specific grab results the chunk data can be accessed
        // via the members of the grab result data.
        if ( IsReadable(ptrGrabResult->ChunkTimestamp))
            cout << "OnImageGrabbed: TimeStamp (Result) accessed via result member: " << ptrGrabResult->ChunkTimestamp.GetValue() << endl;
    }
};

// Number of images to be grabbed.
static const uint32_t c_countOfImagesToGrab = 5;

int main(int argc, char* argv[])
{
    // The exit code of the sample application.
    int exitCode = 0;

    // Before using any pylon methods, the pylon runtime must be initialized. 
    PylonInitialize();

    try
    {
        // Only look for cameras supported by Camera_t
        CDeviceInfo info;
        info.SetDeviceClass( Camera_t::DeviceClass());

        // Create an instant camera object with the first found camera device that matches the specified device class.
        Camera_t camera( CTlFactory::GetInstance().CreateFirstDevice( info));

        // Print the model name of the camera.
        cout << "Using device " << camera.GetDeviceInfo().GetModelName() << endl;

        // Register an image event handler that accesses the chunk data.
        camera.RegisterImageEventHandler( new CSampleImageEventHandler, RegistrationMode_Append, Cleanup_Delete);

        // Open the camera.
        camera.Open();

        // A GenICam node map is required for accessing chunk data. That's why a small node map is required for each grab result.
        // Creating a lot of node maps can be time consuming.
        // The node maps are usually created dynamically when StartGrabbing() is called.
        // To avoid a delay caused by node map creation in StartGrabbing() you have the option to create
        // a static pool of node maps once before grabbing.
        //camera.StaticChunkNodeMapPoolSize = camera.MaxNumBuffer.GetValue();

        // Enable chunks in general.
        if (GenApi::IsWritable(camera.ChunkModeActive))
        {
            camera.ChunkModeActive.SetValue(true);
        }
        else
        {
            throw RUNTIME_EXCEPTION( "The camera doesn't support chunk features");
        }

        // Enable time stamp chunks.
        camera.ChunkSelector.SetValue(ChunkSelector_Timestamp);
        camera.ChunkEnable.SetValue(true);

#ifndef USE_USB // USB camera devices provide generic counters. An explicit FrameCounter value is not provided by USB camera devices.
        // Enable frame counter chunks.
        camera.ChunkSelector.SetValue(ChunkSelector_Framecounter);
        camera.ChunkEnable.SetValue(true);
#endif

        // Enable CRC checksum chunks.
        camera.ChunkSelector.SetValue(ChunkSelector_PayloadCRC16);
        camera.ChunkEnable.SetValue(true);

        // Start the grabbing of c_countOfImagesToGrab images.
        // The camera device is parameterized with a default configuration which
        // sets up free-running continuous acquisition.
        camera.StartGrabbing( c_countOfImagesToGrab);

        // This smart pointer will receive the grab result data.
        GrabResultPtr_t ptrGrabResult;

        // Camera.StopGrabbing() is called automatically by the RetrieveResult() method
        // when c_countOfImagesToGrab images have been retrieved.
        while( camera.IsGrabbing())
        {
            // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
            // RetrieveResult calls the image event handler's OnImageGrabbed method.
            camera.RetrieveResult( 5000, ptrGrabResult, TimeoutHandling_ThrowException);

#ifdef PYLON_WIN_BUILD
            // Display the image
            Pylon::DisplayImage(1, ptrGrabResult);
#endif

            cout << "GrabSucceeded: " << ptrGrabResult->GrabSucceeded() << endl;

            // The result data is automatically filled with received chunk data.
            // (Note:  This is not the case when using the low-level API)
            cout << "SizeX: " << ptrGrabResult->GetWidth() << endl;
            cout << "SizeY: " << ptrGrabResult->GetHeight() << endl;
            const uint8_t *pImageBuffer = (uint8_t *) ptrGrabResult->GetBuffer();
            cout << "Gray value of first pixel: " << (uint32_t) pImageBuffer[0] << endl;

            // Check to see if a buffer containing chunk data has been received.
            if (PayloadType_ChunkData != ptrGrabResult->GetPayloadType())
            {
                throw RUNTIME_EXCEPTION( "Unexpected payload type received.");
            }

            // Since we have activated the CRC Checksum feature, we can check
            // the integrity of the buffer first.
            // Note: Enabling the CRC Checksum feature is not a prerequisite for using
            // chunks. Chunks can also be handled when the CRC Checksum feature is deactivated.
            if (ptrGrabResult->HasCRC() && ptrGrabResult->CheckCRC() == false)
            {
                throw RUNTIME_EXCEPTION( "Image was damaged!");
            }

            // Access the chunk data attached to the result.
            // Before accessing the chunk data, you should check to see
            // if the chunk is readable. When it is readable, the buffer
            // contains the requested chunk data.
            if (IsReadable(ptrGrabResult->ChunkTimestamp))
                cout << "TimeStamp (Result): " << ptrGrabResult->ChunkTimestamp.GetValue() << endl;

#ifndef USE_USB // USB camera devices provide generic counters. An explicit FrameCounter value is not provided by USB camera devices.
            if (IsReadable(ptrGrabResult->ChunkFramecounter))
                cout << "FrameCounter (Result): " << ptrGrabResult->ChunkFramecounter.GetValue() << endl;
#endif

            cout << endl;
        }

        // Disable chunk mode.
        camera.ChunkModeActive.SetValue(false);
    }
    catch (const GenericException &e)
    {
        // Error handling.
        cerr << "An exception occurred." << endl
        << e.GetDescription() << endl;
        exitCode = 1;
    }

    // Comment the following two lines to disable waiting on exit.
    cerr << endl << "Press Enter to exit." << endl;
    while( cin.get() != '\n');

    // Releases all pylon resources. 
    PylonTerminate(); 

    return exitCode;
}

