import machine
import time

def monitor_battery():
    """ Initializes ADC on GPIO 1 and continuously streams voltage data. """
    print("Initializing ADC for battery measurement")
    
    # GPIO 1 is hardwired to the battery voltage divider on Heltec V3
    adc_batt = machine.ADC(machine.Pin(1))
    
    # 11DB attenuation expands the measurement range up to approx 3.3 Volts
    adc_batt.atten(machine.ADC.ATTN_11DB)
    
    print("Starting continuous measurement")
    print("Press Ctrl C to stop")
    
    try:
        while True:
            raw_val = adc_batt.read()
            # Hardware scaling factor calculation
            voltage = (raw_val / 4095.0) * 3.3 * 4.0
            
            print("Raw ADC", raw_val, "Calculated Volts", round(voltage, 2))
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("Battery monitor stopped by user")

if __name__ == "__main__":
    monitor_battery()