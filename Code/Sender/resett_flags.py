import machine

def reset_system_flags():
    """ Reads current RTC memory state and resets flags to zero baseline. """
    rtc = machine.RTC()
    
    print("Reading current RTC memory state")
    mem = rtc.memory()
    
    if mem:
        try:
            state_str = mem.decode('utf-8')
            parts = state_str.split(',')
            if len(parts) == 2:
                mail_st = int(parts[0])
                batt_st = int(parts[1])
                print("Current state Mailbox", mail_st, "Battery", batt_st)
            else:
                print("Found existing state but unexpected format", state_str)
        except Exception:
            print("Found unreadable binary data")
    else:
        print("RTC memory is currently empty")
        
    print("Applying system reset to all flags")
    clean_state = "0,0"
    rtc.memory(clean_state.encode('utf-8'))
    
    print("Verifying new state")
    new_mem = rtc.memory()
    print("Confirmed new state", new_mem.decode('utf-8'))
    print("System reset successful All alerts rearmed")

if __name__ == "__main__":
    reset_system_flags()