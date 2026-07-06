import machine
import time
import uos
import esp32
import ucryptolib
import vl53l0x
from sx1262 import SX1262

class CryptoEngine:
    def __init__(self):
        # Initialize Non-Volatile Storage for AES key retrieval
        nvs = esp32.NVS("sys_sec")
        buf = bytearray(16)
        nvs.get_blob("aes_main", buf)
        self.key = bytes(buf)

    def _pad(self, data: bytes) -> bytes:
        # Apply PKCS7 padding to ensure data length is a multiple of 16 bytes
        pad_len = 16 - (len(data) % 16)
        return data + bytes([pad_len] * pad_len)

    def encrypt(self, text: str) -> bytes:
        # Encrypt string data using AES in CBC mode with a randomized Initialization Vector
        iv = uos.urandom(16)
        aes = ucryptolib.aes(self.key, 2, iv)
        return iv + aes.encrypt(self._pad(text.encode('utf-8')))


class TransmitterNode:
    def __init__(self):
        # Hardware Power Management: Activate Vext power rail for peripherals
        print("Activating Vext power rail")
        self.vext = machine.Pin(36, machine.Pin.OUT)
        self.vext.value(0) 
        time.sleep(0.8)

        print("Initializing subsystems")
        self.rtc = machine.RTC()
        self.crypto = CryptoEngine()
        
        # Distance thresholds for mail detection in mm
        self.dist_thresh_full = 65
        self.dist_thresh_empty = 75
        
        # Voltage thresholds for battery state hysteresis in V
        self.batt_thresh_low = 3.45
        self.batt_thresh_high = 3.60
        
        # Deep sleep duration in milliseconds
        self.sleep_ms = 3600000 
        
        # Analog-to-Digital Converter setup for battery monitoring
        self.adc_ctrl = machine.Pin(37, machine.Pin.OUT)
        self.adc_batt = machine.ADC(machine.Pin(1))
        self.adc_batt.atten(machine.ADC.ATTN_11DB)
        
        # Inter-Integrated Circuit bus and Time-of-Flight sensor initialization
        # Added internal pull-up resistors for signal stability
        pin_sda = machine.Pin(41, machine.Pin.IN, machine.Pin.PULL_UP)
        pin_scl = machine.Pin(42, machine.Pin.IN, machine.Pin.PULL_UP)
        self.i2c = machine.I2C(1, sda=pin_sda, scl=pin_scl)
        self.sensor = vl53l0x.VL53L0X(self.i2c)
        
        # LoRa radio module configuration for 868 MHz band
        self.lora = SX1262(spi_bus=1, clk=9, mosi=10, miso=11, cs=8, irq=14, rst=12, gpio=13)
        self.lora.begin(freq=868, bw=125.0, sf=9, cr=8, syncWord=0x12,
                        power=-5, currentLimit=60.0, preambleLength=8,
                        implicit=False, implicitLen=0xFF,
                        crcOn=True, txIq=False, rxIq=False,
                        tcxoVoltage=1.7, useRegulatorLDO=False, blocking=True)

    def _get_states(self):
        # Retrieve persistent mailbox and battery states from RTC memory
        mem = self.rtc.memory()
        if mem:
            try:
                parts = mem.decode('utf-8').split(',')
                return int(parts[0]), int(parts[1])
            except: 
                pass
        return 0, 0

    def _set_states(self, mail_st, batt_st):
        # Store current mailbox and battery states in RTC memory
        payload = "{},{}".format(mail_st, batt_st)
        self.rtc.memory(payload.encode('utf-8'))

    def _get_battery_level(self):
        # Activate ADC voltage divider, acquire reading, and deactivate to conserve power
        self.adc_ctrl.value(0)
        time.sleep(0.05)
        raw = self.adc_batt.read()
        self.adc_ctrl.value(1)
        # Convert raw 12-bit ADC value to voltage, accounting for hardware divider
        return (raw / 4095.0) * 3.3 * 4.9
        
    def run(self):
        # Main execution cycle sequence
        mail_st, batt_st = self._get_states()
        
        # 1. Battery state evaluation with hysteresis
        voltage = self._get_battery_level()
        print("Measured Voltage", round(voltage, 2), "V")
        
        if batt_st == 0 and voltage < self.batt_thresh_low:
            print("Alert Voltage critically low. Initiating transmission.")
            self.lora.send(self.crypto.encrypt("STATUS BATTERY LOW"))
            batt_st = 1
        elif batt_st == 1 and voltage > self.batt_thresh_high:
            print("Info Voltage recovered above upper threshold. Resetting state.")
            batt_st = 0
            
        # 2. Mailbox state evaluation with hysteresis
        distance = self.sensor.read()
        print("Measured Distance", distance, "mm")
        
        if mail_st == 0 and distance < self.dist_thresh_full:
            print("Alert Proximity threshold breached. Initiating transmission.")
            self.lora.send(self.crypto.encrypt("STATUS NEW MAIL"))
            mail_st = 1
        elif mail_st == 1 and distance > self.dist_thresh_empty:
            print("Info Proximity restored. Resetting state.")
            mail_st = 0
            
        self._set_states(mail_st, batt_st)
        
        # 3. System shutdown sequence and leakage current prevention
        print("Cycle complete. Preparing hardware for deep sleep.")
        
        # Disable I2C pull-ups to prevent phantom powering the unpowered sensor
        machine.Pin(41, machine.Pin.IN, None)
        machine.Pin(42, machine.Pin.IN, None)
        
        # Explicitly set LoRa radio to low power sleep mode
        if hasattr(self.lora, 'sleep'):
            self.lora.sleep()
            
        # Disable Vext power rail
        self.vext.value(1)
        
        print("Entering Deep Sleep")
        machine.deepsleep(self.sleep_ms)


if __name__ == "__main__":
    node = TransmitterNode()
    node.run()
