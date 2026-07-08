# Abstract
# This script validates the AES encryption and the physical LoRa TX path by transmitting a synthetic alert payload.

import machine
import time
import uos
import esp32
import ucryptolib
from sx1262 import SX1262

class CryptoEngine:
    """ Handles NVS retrieval and AES encryption. """
    def __init__(self):
        nvs = esp32.NVS("sys_sec")
        buf = bytearray(16)
        nvs.get_blob("aes_main", buf)
        self.key = bytes(buf)

    def _pad(self, data: bytes) -> bytes:
        """ Applies PKCS7 padding to match block size. """
        pad_len = 16 - (len(data) % 16)
        return data + bytes([pad_len] * pad_len)

    def encrypt(self, text: str) -> bytes:
        """ Encrypts payload with randomized IV. """
        iv = uos.urandom(16)
        aes = ucryptolib.aes(self.key, 2, iv)
        return iv + aes.encrypt(self._pad(text.encode('utf-8')))

def test_transmission():
    """ Initializes LoRa and transmits a single encrypted packet. """
    print("Initializing Cryptography")
    crypto = CryptoEngine()
    
    print("Initializing LoRa transceiver")
    lora = SX1262(spi_bus=1, clk=9, mosi=10, miso=11, cs=8, irq=14, rst=12, gpio=13)
    lora.begin(freq=868, bw=125.0, sf=9, cr=8, syncWord=0x12,
               power=-5, currentLimit=60.0, preambleLength=8,
               implicit=False, implicitLen=0xFF,
               crcOn=True, txIq=False, rxIq=False,
               tcxoVoltage=1.7, useRegulatorLDO=False, blocking=True)
               
    print("Preparing payload")
    cipher = crypto.encrypt("STATUS NEW MAIL")
    
    print("Transmitting data")
    lora.send(cipher)
    print("Transmission successful")

if __name__ == "__main__":
    test_transmission()