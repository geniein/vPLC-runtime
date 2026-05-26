import socket
import time
import sys

def print_hex(label, data):
    print(f"{label}: {' '.join(f'{b:02X}' for b in data)}")

def test_mc_server():
    print("=== Starting Active vPLC Mitsubishi MC Protocol Test ===")
    
    server_ip = "127.0.0.1"
    server_port = 5021
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((server_ip, server_port))
        print(f"[Client] Connected to vPLC MC Server at {server_ip}:{server_port}")
    except Exception as e:
        print(f"[Error] Failed to connect: {e}")
        sys.exit(1)

    # --- TEST 1: Batch Read D Registers ---
    req = b"\x50\x00\x00\xFF\xFF\x03\x00\x0C\x00\x10\x00\x01\x04\x00\x00\x00\x00\x00\xA8\x03\x00"
    print_hex("\n[Test 1] Batch Read D Registers Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 1] Response Received", resp)
    
    if len(resp) >= 17:
        subheader = resp[0:2]
        length = resp[7] | (resp[8] << 8)
        end_code = resp[9] | (resp[10] << 8)
        
        val0 = resp[11] | (resp[12] << 8) # D0 (%MW0 - Pump Starts)
        val1 = resp[13] | (resp[14] << 8) # D1 (%MW1 - Setpoint)
        val2 = resp[15] | (resp[16] << 8) # D2 (%MW2 - Unbound)
        
        print(f"  -> Subheader: {subheader[0]:02X}{subheader[1]:02X} (Expected: D000)")
        print(f"  -> Response Length: {length} (Expected: 8)")
        print(f"  -> End Code: {end_code:04X} (Expected: 0000)")
        print(f"  -> D0 (%MW0 - Pump Starts): {val0} (Expected: >= 0)")
        print(f"  -> D1 (%MW1 - Target Setpoint): {val1} (Expected: 500)")
        print(f"  -> D2 (%MW2 - Unbound): {val2} (Expected: 0)")
        
        assert subheader == b"\xD0\x00", "Test 1 failed: Invalid subheader"
        assert end_code == 0, f"Test 1 failed: End code {end_code:04X}"
        assert val0 >= 0, "Test 1 failed: val0 should be >= 0"
        assert val1 == 500, f"Test 1 failed: val1 should be 500, got {val1}"
        assert val2 == 0, f"Test 1 failed: val2 should be 0, got {val2}"
        print("  [SUCCESS] Test 1 Passed.")
    else:
        print("  [FAILURE] Short response received for Test 1.")
        sys.exit(1)

    # --- TEST 2: Batch Write D1 to 750 ---
    req = b"\x50\x00\x00\xFF\xFF\x03\x00\x0E\x00\x10\x00\x01\x14\x00\x00\x01\x00\x00\xA8\x01\x00\xEE\x02"
    print_hex("\n[Test 2] Batch Write D1 Request (D1 = 750)", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 2] Response Received", resp)
    
    if len(resp) >= 11:
        subheader = resp[0:2]
        end_code = resp[9] | (resp[10] << 8)
        print(f"  -> End Code: {end_code:04X} (Expected: 0000)")
        assert subheader == b"\xD0\x00"
        assert end_code == 0, f"Test 2 failed with end code {end_code:04X}"
        print("  [SUCCESS] Test 2 Passed.")
    else:
        print("  [FAILURE] Short response for Test 2.")
        sys.exit(1)

    time.sleep(0.05)

    # --- TEST 3: Batch Read D1 Again to Verify Write ---
    req = b"\x50\x00\x00\xFF\xFF\x03\x00\x0C\x00\x10\x00\x01\x04\x00\x00\x01\x00\x00\xA8\x01\x00"
    print_hex("\n[Test 3] Batch Read D1 Again Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 3] Response Received", resp)
    
    if len(resp) >= 13:
        end_code = resp[9] | (resp[10] << 8)
        val = resp[11] | (resp[12] << 8)
        print(f"  -> D1 read back: {val} (Expected: 750)")
        assert end_code == 0
        assert val == 750, f"Test 3 failed: expected 750, got {val}"
        print("  [SUCCESS] Test 3 Passed.")
    else:
        print("  [FAILURE] Short response for Test 3.")
        sys.exit(1)

    # --- TEST 4: Batch Read Bit Devices X ---
    req = b"\x50\x00\x00\xFF\xFF\x03\x00\x0C\x00\x10\x00\x01\x04\x01\x00\x00\x00\x00\x9C\x03\x00"
    print_hex("\n[Test 4] Batch Read Bit X Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 4] Response Received", resp)
    
    if len(resp) >= 12:
        end_code = resp[9] | (resp[10] << 8)
        bit_byte = resp[11]
        print(f"  -> End Code: {end_code:04X} (Expected: 0000)")
        print(f"  -> Packed Bit Byte: {bit_byte:08b}")
        assert end_code == 0
        print("  [SUCCESS] Test 4 Passed.")
    else:
        print("  [FAILURE] Short response for Test 4.")
        sys.exit(1)

    # --- TEST 5: Batch Write Bit Devices Y ---
    req = b"\x50\x00\x00\xFF\xFF\x03\x00\x0D\x00\x10\x00\x01\x14\x01\x00\x00\x00\x00\x9D\x02\x00\x10"
    print_hex("\n[Test 5] Batch Write Bit Y Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 5] Response Received", resp)
    
    if len(resp) >= 11:
        end_code = resp[9] | (resp[10] << 8)
        print(f"  -> End Code: {end_code:04X} (Expected: 0000)")
        assert end_code == 0
        print("  [SUCCESS] Test 5 Passed.")
    else:
        print("  [FAILURE] Short response for Test 5.")
        sys.exit(1)

    s.close()
    print("\n================ All MELSEC MC Server Tests Passed! ================")

if __name__ == "__main__":
    test_mc_server()
