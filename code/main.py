#!/usr/bin/python
from Adafruit_MotorHAT import Adafruit_MotorHAT, Adafruit_DCMotor, Adafruit_StepperMotor
import time
import atexit
import threading
import random
import RPi.GPIO as gpio

rdy = 4 #grey RB7
bit0 = 17 #purple RB4
bit1 = 18 #blue RB3
#bit2 = 27 #white RB5
ack = 22 #green RB8

gpio.setmode(gpio.BCM)

gpio.setup(rdy,gpio.IN,pull_up_down=gpio.PUD_DOWN)
gpio.setup(bit0,gpio.IN,pull_up_down=gpio.PUD_DOWN)
gpio.setup(bit1,gpio.IN,pull_up_down=gpio.PUD_DOWN)
gpio.setup(bit2,gpio.IN,pull_up_down=gpio.PUD_DOWN)
gpio.setup(ack,gpio.OUT)

# create a default object, no changes to I2C address or frequency
mh = Adafruit_MotorHAT()

# create empty threads (these will hold the stepper 1 and 2 threads)

down = Adafruit_MotorHAT.BACKWARD
right = Adafruit_MotorHAT.BACKWARD

up = Adafruit_MotorHAT.FORWARD
left = Adafruit_MotorHAT.FORWARD
stepstyles = [Adafruit_MotorHAT.SINGLE, Adafruit_MotorHAT.DOUBLE, Adafruit_MotorHAT.INTERLEAVE, Adafruit_MotorHAT.MICROSTEP]

# Tuples for each setting (stepper1 step, stepper2 step, stepper1 dir, stepper2 dir) 
bitDict = {}
bitDict[11] = 0,130,right,stepstyles[1]
bitDict[10] = 0,130,left,stepstyles[1]
bitDict[01] = 210,0,down,stepstyles[1]
bitDict[00] = 210,0,up,stepstyles[1]

myStepper0 = mh.getStepper(200, 1)      # 200 steps/rev, motor port #1
myStepper1 = mh.getStepper(200, 2)      # 200 steps/rev, motor port #1
myStepper0.setSpeed(60)          # 30 RPM
myStepper1.setSpeed(60)          # 30 RPM
#111 begin, 001,010


def read(myStepper0, myStepper1):
    num = 10*gpio.input(bit1)+gpio.input(bit0) 
    command = bitDict.get(num)

    print("command"+str(num) +str(command))
    start_time=time.time()
    st0 = threading.Thread()	
    st1 = threading.Thread()
    #while(time.time()-start_time<10 and (not st0.isAlive() or not st1.isAlive())):
    print("in thread creation")
    print(type(myStepper0))
    print(type(command[0]))
    print(type(command[1]))
    print(type(stepstyles[1]))
    print(type(command[2]))
    print(type(command[3]))
    st0 = threading.Thread(target=stepper_worker, args=(myStepper0, command[0], command[2], stepstyles[1],))
    st0.start()
    st1 = threading.Thread(target=stepper_worker, args=(myStepper1, command[1], command[2], stepstyles[1],))
    st1.start()
    st0.join()
    st1.join()
    #myStepper0.step(command[0],command[2],command[-1])
    #myStepper1.step(command[1],command[3],command[-1])
    time.sleep(2)
    gpio.output(ack,1)
    while (gpio.input(rdy)==1):
    	print("waiting for rdy to go low")
        pass
    gpio.output(ack,0)


# recommended for auto-disabling motors on shutdown!
#gpio.add_event_detect(rdy, gpio.RISING, callback=read)
def turnOffMotors():
    mh.getMotor(1).run(Adafruit_MotorHAT.RELEASE)
    mh.getMotor(2).run(Adafruit_MotorHAT.RELEASE)
    mh.getMotor(3).run(Adafruit_MotorHAT.RELEASE)
    mh.getMotor(4).run(Adafruit_MotorHAT.RELEASE)


def stepper_worker(stepper, numsteps, direction, style):
    print("Steppin!"+str(stepper))
    stepper.step(numsteps, direction, style)
    print("Done")




atexit.register(turnOffMotors)
try:
    while (True):
        while (gpio.input(rdy)==1):
	    read(myStepper0, myStepper1)
        #if not st0.isAlive():
        #    print("Stepper 1"),
        #    dir = Adafruit_MotorHAT.BACKWARD
    #        print("forward"),
    #        randomsteps = 5000
    #        print("%d steps" % randomsteps)
    #        st0 = threading.Thread(target=stepper_worker, args=(myStepper0, randomsteps, dir, stepstyles[1],))
    #        st0.start()

    #    if not st1.isAlive():
    #        print("Stepper 2"),
    #        dir = Adafruit_MotorHAT.BACKWARD
    #        print("backward"),
    #        randomsteps = 5000
    #        print("%d steps" % randomsteps)
    #        st1 = threading.Thread(target=stepper_worker, args=(myStepper1, randomsteps, dir, stepstyles[1],))
    #        st1.start()
    # Small delay to stop from constantly polling threads (see: https://forums.adafruit.com/viewtopic.php?f=50&t=104354&p=562733#p56273
        

except KeyboardInterrupt:
    gpio.cleanup()

finally:
    gpio.cleanup()
