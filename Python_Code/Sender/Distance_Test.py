import machine
import time
import vl53l0x

def test_sensor():
    """ Initializes I2C with pull-ups and reads distance data sequentially. """
    print("Initializing I2C bus")
    pin_sda = machine.Pin(41, machine.Pin.IN, machine.Pin.PULL_UP)
    pin_scl = machine.Pin(42, machine.Pin.IN, machine.Pin.PULL_UP)
    i2c = machine.I2C(1, sda=pin_sda, scl=pin_scl)
    
    print("Initializing VL53L0X sensor")
    sensor = vl53l0x.VL53L0X(i2c)
    
    print("Starting measurement loop")
    for i in range(100):
        dist = sensor.read()
        print("Distance mm", dist)
        time.sleep(1)
        
    print("Test sequence completed")

if __name__ == "__main__":
    test_sensor()
