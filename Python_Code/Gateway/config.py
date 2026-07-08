import esp32

class ConfigLoader:
    """ Retrieves stored credentials and config data from NVS. """
    def __init__(self):
        self.nvs = esp32.NVS("sys_sec")
        buf_aes = bytearray(16)
        self.nvs.get_blob("aes_main", buf_aes)
        self.aes_key = bytes(buf_aes)
        self.smtp_usr = self.read_str("smtp_user")
        self.smtp_pwd = self.read_str("smtp_pass")
        
        raw_recipients = self.read_str("recipients")
        if raw_recipients:
            self.recipients = raw_recipients.split(",")
        else:
            self.recipients = []
            print("Warning No recipients found in NVS")

    def read_str(self, key: str) -> str:
        """ Reads a binary blob and decodes it to a string. """
        buf = bytearray(128)
        try:
            length = self.nvs.get_blob(key, buf)
            return buf[:length].decode('utf-8')
        except OSError:
            print("Error Configuration key missing")
            return ""