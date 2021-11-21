#define DIRECTINPUT_VERSION 0x700

#include "Joystick.h"
#include <dinput.h>

unsigned short num_joypads; // Number of plugged in joypads
GUID joyid[8]; // Joypad ID's
char joynames[8][MAX_PATH]; // Strings of all the names
bool joy_in[8];


Joystick::Joystick()   
{
}

Joystick::~Joystick(void)
{
}

int CALLBACK enum_joypads(
    LPCDIDEVICEINSTANCE lpddi,
    LPVOID guid_ptr)
{
     if(num_joypads == 8)
          return DIENUM_STOP;
     // Record the GUID
     joyid[num_joypads] = lpddi->guidInstance;
     // Copy the joypad name
     for(int x = 0;x < MAX_PATH;x++)
          joynames[num_joypads][x] = lpddi->tszProductName[x];
     joy_in[num_joypads] = true;
     num_joypads++; // A joypad was registered
     return DIENUM_CONTINUE;
}
bool
Joystick::hasJoystick()
{
    return num_joypads!=0;
}

// Creates the joypad objects
bool Joystick::init(HINSTANCE hmain) 
{
    hmain_=hmain;
    if(FAILED(DirectInputCreate(hmain_,DIRECTINPUT_VERSION,&lpdii,NULL))) { // Create the main object
        return false; // Return failure code
    }
//    write_debug("DInput Joypad Open.");
    // Now guid all devices
    if(FAILED(lpdii->EnumDevices(DIDEVTYPE_JOYSTICK,(LPDIENUMDEVICESCALLBACKA)enum_joypads,NULL,DIEDFL_ATTACHEDONLY))) {
        return false; // Return failure code
    }
    // Now create all devices
    for(int x = 0;x < num_joypads;x++) {
        lpdii->CreateDevice(joyid[x],&lpdiid,NULL);
        if(FAILED(lpdiid->QueryInterface(IID_IDirectInputDevice2,(void**)&joy[x]))) {
            return false;
        }
        lpdiid->Release();
        lpdiid = NULL;
        // Set cooperative level
        if(FAILED(joy[x]->SetCooperativeLevel(GetDesktopWindow(),DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))) {
            return false;
        }
        if(FAILED(joy[x]->SetDataFormat(&c_dfDIJoystick))) {
            return false;
        }
        // Set range and dead zones
        // X Range
        joyrange.lMin = -255;
        joyrange.lMax = 255;
        joyrange.diph.dwSize = sizeof(DIPROPRANGE);
        joyrange.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        joyrange.diph.dwObj = DIJOFS_X;
        joyrange.diph.dwHow = DIPH_BYOFFSET;
        joy[x]->SetProperty(DIPROP_RANGE,&joyrange.diph);
        // Y Range
        joyrange.lMin = -255;
        joyrange.lMax = 255;
        joyrange.diph.dwSize = sizeof(DIPROPRANGE);
        joyrange.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        joyrange.diph.dwObj = DIJOFS_Y;
        joyrange.diph.dwHow = DIPH_BYOFFSET;
        joy[x]->SetProperty(DIPROP_RANGE,&joyrange.diph);
        // Acquire the joypad
        if(FAILED(joy[x]->Acquire())) {
            return false;
        }
        // Poll the joypad
        if(FAILED(joy[x]->Poll())) {
            return false;
        }
    }
    return true; // Return success code
}
void Joystick::free_joypads()
{
     // Call this on game exit
     for(int x = 0;x <8;x++) {
          if(joy[x]) {
               joy[x]->Unacquire();
               joy[x]->Release();
          }
     }
     if(lpdii) {
          lpdii->Release();
     }
}

// Returns number of joypads
int joypads()
{
     return num_joypads;
}

// Returns a joypad vendor name (returns the pointer to string)
char *str_joy(int index)
{
     return &joynames[index][0];
}

// Gathers the joypad states
void Joystick::readState()
{
    // Call this every frame
    for(int x = 0;x < num_joypads;x++) {
        // Get states of joypads
        if(joy_in[x]) {
            if(FAILED(joy[x]->GetDeviceState(sizeof(DIJOYSTATE),(LPVOID)&joys[x]))) {
                joy_in[x] = false;
            }
            joystate_[x].joy_axis_x = (short)joys[x].lX;
            joystate_[x].joy_axis_y = (short)joys[x].lY;
            for(int y = 0;y < 32;y++) {
                if(joys[x].rgbButtons[y] > 32) {
                    joystate_[x].button[y] = true;
                } else {
                    joystate_[x].button[y] = false;
                }
            }
            switch(joys[x].rgdwPOV[0]) {
            case -1:
                joystate_[x].button[32] = false; // UP
                joystate_[x].button[33] = false; // RIGHT
                joystate_[x].button[34] = false; // DOWN
                joystate_[x].button[35] = false; // LEFT
                break;
            case 0:
                joystate_[x].button[32] = true; // UP
                joystate_[x].button[33] = false; // RIGHT
                joystate_[x].button[34] = false; // DOWN
                joystate_[x].button[35] = false; // LEFT
                break;
            case 4500:
                joystate_[x].button[32] = true; // UP
                joystate_[x].button[33] = true; // RIGHT
                joystate_[x].button[34] = false; // DOWN
                joystate_[x].button[35] = false; // LEFT
                break;
            case 9000:
                joystate_[x].button[32] = false; // UP
                joystate_[x].button[33] = true; // RIGHT
                joystate_[x].button[34] = false; // DOWN
                joystate_[x].button[35] = false; // LEFT
                break;
            case 9000+4500:
                joystate_[x].button[32] = false; // UP
                joystate_[x].button[33] = true; // RIGHT
                joystate_[x].button[34] = true; // DOWN
                joystate_[x].button[35] = false; // LEFT
                break;
            case 18000:
                joystate_[x].button[32] = false; // UP
                joystate_[x].button[33] = false; // RIGHT
                joystate_[x].button[34] = true; // DOWN
                joystate_[x].button[35] = false; // LEFT
                break;
            case 18000+4500:
                joystate_[x].button[32] = false; // UP
                joystate_[x].button[33] = false; // RIGHT
                joystate_[x].button[34] = true; // DOWN
                joystate_[x].button[35] = true; // LEFT
                break;
            case 18000+9000:
                joystate_[x].button[32] = false; // UP
                joystate_[x].button[33] = false; // RIGHT
                joystate_[x].button[34] = false; // DOWN
                joystate_[x].button[35] = true; // LEFT
                break;
            case 18000+9000+4500:
                joystate_[x].button[32] = true; // UP
                joystate_[x].button[33] = false; // RIGHT
                joystate_[x].button[34] = false; // DOWN
                joystate_[x].button[35] = true; // LEFT
                break;
            }
        } else {
            ZeroMemory(&joystate_[x],sizeof(joyplug_state));
        }
    }
}

const joyplug_state& Joystick::getState()
{
    readState();
    return joystate_[0];
}