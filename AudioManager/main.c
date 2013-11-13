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
#include <sys/types.h>
#include <sys/stat.h>
#include <CoreAudio/CoreAudio.h>

//AudioObjectPropertyAddress 
static AudioObjectPropertyAddress oPropertyAddress = {0,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
//environemnt variables 
extern char **environ;

/* Get Audio Device List */
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

/* Print Audio Devices */
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
     
/* Get user home directory and not the sandbox home directory */
static char *gethomedir(void){
    struct passwd *pwd;
   
    errno = 0; /* to tell if there was an error in the execution */
    pwd  = getpwuid(getuid());
    
    if(pwd){ 
        return pwd->pw_dir;
    } else {
        if (errno)
            perror("getpwuid");
        return getenv("HOME");
    }
}

static int modifypath(void){
    char *temp, *path_env;

    path_env = getenv("PATH"); 
    temp = malloc((strlen(path_env)+100)*sizeof(char));
    //modify the variable 
    sprintf(temp, "%s:%s",path_env,"/usr/local/bin:/usr/local/sbin:/opt/local/bin:/opt/local/sbin");
    //overwrite PATH with new value
    setenv("PATH", temp,1);
   
    free(temp);
    
    return 0;
}

static int spawn_cmd(pid_t *pid, char **argv){
    if(posix_spawnp(pid,argv[0], NULL, NULL,argv,NULL)!= 0){
        fprintf(stdout, "spawning %s failed: %s\n",argv[0],strerror(errno));
        return -1;
    }

    fprintf(stdout, "\n*********\nsuccessful spawn with PID: %d\n**************\n",*pid);
    return 0;
}

/* Change Default Audio device */
static OSStatus SetDefaultAudioDevice(const AudioDeviceID *devices, const UInt16 *nAvailableDevices, const char *sDeviceUID, UInt16 IO){
    OSStatus err = noErr;
    CFStringRef sUID; 
    UInt32 nOutSize = sizeof(CFStringRef);
    CFStringRef sTempUID = CFStringCreateWithCString(kCFAllocatorDefault, sDeviceUID, CFStringGetSystemEncoding());
    
    for (UInt16 nIndex = 0; nIndex < *nAvailableDevices; nIndex++){
        oPropertyAddress.mSelector = kAudioDevicePropertyDeviceUID;
        if ((err = AudioObjectGetPropertyData(devices[nIndex],&oPropertyAddress, 0, NULL,&nOutSize, &sUID)) != err){
            if (sUID)
                CFRelease(sUID);   
            CFRelease(sTempUID);
            return err;  
        }
 
        if (CFStringCompare(sUID,sTempUID,0) == kCFCompareEqualTo){
            /*
             IO: 1 -> Input
                 0 -> Output
             */
            oPropertyAddress.mSelector = IO? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice;
            if ((err = AudioObjectSetPropertyData(kAudioObjectSystemObject, &oPropertyAddress, 0, NULL, sizeof(AudioDeviceID), &devices[nIndex])) != noErr){
                if (sUID)
                    CFRelease(sUID);   
                CFRelease(sTempUID);
                return err;  
                
            } else {
                fprintf(stdout, "Set the default %s device to: %s\n",IO? "Input":"Output",sDeviceUID);
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

/* Read/Write PID values to pidfile */
static int process_pid_file(const char *pidfile, pid_t *darkice, pid_t *icecast, const UInt16 IO){
    FILE *openstream; 
    
    openstream = fopen(pidfile, "w+");
    if (openstream){
        if (IO)//Output
            fprintf(openstream,"icecast_pid:%d\ndarkice_pid:%d\n",*icecast,*darkice);
        else 
            fscanf(openstream,"icecast_pid:%d\ndarkice_pid:%d\n", (int*)icecast,(int*)darkice);
        fclose(openstream);
    } else {
        perror("fopen in process_pid_file");
        exit(EXIT_FAILURE);
    }    
    
    return 0;
}

static void *start_service (const char *pidfile){
    /*
     1. Enumerate audio devices
     2. Set correct audio device
     3. modify PATH env variable
     4. Start the services
     5. Save PID values and return
     */
        
    return NULL;
}
    
static void *stop_service(const char *pidfile){
    /*
     1. Enumerate audio devices
     2. get PID values from file
     3. if valid PID values, stop services
     4. overwrite PID values in file with 0
     5. Revert Default Audio file and return
        -> DeviceUID:	AppleHDAEngineOutput:1B,0,1,1:0
     */
    const char * default_device = "AppleHDAEngineOutput:1B,0,1,1:0";
    AudioDeviceID *devices = NULL;
    pid_t darkice, icecast;
    OSStatus err = noErr;
    UInt16 devicesAvailable =0;
    
    /*
     process_pid_file
     IO: 0 -> read
         1 -> write
     */
    if (process_pid_file(pidfile, &darkice, &icecast,0) != 0){
        fprintf(stderr,"Could not process pidfile: %s\n",pidfile);
        exit(EXIT_FAILURE);
    }
    
    if (darkice){
        if(kill(darkice, SIGKILL) != 0)
            perror("kill->darkice");
    }
    
    sleep(5); 

    if(icecast){
        if (kill(icecast, SIGKILL) != 0)
            perror("kill->icecast");
    }
    
    //Overwrite PID values in the file
    darkice = 0;
    icecast = 0;
    process_pid_file(pidfile, &darkice, &icecast, 1);
    
    if ((err = GetAudioDevices((Ptr*) devices,&devicesAvailable)) != noErr){
        if (devices)
            free(devices);
        fprintf(stderr, "Could not enumerate Audio devices\n");
        exit(EXIT_FAILURE);
    }
    
    /*
     SetDefaultAudioDevice
     IO: 1 -> Input
     0 -> Output
     */
    if ((err = SetDefaultAudioDevice(devices, &devicesAvailable,default_device,1)) != noErr){
        fprintf(stderr, "Could not set change default device to: %s",default_device);
        exit(EXIT_FAILURE);
    }
    
    //clean up
    if(devices)
        free(devices);
    
    return NULL;
}
/* Parse Commandline arguments */
static int parse_opts(const int *argc, char **argv){
    char *pidfile_path;
    UInt16 actionFlag;
    static struct option cmdline_options[] = {
        {"pidfile", required_argument,NULL,'f'},
        {"start",no_argument,NULL, 's'},
        {"stop",no_argument,NULL,'k'},
        {NULL,0,NULL,0}
    };
    
    if (*argc > 1){
        
        int opt;
        while ((opt = getopt_long(*argc, argv, "skf:",cmdline_options, NULL)) != -1){
            switch(opt){
                case 'f':
                    pidfile_path = optarg;
                case 's':
                    actionFlag = 1;
                    break;
                case 'k':
                    actionFlag = 0;
                    break;
                case '?':
                    if (optopt == 'f')
                        fprintf(stderr, "The option -%c a file argument.\n", optopt);
                    else if (isprint(optopt))
                        fprintf(stderr, "Unknown option -%c .\n",optopt);
                    else
                        fprintf(stderr, "Unkown option character `\\x%x \n",optopt);
                    exit(EXIT_FAILURE);
            }
        }
    }else {
        //usage
    }
        
    
    if (actionFlag)
        start_service(pidfile_path);
    else 
        stop_service(pidfile_path);
    
    return optind; /* Index of the last processed value in argv */
}

int main (int argc, const char * argv[])
{
    char *real_homedir = gethomedir();
    OSStatus err = noErr;
    UInt16 DevicesAvailable = 0;
    AudioDeviceID *devices = NULL;
        
    modifypath();
    
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


    // IO -> 0 : output
    // IO -> 1 : input 
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

//    spawn_cmd(&icecast_pid, icecast);
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
//   spawn_cmd(&darkice_pid,darkice);

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

