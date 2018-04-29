#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include <mach/mach.h>
#include <stdio.h>

// This is the USB vendor ID for Yubico
#define kMyVendorID   0x1050


typedef struct MyPrivateData {
    io_object_t			notification;
    IOUSBDeviceInterface *	*deviceInterface;
    CFStringRef			deviceName;
    UInt32			locationID;
} MyPrivateData;

//================================================================================================
//   Globals
//================================================================================================
//
static IONotificationPortRef	gNotifyPort;
static io_iterator_t		gAddedIter;
static CFRunLoopRef		gRunLoop;

void DeviceNotification( void *		refCon,
                        io_service_t 	service,
                        natural_t 	messageType,
                        void *		messageArgument )
{
    if (messageType == kIOMessageServiceIsTerminated)
    {
        printf("Device 0x%08x removed.\n", service);

        // Run lock via idle
        //
        printf("Yubikey removed. Lock the screen.\n");

        // Lock the keychain too
        system("/usr/bin/security lock-keychain");

        io_registry_entry_t reg = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/IOResources/IODisplayWrangler");
        if (reg) {
            IORegistryEntrySetCFProperty(reg, CFSTR("IORequestIdle"), kCFBooleanTrue);
            IOObjectRelease(reg);
        }

    }
}

void DeviceAdded(void *refCon, io_iterator_t iterator)
{
    kern_return_t		kr;
    io_service_t		usbDevice;
    IOCFPlugInInterface 	**plugInInterface=NULL;
    SInt32 			score;
    HRESULT 			res;

    while ( (usbDevice = IOIteratorNext(iterator)) )
    {
        io_name_t		deviceName;
        CFStringRef		deviceNameAsCFString;
        MyPrivateData		*privateDataRef = NULL;
        UInt32			locationID;

        printf("Device 0x%08x added.\n", usbDevice);

        // Make activity and turn screen on
        printf("Wake up on Yubikey insertion.\n");
        IOPMAssertionID assertionID;
        IOPMAssertionDeclareUserActivity(CFSTR(""), kIOPMUserActiveLocal, &assertionID);

        // Add some app-specific information about this device.
        // Create a buffer to hold the data.

        privateDataRef = malloc(sizeof(MyPrivateData));
        bzero( privateDataRef, sizeof(MyPrivateData));

        // In this sample we'll just use the service's name.
        //
        kr = IORegistryEntryGetName(usbDevice, deviceName);
        if (KERN_SUCCESS != kr)
        {
            deviceName[0] = '\0';
        }

        deviceNameAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, deviceName, kCFStringEncodingASCII);

        // Dump our data to stdout just to see what it looks like.
        //
        CFShow(deviceNameAsCFString);

        privateDataRef->deviceName = deviceNameAsCFString;

        // Now, get the locationID of this device.  In order to do this, we need to create an IOUSBDeviceInterface for
        // our device.  This will create the necessary connections between our user land application and the kernel object
        // for the USB Device.
        //
        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);

        if ((kIOReturnSuccess != kr) || !plugInInterface)
        {
            printf("unable to create a plugin (%08x)\n", kr);
            continue;
        }

        // I have the device plugin, I need the device interface
        //
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID)&privateDataRef->deviceInterface);
        (*plugInInterface)->Release(plugInInterface);			// done with this
        if (res || !privateDataRef->deviceInterface)
        {
            printf("couldn't create a device interface (%08x)\n", (int) res);
            continue;
        }

        // Now that we have the IOUSBDeviceInterface, we can call the routines in IOUSBLib.h
        // In this case, we just want the locationID.
        //
        kr = (*privateDataRef->deviceInterface)->GetLocationID(privateDataRef->deviceInterface, &locationID);
        if (KERN_SUCCESS != kr)
        {
            printf("GetLocationID returned %08x\n", kr);
            continue;
        }
        else
        {
            printf("Location ID: 0x%lx\n", (unsigned long)locationID);

        }

        privateDataRef->locationID = locationID;

        // Register for an interest notification for this device. Pass the reference to our
        // private data as the refCon for the notification.
        //
        kr = IOServiceAddInterestNotification(	gNotifyPort,			// notifyPort
                                              usbDevice,			// service
                                              kIOGeneralInterest,		// interestType
                                              DeviceNotification,		// callback
                                              privateDataRef,			// refCon
                                              &(privateDataRef->notification)	// notification
                                              );

        if (KERN_SUCCESS != kr)
        {
            printf("IOServiceAddInterestNotification returned 0x%08x\n", kr);
        }

        // Done with this io_service_t
        //
        kr = IOObjectRelease(usbDevice);

        free(privateDataRef);
    }
}

void SignalHandler(int sigraised)
{
    printf("\nInterrupted\n");

    // Clean up here
    IONotificationPortDestroy(gNotifyPort);

    if (gAddedIter)
    {
        IOObjectRelease(gAddedIter);
        gAddedIter = 0;
    }

    // exit(0) should not be called from a signal handler.  Use _exit(0) instead
    //
    _exit(0);
}

//================================================================================================
//	main
//================================================================================================
//
int main (int argc, const char *argv[])
{
    mach_port_t 		masterPort;
    CFMutableDictionaryRef 	matchingDict;
    CFRunLoopSourceRef		runLoopSource;
    CFNumberRef			numberRef;
    CFStringRef         stringRef;
    kern_return_t		kr;
    long			usbVendor = kMyVendorID;
    sig_t			oldHandler;

    // pick up command line arguments
    //
    if (argc > 1)
        usbVendor = atoi(argv[1]);

    // Set up a signal handler so we can clean up when we're interrupted from the command line
    // Otherwise we stay in our run loop forever.
    //
    oldHandler = signal(SIGINT, SignalHandler);
    if (oldHandler == SIG_ERR)
        printf("Could not establish new signal handler");

    // first create a master_port for my task
    //
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr || !masterPort)
    {
        printf("ERR: Couldn't create a master IOKit Port(%08x)\n", kr);
        return -1;
    }

    printf("Looking for devices matching vendor ID=%ld\n", usbVendor);

    // Set up the matching criteria for the devices we're interested in.  The matching criteria needs to follow
    // the same rules as kernel drivers:  mainly it needs to follow the USB Common Class Specification, pp. 6-7.
    // See also http://developer.apple.com/qa/qa2001/qa1076.html
    // One exception is that you can use the matching dictionary "as is", i.e. without adding any matching criteria
    // to it and it will match every IOUSBDevice in the system.  IOServiceAddMatchingNotification will consume this
    // dictionary reference, so there is no need to release it later on.
    //
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);	// Interested in instances of class
    // IOUSBDevice and its subclasses
    if (!matchingDict)
    {
        printf("Can't create a USB matching dictionary\n");
        mach_port_deallocate(mach_task_self(), masterPort);
        return -1;
    }

    // We are interested in all USB Devices (as opposed to USB interfaces).  The Common Class Specification
    // tells us that we need to specify the idVendor, idProduct, and bcdDevice fields, or, if we're not interested
    // in particular bcdDevices, just the idVendor and idProduct.  Note that if we were trying to match an IOUSBInterface,
    // we would need to set more values in the matching dictionary (e.g. idVendor, idProduct, bInterfaceNumber and
    // bConfigurationValue.
    //

    // Create a CFNumber for the idVendor and set the value in the dictionary
    //
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor);
    CFDictionarySetValue(
                         matchingDict,
                         CFSTR(kUSBVendorID),
                         numberRef);
    CFRelease(numberRef);
    numberRef = 0;

    // Create a CFString for the wildcard product ID and set the value in the dictionary
    //
    stringRef = CFSTR("*");
    CFDictionarySetValue(
                         matchingDict,
                         CFSTR(kUSBProductID),
                         stringRef);
    CFRelease(stringRef);
    stringRef = 0;

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    //
    gNotifyPort = IONotificationPortCreate(masterPort);
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);

    gRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);

    // Now set up a notification to be called when a device is first matched by I/O Kit.
    // Note that this will not catch any devices that were already plugged in so we take
    // care of those later.
    //
    kr = IOServiceAddMatchingNotification(gNotifyPort,			// notifyPort
                                          kIOFirstMatchNotification,	// notificationType
                                          matchingDict,			// matching
                                          DeviceAdded,			// callback
                                          NULL,				// refCon
                                          &gAddedIter			// notification
                                          );

    // Iterate once to get already-present devices and arm the notification
    //
    DeviceAdded(NULL, gAddedIter);

    // Now done with the master_port
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;

    // Start the run loop. Now we'll receive notifications.
    //
    printf("Starting run loop.\n");
    CFRunLoopRun();

    // We should never get here
    //
    printf("Unexpectedly back from CFRunLoopRun()!\n");

    return 0;
}
