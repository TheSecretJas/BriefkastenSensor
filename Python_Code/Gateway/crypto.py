import ucryptolib

class CryptoUnit:
    """ Handles AES decryption using the injected key. """
    def __init__(self, key: bytes):
        self.key = key

    def decrypt(self, data: bytes) -> str:
        """ Decrypts payload separating IV and ciphertext. """
        iv = data[:16]
        cipher = data[16:]
        aes = ucryptolib.aes(self.key, 2, iv)
        plain = aes.decrypt(cipher)
        return plain[:-plain[-1]].decode('utf-8')