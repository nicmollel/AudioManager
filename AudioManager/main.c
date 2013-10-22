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
    Boolean outWriteable;
    
    //find out how many audio devices there is
    err = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices,&outSize, &outWriteable);
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
    
    err = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &outSize, (void*) *devices);
    
    if (err != noErr)
        return err;
    
    return err;

}

int main (int argc, const char * argv[])
{

    OSStatus err = noErr;
//    UInt32 outSize = 0;
    UInt16 DevicesAvailable = 0;
    AudioDeviceID * devices = NULL;
    
    if ((err = GetAudioDevices((Ptr*)&devices, &DevicesAvailable)) != noErr)
        return err;
    
    return 0;
}

