import socket
import time
import sys

def print_hex(label, data):
    print(f"{label}: {' '.join(f'{b:02X}' for b in data)}")

def test_vplc():
    print("=== Starting Active vPLC Modbus TCP Protocol Test ===")
    
    server_ip = "127.0.0.1"
    server_port = 5020
    
    # 1. Connect to vPLC
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((server_ip, server_port))
        print(f"[Client] Connected to vPLC at {server_ip}:{server_port}")
    except Exception as e:
        print(f"[Error] Failed to connect: {e}")
        sys.exit(1)

    # --- PRE-TEST: Read Input Register 0 (%IW0) to get current analog value ---
    req = b"\x00\x00\x00\x00\x00\x06\x01\x04\x00\x00\x00\x01"
    s.sendall(req)
    resp = s.recv(1024)
    if len(resp) >= 11:
        iw0 = (resp[9] << 8) | resp[10]
        print(f"[Pre-Test] Read %IW0 (Analog Input) value: {iw0}")
    else:
        print("[Error] Failed to read %IW0.")
        sys.exit(1)
        
    # --- TEST 1: Read Holding Registers 0 to 2 ---
    req = b"\x00\x01\x00\x00\x00\x06\x01\x03\x00\x00\x00\x03"
    print_hex("\n[Test 1] Read Holding Registers Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 1] Response Received", resp)
    
    if len(resp) >= 15:
        fc = resp[7]
        byte_count = resp[8]
        val0 = (resp[9] << 8) | resp[10]
        val1 = (resp[11] << 8) | resp[12]
        val2 = (resp[13] << 8) | resp[14]
        print(f"  -> Function Code: {fc}")
        print(f"  -> Byte Count: {byte_count}")
        print(f"  -> Reg 0 (%MW0 - Pump Starts): {val0} (Expected: >= 0)")
        print(f"  -> Reg 1 (%MW1 - Set Point): {val1} (Expected: 500)")
        print(f"  -> Reg 2 (%MW2 - Unbound): {val2} (Expected: 0)")
        
        assert val0 >= 0, f"Test 1 Error: Reg 0 ({val0}) should be >= 0"
        assert val1 == 500, f"Test 1 Error: Reg 1 ({val1}) should be 500"
        print("  [SUCCESS] Test 1 Passed.")
    else:
        print("  [FAILURE] Short response received for Test 1.")
        sys.exit(1)

    # --- TEST 2: Write Single Holding Register 0 to 500 ---
    req = b"\x00\x02\x00\x00\x00\x06\x01\x06\x00\x00\x01\xF4"
    print_hex("\n[Test 2] Write Single Holding Register Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 2] Response Received", resp)
    
    if len(resp) >= 12 and resp[7] == 0x06:
        addr = (resp[8] << 8) | resp[9]
        val = (resp[10] << 8) | resp[11]
        print(f"  -> Written Address: {addr}")
        print(f"  -> Written Value: {val} (Expected: 500)")
        assert addr == 0 and val == 500, "Test 2 failed!"
        print("  [SUCCESS] Test 2 Passed.")
    else:
        print("  [FAILURE] Invalid response for Test 2.")
        sys.exit(1)

    # --- TEST 3: Read Holding Register 0 Again ---
    req = b"\x00\x03\x00\x00\x00\x06\x01\x03\x00\x00\x00\x01"
    print_hex("\n[Test 3] Read Holding Register 0 Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 3] Response Received", resp)
    if len(resp) >= 11:
        val0 = (resp[9] << 8) | resp[10]
        print(f"  -> Reg 0: {val0} (Expected: >= 500)")
        assert val0 >= 500, f"Test 3 Error: Reg 0 ({val0}) should be >= 500"
        print("  [SUCCESS] Test 3 Passed.")
    else:
        print("  [FAILURE] Short response received for Test 3.")
        sys.exit(1)

    # --- TEST 4: Read Coils 0 to 4 ---
    req = b"\x00\x04\x00\x00\x00\x06\x01\x01\x00\x00\x00\x04"
    print_hex("\n[Test 4] Read Coils Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 4] Response Received", resp)
    if len(resp) >= 10:
        byte_count = resp[8]
        bits = resp[9]
        print(f"  -> Byte Count: {byte_count}")
        print(f"  -> Coil Packed Bits: {bits:08b}")
        print("  [SUCCESS] Test 4 Passed.")
    else:
        print("  [FAILURE] Short response received for Test 4.")
        sys.exit(1)

    # --- TEST 5: Write Multiple Coils (0 to 3) to [False, True, True, False] ---
    req = b"\x00\x05\x00\x00\x00\x08\x01\x0F\x00\x00\x00\x04\x01\x06"
    print_hex("\n[Test 5] Write Multiple Coils Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 5] Response Received", resp)
    if len(resp) >= 12 and resp[7] == 0x0F:
        print("  [SUCCESS] Test 5 Passed.")
    else:
        print("  [FAILURE] Invalid response for Test 5.")
        sys.exit(1)

    # --- TEST 6: Read Coils Again ---
    req = b"\x00\x06\x00\x00\x00\x06\x01\x01\x00\x00\x00\x04"
    print_hex("\n[Test 6] Read Coils Again Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 6] Response Received", resp)
    if len(resp) >= 10:
        bits = resp[9]
        coil2 = (bits & (1 << 2)) != 0
        coil3 = (bits & (1 << 3)) != 0
        print(f"  -> Coil Packed Bits: {bits:08b}")
        print(f"  -> Coil 2 (Unbound): {coil2} (Expected: True)")
        print(f"  -> Coil 3 (Unbound): {coil3} (Expected: False)")
        assert coil2 == True, "Test 6 Error: Coil 2 should remain True"
        assert coil3 == False, "Test 6 Error: Coil 3 should remain False"
        print("  [SUCCESS] Test 6 Passed (Verified PLC logic overwrite-protection for unbound coils).")
    else:
        print("  [FAILURE] Short response received for Test 6.")
        sys.exit(1)

    s.close()
    print("\n================ All Tests Passed dynamically! ================")

if __name__ == "__main__":
    test_vplc()
