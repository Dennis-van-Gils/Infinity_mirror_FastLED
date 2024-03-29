/*
Edited:
  - Added name string to class `State`
  - Added `getCurrentStateName(char *buffer)` to class `FiniteStateMachine`

Dennis van Gils
18-11-2021

||
|| @file FiniteStateMachine.cpp
|| @version 1.7
|| @author Alexander Brevig
|| @contact alexanderbrevig@gmail.com
||
|| @description
|| | Provide an easy way of making finite state machines
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

#include "FiniteStateMachine.h"

// FINITE STATE
State::State(void (*updateFunction)()) {
  userEnter = 0;
  userUpdate = updateFunction;
  userExit = 0;
}

State::State(void (*enterFunction)(), void (*updateFunction)()) {
  userEnter = enterFunction;
  userUpdate = updateFunction;
  userExit = 0;
}

State::State(void (*enterFunction)(), void (*updateFunction)(),
             void (*exitFunction)()) {
  userEnter = enterFunction;
  userUpdate = updateFunction;
  userExit = exitFunction;
}

State::State(const char *name, void (*updateFunction)()) {
  snprintf(_name, STATE_NAME_LEN, name);
  userEnter = 0;
  userUpdate = updateFunction;
  userExit = 0;
}

State::State(const char *name, void (*enterFunction)(),
             void (*updateFunction)()) {
  snprintf(_name, STATE_NAME_LEN, name);
  userEnter = enterFunction;
  userUpdate = updateFunction;
  userExit = 0;
}

State::State(const char *name, void (*enterFunction)(),
             void (*updateFunction)(), void (*exitFunction)()) {
  snprintf(_name, STATE_NAME_LEN, name);
  userEnter = enterFunction;
  userUpdate = updateFunction;
  userExit = exitFunction;
}

// what to do when entering this state
void State::enter() {
  if (userEnter) {
    userEnter();
  }
}

// what to do when this state updates
void State::update() {
  if (userUpdate) {
    userUpdate();
  }
}

// what to do when exiting this state
void State::exit() {
  if (userExit) {
    userExit();
  }
}

const char *State::getName() { return _name; }
// END FINITE STATE

// FINITE STATE MACHINE
FiniteStateMachine::FiniteStateMachine(State &current) {
  needToTriggerEnter = true;
  currentState = nextState = &current;
  stateChangeTime = 0;
}

FiniteStateMachine &FiniteStateMachine::update() {
  // simulate a transition to the first state
  // this only happens the first time update is called
  if (needToTriggerEnter) {
    currentState->enter();
    needToTriggerEnter = false;
  } else {
    if (currentState != nextState) {
      immediateTransitionTo(*nextState);
    }
    currentState->update();
  }
  return *this;
}

FiniteStateMachine &FiniteStateMachine::transitionTo(State &state) {
  nextState = &state;
  stateChangeTime = millis();
  return *this;
}

FiniteStateMachine &FiniteStateMachine::immediateTransitionTo(State &state) {
  currentState->exit();
  currentState = nextState = &state;
  currentState->enter();
  stateChangeTime = millis();
  return *this;
}

// return the current state
State &FiniteStateMachine::getCurrentState() { return *currentState; }

// check if state is equal to the currentState
boolean FiniteStateMachine::isInState(State &state) const {
  if (&state == currentState) {
    return true;
  } else {
    return false;
  }
}

const char *FiniteStateMachine::getCurrentStateName() {
  return currentState->_name;
}

unsigned long FiniteStateMachine::timeInCurrentState() {
  return millis() - stateChangeTime;
}
// END FINITE STATE MACHINE
