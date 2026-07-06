import socket
import ssl
import ubinascii
import time
import gc

class SmtpClient:
    """ SMTP client utilizing explicit garbage collection and deterministic socket closure. """
    
    def __init__(self, usr, pwd):
        self.host = "smtp.gmail.com"
        self.port = 465
        self.usr = usr
        self.pwd = pwd

    def dispatch(self, targets, subject, body, log_cb=None):
        """ Executes SMTP sequence with aggressive RAM defragmentation extended retries and custom payload. """
        def log_event(console_text, oled_text):
            if log_cb:
                log_cb(console_text, oled_text)
            else:
                print(console_text)

        if not targets:
            log_event("Abort No valid targets", "Err No Target")
            return
            
        max_retries = 5
        
        for attempt in range(max_retries):
            log_event("SMTP attempt " + str(attempt + 1), "SMTP Try " + str(attempt + 1))
            gc.collect()
            
            sock = None
            tls = None
            
            try:
                addr = socket.getaddrinfo(self.host, self.port)[0][-1]
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(15.0)
                
                sock.connect(addr)
                tls = ssl.wrap_socket(sock, server_hostname=self.host)
                
                def read_resp():
                    """ Reads response and evaluates SMTP status codes. """
                    last_line = ""
                    while True:
                        reply = tls.readline()
                        if not reply:
                            raise Exception("Connection closed unexpectedly")
                        last_line = reply.decode('utf-8').strip()
                        if len(reply) >= 4 and reply[3] == 32:
                            break
                    if len(last_line) >= 3 and last_line[0] in ['4', '5']:
                        raise Exception("SMTP Protocol Denial " + last_line)

                def send_cmd(cmd):
                    """ Transmits command and awaits server acknowledgment. """
                    tls.write(cmd + b"\r\n")
                    read_resp()

                read_resp()
                send_cmd(b"EHLO esp32")
                send_cmd(b"AUTH LOGIN")
                send_cmd(ubinascii.b2a_base64(self.usr.encode()).strip())
                send_cmd(ubinascii.b2a_base64(self.pwd.encode()).strip())
                send_cmd(b"MAIL FROM:<" + self.usr.encode() + b">")
                
                for r in targets: 
                    send_cmd(b"RCPT TO:<" + r.encode() + b">")
                    
                send_cmd(b"DATA")
                msg = (f"From: {self.usr}\r\nTo: {', '.join(targets)}\r\n"
                       f"Subject: {subject}\r\n\r\n"
                       f"Moin zusammen,\r\n"
                       f"\r\n"
                       f"{body}\r\n"
                       f"Liebe Grüße,\r\n"
                       f"Jonas\r\n.\r\n")
                tls.write(msg.encode('utf-8'))
                
                read_resp()
                send_cmd(b"QUIT")
                log_event("Dispatched successfully", "Mail Sent OK")
                return
                
            except Exception as e:
                log_event("SMTP Error " + str(e), "SMTP Err " + str(attempt + 1))
                if attempt < max_retries - 1:
                    time.sleep(5)
                    
            finally:
                if tls is not None:
                    try:
                        tls.close()
                    except Exception:
                        pass
                if sock is not None:
                    try:
                        sock.close()
                    except Exception:
                        pass
                        
        log_event("Critical Error SMTP dispatch failed", "Mail Failed")