//
//  main.c
//  AudioManager
//
//  Created by Nic M on 10/21/13.
//  Copyright 2013 Nic M. All rights reserved.
//

#include <stdio.h>
#include <spawn.h>
#include <stdlib.h>
#include <unistd.h>
#include <CoreAudio/CoreAudio.h>

//AudioObjectPropertyAddress 
static AudioObjectPropertyAddress oPropertyAddress = {0,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
//environemnt variables 
extern char **environ;

//Get Device List 
static OSStatus GetAudioDevices (Ptr *devices, UInt16 *devicesAvailable) 
{
    OSStatus err = noErr;
    UInt32 outSize;
    oPropertyAddress.mSelector = kAudioHardwarePropertyDevices;

    //find out how many audio devices there is
    err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,&oPropertyAddress, 0, NULL, &outSize);

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
    
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject,&oPropertyAddress,0,NULL, &outSize, *devices);
        
    if (err != noErr)
        return err;
    
    return err;

}

static OSStatus PrintDeviceNames(const AudioDeviceID *devices, const UInt16 *nDevicesFound){
    OSStatus err = noErr;
    CFStringRef sDeviceName;    
    UInt32 outSize = sizeof(CFStringRef);

    //Do the actual printing  
    fprintf(stdout, "Total Available Devices: %d\n",*nDevicesFound);

    for (UInt16 nIndex = 0; nIndex < *nDevicesFound; nIndex++){
 
        oPropertyAddress.mSelector = kAudioObjectPropertyName;
        if ((err = AudioObjectGetPropertyData(devices[nIndex],&oPropertyAddress, 0, NULL,&outSize, &sDeviceName)) != err){
            if (sDeviceName)
                CFRelease(sDeviceName);
            return err;

        }
    
        fprintf(stdout, "%d:\t%s,\t",nIndex+1,CFStringGetCStringPtr(sDeviceName,CFStringGetSystemEncoding()));
        
        oPropertyAddress.mSelector = kAudioDevicePropertyDeviceUID;

        //Device UID
        if ((err = AudioObjectGetPropertyData(devices[nIndex],&oPropertyAddress, 0, NULL,&outSize, &sDeviceName)) != err){
            if (sDeviceName)
                CFRelease(sDeviceName);
            return err;

        }
        fprintf(stdout, "DeviceUID:\t%s\n",CFStringGetCStringPtr(sDeviceName,CFStringGetSystemEncoding()));

    }

    //clean up
    if (sDeviceName)
        CFRelease(sDeviceName);
    return noErr;
}

static int StartAuxProcesses(void){
    pid_t ls_pid;
    const char *ls_argv [] ={
        "ls",
        "-l",
        "/",
        NULL
    };
    
    if(posix_spawnp(&ls_pid, ls_argv[0], NULL, NULL,ls_argv,NULL)!= 0){
        fprintf(stdout, "spawn failed: %s\n",strerror(errno));
        return -1;
    }
    
    fprintf(stdout, "successful spawn with PID: %d\n",ls_pid);
    return 0;
}

static OSStatus SetDefaultAudioDevice(const AudioDeviceID *devices, const UInt16 *nAvailableDevices, const char *sDeviceUID, UInt16 nIO){
    OSStatus err = noErr;
    CFStringRef sUID; 
    UInt32 nOutSize = sizeof(CFStringRef);
    CFStringRef sTempUID = CFStringCreateWithCString(kCFAllocatorDefault, sDeviceUID, CFStringGetSystemEncoding());
    
    //loop through the list fo find the device
    for (UInt16 nIndex = 0; nIndex < *nAvailableDevices; nIndex++){
        oPropertyAddress.mSelector = kAudioDevicePropertyDeviceUID;
        if ((err = AudioObjectGetPropertyData(devices[nIndex],&oPropertyAddress, 0, NULL,&nOutSize, &sUID)) != err){
            //clean up before exiting
            if (sUID)
                CFRelease(sUID);   
            CFRelease(sTempUID);
            return err;  
        }
 
        //is This the device I am looking for?            
        if (CFStringCompare(sUID,sTempUID,0) == kCFCompareEqualTo){
            //set this as the default device
            oPropertyAddress.mSelector = nIO? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice;
            if ((err = AudioObjectSetPropertyData(kAudioObjectSystemObject, &oPropertyAddress, 0, NULL, sizeof(AudioDeviceID), &devices[nIndex])) != noErr){
                if (sUID)
                    CFRelease(sUID);   
                CFRelease(sTempUID);
                return err;  
                
            } else {
                fprintf(stdout, "Set the default %s device to: %s\n",nIO? "Input":"Output",sDeviceUID);
                break;
            }
        }
    }

    //clean
    if (sUID)
        CFRelease(sUID);

    CFRelease(sTempUID);
    
    return noErr;
}

int main (int argc, const char * argv[])
{
    
    OSStatus err = noErr;
    UInt16 DevicesAvailable = 0;
    AudioDeviceID *devices = NULL;
    
    if ((err = GetAudioDevices((Ptr*)&devices, &DevicesAvailable)) != noErr){
        if(devices)
            free(devices);
        return err;
    }
 
    if ((err = PrintDeviceNames(devices, &DevicesAvailable)) != noErr){
        if(devices)
            free(devices);
        return err;
    }
    // nIO -> 0 : output
    // nIO -> 1 : input 
//    if ((err = SetDefaultAudioDevice(devices, &DevicesAvailable, "SoundflowerEngine:1",1)) != noErr){
//        if(devices)
//            free(devices);
//        return err;
//    }
    
    while(*environ != NULL){
//        fprintf(stdout, "%s\n",*environ);
        environ++;
    }
        
    StartAuxProcesses();
    
    //clean up
    if(devices)
        free(devices);
    
    return 0;
}

