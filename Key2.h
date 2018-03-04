/*
||
|| @file Key2.h
|| @version 1.0
|| @author Mark Stanley
|| @contact mstanley@technologist.com
||
|| @description
|| | Key2 class provides an abstract definition of a key or button
|| | and was initially designed to be used in conjunction with a
|| | state-machine.
|| #
||
|| @license
|| | This library is free software; you can redistribute it and/or
|| | modify it under the terms of the GNU Lesser General Public
|| | License as published by the Free Software Foundation; version
|| | 2.1 of the License.
|| |
|| | This library is distributed in the hope that it will be useful,
|| | but WITHOUT ANY WARRANTY; without even the implied warranty of
|| | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
|| | Lesser General Public License for more details.
|| |
|| | You should have received a copy of the GNU Lesser General Public
|| | License along with this library; if not, write to the Free Software
|| | Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
|| #
||
*/

#ifndef Keypadlib_KEY_H_
#define Keypadlib_KEY_H_

#include <Arduino.h>

#define OPEN LOW
#define CLOSED HIGH

typedef unsigned int uint;
typedef enum{ IDLE, PRESSED, HOLD, RELEASED } KeyState;

const char NO_KEY = '\0';
const int NO_KEYCODE = 0xff;

class Key2 {
public:
	byte kcode;

	KeyState kstate : 2;
	boolean stateChanged : 1;

    // methods
	Key2();
	void key_update(char userKeyChar, KeyState userState, boolean userStatus);

private:

};

#endif

/*
|| @changelog
|| | 1.0 2012-06-04 - Mark Stanley : Initial Release
|| #
*/
