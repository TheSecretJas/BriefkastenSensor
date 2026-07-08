# Phase 1 Early Network Boot
import gc
import time
import machine
import network
import esp32

def init_wlan():
    """ Allocates DMA memory for WiFi with retry mechanism. """
    nvs = esp32.NVS("sys_sec")
    buf = bytearray(64)
    try:
        l_ssid = nvs.get_blob("wifi_ssid", buf)
        ssid = buf[:l_ssid].decode('utf-8')
        l_pwd = nvs.get_blob("wifi_pass", buf)
        pwd = buf[:l_pwd].decode('utf-8')
    except OSError:
        print("Error Network credentials missing")
        return

    sta = network.WLAN(network.STA_IF)
    max_retries = 3
    
    for attempt in range(max_retries):
        print("WiFi connection attempt", attempt + 1)
        sta.active(False)
        time.sleep(0.5)
        sta.active(True)
        sta.connect(ssid, pwd)
        
        timeout = 20
        while not sta.isconnected() and timeout > 0:
            time.sleep(0.5)
            timeout -= 1
            
        if sta.isconnected():
            print("WiFi connected successfully")
            return
            
        print("Connection failed retrying")
        
    print("Error Critical WiFi failure")

init_wlan()
gc.collect()

# Phase 2 Peripheral Initialization
import ssd1306
import bme280
from sx1262 import SX1262

# Import custom modules
from config import ConfigLoader
from crypto import CryptoUnit
from smtp import SmtpClient

class GatewayDev:
    """ Manages OLED BME280 sensor LoRa RX and SMTP coordination. """
    def __init__(self):
        self.cfg = ConfigLoader()
        self.crypto = CryptoUnit(self.cfg.aes_key)
        self.mailer = SmtpClient(self.cfg.smtp_usr, self.cfg.smtp_pwd)
        self.sys_status = "RX Ready"
        self.status_until = 0
        
        vext = machine.Pin(36, machine.Pin.OUT)
        vext.value(0)
        time.sleep(0.1)

        rst = machine.Pin(21, machine.Pin.OUT)
        rst.value(0)
        time.sleep(0.05)
        rst.value(1)

        self.i2c_oled = machine.I2C(0, sda=machine.Pin(17), scl=machine.Pin(18))
        self.disp = ssd1306.SSD1306_I2C(128, 64, self.i2c_oled)
        
        self.i2c_ext = machine.I2C(1, sda=machine.Pin(41), scl=machine.Pin(42))
        self.bme = bme280.BME280(i2c=self.i2c_ext, address=0x77)
        
        print("Forcing LoRa hardware reset")
        lora_rst = machine.Pin(12, machine.Pin.OUT)
        lora_rst.value(0)
        time.sleep(0.05)
        lora_rst.value(1)
        time.sleep(0.05)
        
        self.lora = SX1262(spi_bus=1, clk=9, mosi=10, miso=11, cs=8, irq=14, rst=12, gpio=13)
        self.lora.begin(freq=868, bw=125.0, sf=9, cr=8, syncWord=0x12,
                        power=-5, currentLimit=60.0, preambleLength=8,
                        implicit=False, implicitLen=0xFF,
                        crcOn=True, txIq=False, rxIq=False,
                        tcxoVoltage=1.7, useRegulatorLDO=False, blocking=False)
        
        self.msg_flag = False
        self.lora.setBlockingCallback(False, self.lora_isr)
        
        self.alert_act = False
        self.alert_time = 0
        self.upd_screen()

        self.wdt = machine.WDT(timeout=15000)

    def lora_isr(self, events):
        """ Hardware interrupt routine triggered by DIO1 pin. """
        if events & SX1262.RX_DONE:
            self.msg_flag = True

    def log_ui(self, console_text: str, oled_text: str = "", duration: int = 2):
        """ Logs verbose string to console and short string to OLED display. """
        print(console_text)
        self.sys_status = oled_text if oled_text else console_text[:16]
        self.status_until = time.time() + duration
        self.upd_screen()

    def upd_screen(self):
        """ Reads environmental data array and refreshes OLED without scaling. """
        try:
            data = self.bme.read_compensated_data()
            temp_c = data[0]
            hum_p = data[2]
            
            self.disp.fill(0)
            if self.alert_act:
                self.disp.text("NEW MAIL ARRIVED", 0, 0)
            else:
                self.disp.text("ROOM CLIMATE", 0, 0)
                
            self.disp.text("Temp {} C".format(round(temp_c, 1)), 0, 20)
            self.disp.text("Hum  {} %".format(round(hum_p, 1)), 0, 35)
            self.disp.text(self.sys_status, 0, 55)
            self.disp.show()
        except Exception as err:
            print("OLED or BME Error", err)

    def ensure_network(self):
        """ Validates link state and triggers a hard reset if disconnected to prevent heap fragmentation. """
        sta = network.WLAN(network.STA_IF)
        if not sta.isconnected():
            self.log_ui("WLAN lost Rebooting system", "WLAN Error")
            time.sleep(2)
            machine.reset()

    def run(self):
        """ Main non blocking loop incorporating continuous WDT feeding and fault recovery. """
        self.log_ui("Gateway operational", "RX Ready")
        last_upd = 0
        
        while True:
            try:
                now = time.time()
                self.wdt.feed()
                
                if self.sys_status != "RX Ready" and now > self.status_until:
                    self.sys_status = "RX Ready"
                    self.upd_screen()

                if now - last_upd > 5:
                    if self.alert_act and (now - self.alert_time > 60): 
                        self.alert_act = False
                    self.upd_screen()
                    last_upd = now
                    gc.collect()
                    
                if self.msg_flag:
                    self.msg_flag = False
                    payload, err = self.lora.recv()
                    if not err:
                        try:
                            dec = self.crypto.decrypt(bytes(payload))
                            
                            if dec == "STATUS NEW MAIL":
                                self.alert_act = True
                                self.alert_time = now
                                self.log_ui("New Mail Alert", "Valid Alert")
                                self.ensure_network()
                                self.mailer.dispatch(self.cfg.recipients, "Posteinwurf", "Die Sensorik meldet einen Einwurf, neue Post liegt zur Abholung bereit.", self.log_ui)
                                self.log_ui("Sequence completed returning to idle", "RX Ready")
                                
                            elif dec == "STATUS BATTERY LOW":
                                self.log_ui("Battery Warning", "Batt Alert")
                                self.ensure_network()
                                self.mailer.dispatch(self.cfg.recipients, "Akku kritisch", "Die Akkuspannung am Sendemodul ist unter die kritische Schwelle gefallen. Bitte aufladen!", self.log_ui)
                                self.log_ui("Sequence completed returning to idle", "RX Ready")
                                
                        except Exception as decode_err:
                            print("Crypto or Payload Error", decode_err)
                            
            except Exception as loop_err:
                print("Critical Loop Fault Recovering", loop_err)
                time.sleep(2)
                
            time.sleep(0.1)

if __name__ == "__main__":
    node = GatewayDev()
    node.run()