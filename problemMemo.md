# Problem memo -- <Team Crop-Guardians> (<Mateo Estrada>, <Juan Gutierrez>)
## The user
Small-scale farmers
## The problem
In the United States, the farming industry loses roughly 1 billon dollars a year from animals disturbing farm crops, particularly, birds feeding on small scale farmer's crops
## Why a device
A farmer can't constantly monitor their crops. A phone app can't solve this because birds need a physical presence in place in order to be warded off
## The sensors
distance sensor to limit the proximity in which it detect birds and a motion sensor to detect where in the field of view of our device is the bird.
## The mechanisms
Interrupt driven input(Mechanism B):Immediate recognition to events occuring in the vicinity of the device for monitoring/logging

Multi-process architechture(Mechanism E): motion and distance sensor work in separate processes but in order for them to work in tandem, processes will have to share information throught IPC
## The risk
sensor inaccuracy due to budget contrains which will lead difficulties navigating enviornmental factors like weather and time of day.
