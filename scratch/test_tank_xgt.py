import socket
import time
import sys

def print_hex(label, data):
    print(f"{label}: {' '.join(f'{b:02X}' for b in data)}")

def test_xgt_server():
    print("=== Starting Active vPLC LS Electric XGT Protocol Test ===")
    
    server_ip = "127.0.0.1"
    server_port = 2004
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((server_ip, server_port))
        print(f"[Client] Connected to vPLC XGT Server at {server_ip}:{server_port}")
    except Exception as e:
        print(f"[Error] Failed to connect: {e}")
        sys.exit(1)

    # --- TEST 1: Read Word Register %MW1 ---
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x01\x00\x10\x00"
    body = b"\x00\x00\x54\x00\x02\x00\x00\x00\x01\x00\x04\x00%MW1"
    req = header + body
    
    print_hex("\n[Test 1] Read Word Register %MW1 Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 1] Response Received", resp)
    
    if len(resp) >= 30:
        company_id = resp[0:8]
        invoke_id = resp[14] | (resp[15] << 8)
        resp_cmd = resp[20] | (resp[21] << 8)
        status = resp[26] | (resp[27] << 8)
        
        data_len = resp[30] | (resp[31] << 8)
        val = resp[32] | (resp[33] << 8)
        
        print(f"  -> Company ID: {company_id.decode('ascii', errors='ignore')}")
        print(f"  -> Invoke ID: {invoke_id} (Expected: 1)")
        print(f"  -> Response Cmd: {resp_cmd:04X} (Expected: 0055)")
        print(f"  -> Status/Error: {status:04X} (Expected: 0000)")
        print(f"  -> Data Length: {data_len} (Expected: 2)")
        print(f"  -> Value Read: {val} (Expected: 500)")
        
        assert company_id == b"LSIS-XGT", "Invalid Company ID"
        assert invoke_id == 1, "Invalid Invoke ID"
        assert resp_cmd == 0x0055, "Invalid Response Command"
        assert status == 0, f"Error status returned: {status:04X}"
        assert data_len == 2, "Data length should be 2 bytes (Word)"
        assert val == 500, f"Value mismatch, expected 500 but got {val}"
        print("  [SUCCESS] Test 1 Passed.")
    else:
        print("  [FAILURE] Short response received for Test 1.")
        sys.exit(1)

    # --- TEST 2: Write Word Register %MW1 to 750 ---
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x02\x00\x14\x00"
    body = b"\x00\x00\x58\x00\x02\x00\x00\x00\x01\x00\x04\x00%MW1\x02\x00\xEE\x02"
    req = header + body
    
    print_hex("\n[Test 2] Write Word Register %MW1 to 750 Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 2] Response Received", resp)
    
    if len(resp) >= 30:
        invoke_id = resp[14] | (resp[15] << 8)
        resp_cmd = resp[20] | (resp[21] << 8)
        status = resp[26] | (resp[27] << 8)
        
        print(f"  -> Invoke ID: {invoke_id} (Expected: 2)")
        print(f"  -> Response Cmd: {resp_cmd:04X} (Expected: 0059)")
        print(f"  -> Status/Error: {status:04X} (Expected: 0000)")
        
        assert invoke_id == 2
        assert resp_cmd == 0x0059
        assert status == 0
        print("  [SUCCESS] Test 2 Passed.")
    else:
        print("  [FAILURE] Short response received for Test 2.")
        sys.exit(1)

    time.sleep(0.05)

    # --- TEST 3: Read Word Register %MW1 Again to Verify ---
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x03\x00\x10\x00"
    body = b"\x00\x00\x54\x00\x02\x00\x00\x00\x01\x00\x04\x00%MW1"
    req = header + body
    
    print_hex("\n[Test 3] Read Word Register %MW1 Again Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 3] Response Received", resp)
    
    if len(resp) >= 34:
        val = resp[32] | (resp[33] << 8)
        print(f"  -> Value Read Back: {val} (Expected: 750)")
        assert val == 750, f"Expected 750 but read back {val}"
        print("  [SUCCESS] Test 3 Passed.")
    else:
        print("  [FAILURE] Short response for Test 3.")
        sys.exit(1)

    # --- TEST 4: Read Bit Input Register %IX0.0.0 (Auto Mode) ---
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x04\x00\x14\x00"
    body = b"\x00\x00\x54\x00\x00\x00\x00\x00\x01\x00\x08\x00%IX0.0.0"
    req = header + body
    
    print_hex("\n[Test 4] Read Bit Input %IX0.0.0 Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 4] Response Received", resp)
    
    if len(resp) >= 33:
        data_len = resp[30] | (resp[31] << 8)
        val = resp[32]
        
        print(f"  -> Data Length: {data_len} (Expected: 1)")
        print(f"  -> Bit Value Read: {val} (0 or 1)")
        assert data_len == 1, "Bit read data length should be 1 byte"
        assert val in (0, 1), "Value should be 0 or 1"
        print("  [SUCCESS] Test 4 Passed.")
    else:
        print("  [FAILURE] Short response for Test 4.")
        sys.exit(1)

    # --- TEST 5: Write Bit Coil %QX0.0 to True ---
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x05\x00\x15\x00"
    body = b"\x00\x00\x58\x00\x00\x00\x00\x00\x01\x00\x06\x00%QX0.0\x01\x00\x01"
    req = header + body
    
    print_hex("\n[Test 5] Write Bit Coil %QX0.0 to True Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 5] Response Received", resp)
    
    if len(resp) >= 30:
        status = resp[26] | (resp[27] << 8)
        print(f"  -> Status/Error: {status:04X} (Expected: 0000)")
        assert status == 0
        print("  [SUCCESS] Test 5 Passed.")
    else:
        print("  [FAILURE] Short response for Test 5.")
        sys.exit(1)

    s.close()
    print("\n================ All LS Electric XGT Server Tests Passed! ================")

if __name__ == "__main__":
    test_xgt_server()
