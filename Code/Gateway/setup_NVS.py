import esp32

def parse_env_file(file_path: str) -> dict:
    """ 
    Parses a simple .env file and returns a dictionary of key-value pairs. 
    Ignores empty lines and comments starting with '#'.
    """
    env_vars = {}
    try:
        with open(file_path, 'r') as file:
            for line in file:
                line = line.strip()
                # Ignore empty lines and comments
                if not line or line.startswith('#'):
                    continue
                # Split only on the first equals sign
                if '=' in line:
                    key, value = line.split('=', 1)
                    env_vars[key.strip()] = value.strip()
    except OSError:
        print("Error Environment file not found")
    
    return env_vars

class SecureConfig:
    """ Manages provisioning of credentials and keys into NVS. """
    def __init__(self):
        self.nvs = esp32.NVS("sys_sec")

    def store_bytes(self, key_id: str, data: bytes):
        """ Stores raw byte data. """
        self.nvs.set_blob(key_id, data)

    def store_string(self, key_id: str, text: str):
        """ Encodes string to bytes and stores it. """
        self.nvs.set_blob(key_id, text.encode('utf-8'))

    def apply(self):
        """ Commits all pending changes to the flash memory. """
        self.nvs.commit()
        print("Credentials saved to NVS successfully")

if __name__ == "__main__":
    config = SecureConfig()
    
    # Load configuration from .env file
    env_data = parse_env_file('.env')
    
    if env_data:
        print("Environment file loaded successfully")
        
        # Cryptographic Key
        if 'AES_MAIN' in env_data:
            # Convert the string from .env to bytes for AES encryption
            config.store_bytes("aes_main", env_data['AES_MAIN'].encode('utf-8'))
        
        # Network Credentials
        if 'WIFI_SSID' in env_data:
            config.store_string("wifi_ssid", env_data['WIFI_SSID'])
        if 'WIFI_PASS' in env_data:
            config.store_string("wifi_pass", env_data['WIFI_PASS'])
        
        # SMTP Credentials
        if 'SMTP_USER' in env_data:
            config.store_string("smtp_user", env_data['SMTP_USER'])
        if 'SMTP_PASS' in env_data:
            config.store_string("smtp_pass", env_data['SMTP_PASS'])
        if 'RECIPIENTS' in env_data:
            config.store_string("recipients", env_data['RECIPIENTS'])
        
        config.apply()
    else:
        print("Provisioning aborted due to missing or empty environment file")
