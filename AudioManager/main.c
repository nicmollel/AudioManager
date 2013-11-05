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
#include <pwd.h>
#include <getopt.h>
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
                                        
static char *getRealHomeDir(void){
    struct passwd *pwd = getpwuid(getuid());
    
    if(pwd){ 
        return pwd->pw_dir;
    } else {
        return getenv("HOME");
    }
}

static int modifyPath(void){
    char *temp, *path_env;

    path_env = getenv("PATH"); 
    temp = (char*) malloc((strlen(path_env)+100)*sizeof(char));
    //modify the variable 
    sprintf(temp, "%s:%s",path_env,"/usr/local/bin:/usr/local/sbin:/opt/local/bin:/opt/local/sbin");
    //overwrite PATH with new value
    setenv("PATH", temp,1);
   
    free(temp);
    
    return 0;
}

static int SpawnProcesses(pid_t *pid, const char **argv){
    if(posix_spawnp(pid,argv[0], NULL, NULL,argv,NULL)!= 0){
        fprintf(stdout, "spawning %s failed: %s\n",argv[0],strerror(errno));
        return -1;
    }

    fprintf(stdout, "\n*********\nsuccessful spawn with PID: %d\n**************\n",*pid);
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
    char *real_homedir = getRealHomeDir();
    pid_t icecast_pid,darkice_pid;
    OSStatus err = noErr;
    UInt16 actionFlag,DevicesAvailable = 0;
    AudioDeviceID *devices = NULL;
    

    //command line options
    static struct option cmdline_options[] = {
        {"pidfile", required_argument,NULL,'f'},
        {"start",no_argument,NULL, 's'},
        /* default is 0 and so if started without any option, it will stop running processes in the provided 
            pid_file */
        {"stop",no_argument,NULL,'k'},
        {NULL,0,NULL,0}
    };
    
    modifyPath();
    
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
    
    char *temp = malloc(strlen(real_homedir)+(sizeof(char)*50));
    sprintf(temp, "%s/%s",real_homedir,"icecast.xml");
    const char *icecast [] ={
        "icecast",
        "-c",
        temp,
        "-b",
        NULL
    };

//    SpawnProcesses(&icecast_pid, icecast);
    sleep(5); //wait for icecast to be all set up
    free(temp);
    temp = malloc(strlen(real_homedir)+(sizeof(char)*50));
    sprintf(temp, "%s/%s",real_homedir,"darkice.cfg");
 
    const char *darkice [] ={
        "darkice",
        "-c",
        temp,
        NULL
    };
//   SpawnProcesses(&darkice_pid,darkice);

    while(*environ != NULL){
//        fprintf(stdout, "%s\n",*environ);
        environ++;
    }
//            

    //clean up
    if(devices)
        free(devices);   
    free(temp);
    return 0;
}

