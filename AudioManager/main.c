//
//  main.c
//  AudioManager
//
//  Created by Nic M on 10/21/13.
//  Copyright 2013 Nic M. All rights reserved.
//

#include <stdio.h>
#include <CoreAudio/CoreAudio.h>

//Get Device List 

static OSStatus GetAudioDevices (Ptr * devices, UInt16 * devicesAvailable) 
{
    OSStatus err = noErr;
    UInt32 outSize;
    AudioObjectPropertyAddress thePropertyAddress = {kAudioHardwarePropertyDevices,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};

    //find out how many audio devices there is
    err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &thePropertyAddress, 0, NULL, &outSize);

    if (err != noErr)
        return err;
    
    //How many devices did you find?
    *devicesAvailable = outSize / sizeof(AudioDeviceID);
    
    if (*devicesAvailable < 1)
    { 
        fprintf(stderr, "No devices available");
        return err;
    }
    
    //make space for devices to 
    *devices = (Ptr) malloc(outSize);
    
    memset(*devices, 0, outSize);
    
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &thePropertyAddress,0,NULL, &outSize, *devices);
        
    if (err != noErr)
        return err;
    
    return err;

}

static OSStatus PrintDeviceNames(AudioDeviceID * devices, UInt16 * nDevicesFound){
    OSStatus err = noErr;
    CFStringRef sDeviceName;    
    UInt32 outSize = sizeof(CFStringRef);
    AudioObjectPropertyAddress oPropertyAddress = {kAudioObjectPropertyName,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};

    //Do the actual printing     
    fprintf(stdout, "Total Available Devices: %d\n",*nDevicesFound);

    for (UInt16 nIndex = 0; nIndex < *nDevicesFound; nIndex++){
        if ((err = AudioObjectGetPropertyData(devices[nIndex],&oPropertyAddress, 0, NULL,&outSize, &sDeviceName)) != err)
            return err;
        fprintf(stdout, "%d:\t%s\n",nIndex+1,CFStringGetCStringPtr(sDeviceName,CFStringGetSystemEncoding()));
    }

    //clean up
    if (sDeviceName)
        CFRelease(sDeviceName);
    return noErr;
}

int main (int argc, const char * argv[])
{

    OSStatus err = noErr;
    UInt16 DevicesAvailable = 0;
    AudioDeviceID * devices = NULL;
    
    if ((err = GetAudioDevices((Ptr*)&devices, &DevicesAvailable)) != noErr)
        return err;
 
    if ((err = PrintDeviceNames(devices, &DevicesAvailable)) != noErr)
        return err;
    
    //clean up
    if(devices)
        free(devices);
    
    return 0;
}

