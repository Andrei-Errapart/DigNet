#ifndef Joystick_h_
#define Joystick_h_

#define DIRECTINPUT_VERSION 0x700
#include <dinput.h> // DirectInput header

// Based on http://www.dreamincode.net/code/snippet434.htm

struct joyplug_state {
     short joy_axis_x,joy_axis_y;
     bool button[36];
}; // Specialized joypad state


class Joystick {

    joyplug_state joystate_[8];
    LPDIRECTINPUT lpdii;
    HINSTANCE hmain_;
    LPDIRECTINPUTDEVICE lpdiid;
    LPDIRECTINPUTDEVICE2 joy[8];
    DIJOYSTATE joys[8];
    DIPROPRANGE joyrange;
    void free_joypads();
    void readState();
public:

    bool hasJoystick();
    bool init(HINSTANCE hmain);
    Joystick();
    ~Joystick();
    const joyplug_state& getState();
    
};

#endif
