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

    # --- TEST 1: Read Word Register %MW1 (Target Setpoint) ---
    # Expected: 500
    # Header: CompanyID="LSIS-XGT\x00\x00", Info=0000, CPU=A0, Dir=33 (Req), InvokeID=0100 (1), Length=1300 (19)
    # Body: Slot=00, Station=00, Checksum=00, Reserved=00, Cmd=5400 (Read), DataType=0200 (Word), Reserved=0000, Blocks=0100, VarLen=0500, VarName="%MW1"
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x01\x00\x13\x00"
    body = b"\x00\x00\x00\x00\x54\x00\x02\x00\x00\x00\x01\x00\x05\x00%MW1"
    req = header + body
    
    print_hex("\n[Test 1] Read Word Register %MW1 Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 1] Response Received", resp)
    
    if len(resp) >= 30:
        company_id = resp[0:8]
        invoke_id = resp[14] | (resp[15] << 8)
        resp_cmd = resp[22] | (resp[23] << 8)
        status = resp[30] | (resp[31] << 8)
        
        # Data starts after Status (2), VarLen (2), VarName ("%MW1" -> 5), DataLen (2)
        var_len = resp[32] | (resp[33] << 8)
        data_len_offset = 34 + var_len
        data_len = resp[data_len_offset] | (resp[data_len_offset+1] << 8)
        val = resp[data_len_offset+2] | (resp[data_len_offset+3] << 8)
        
        print(f"  -> Company ID: {company_id.decode('ascii', errors='ignore')}")
        print(f"  -> Invoke ID: {invoke_id} (Expected: 1)")
        print(f"  -> Response Cmd: {resp_cmd:04X} (Expected: 0055)")
        print(f"  -> Status/Error: {status:04X} (Expected: 0000)")
        print(f"  -> Variable Name: {resp[34:34+var_len].decode('ascii')}")
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

    # --- TEST 2: Write Word Register %MW1 (Target Setpoint) to 750 ---
    # Header: Length=1700 (23 bytes body)
    # Body: Slot=00, Station=00, Checksum=00, Reserved=00, Cmd=5800 (Write), DataType=0200 (Word), Reserved=0000, Blocks=0100, VarLen=0500, VarName="%MW1", WriteDataLen=0200, WriteData=EE02 (750)
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x02\x00\x17\x00"
    body = b"\x00\x00\x00\x00\x58\x00\x02\x00\x00\x00\x01\x00\x05\x00%MW1\x02\x00\xEE\x02"
    req = header + body
    
    print_hex("\n[Test 2] Write Word Register %MW1 to 750 Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 2] Response Received", resp)
    
    if len(resp) >= 30:
        invoke_id = resp[14] | (resp[15] << 8)
        resp_cmd = resp[22] | (resp[23] << 8)
        status = resp[30] | (resp[31] << 8)
        
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

    # Wait briefly for synchronization
    time.sleep(0.05)

    # --- TEST 3: Read Word Register %MW1 Again to Verify ---
    # Expected: 750
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x03\x00\x13\x00"
    body = b"\x00\x00\x00\x00\x54\x00\x02\x00\x00\x00\x01\x00\x05\x00%MW1"
    req = header + body
    
    print_hex("\n[Test 3] Read Word Register %MW1 Again Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 3] Response Received", resp)
    
    if len(resp) >= 30:
        var_len = resp[32] | (resp[33] << 8)
        data_len_offset = 34 + var_len
        val = resp[data_len_offset+2] | (resp[data_len_offset+3] << 8)
        print(f"  -> Value Read Back: {val} (Expected: 750)")
        assert val == 750, f"Expected 750 but read back {val}"
        print("  [SUCCESS] Test 3 Passed.")
    else:
        print("  [FAILURE] Short response for Test 3.")
        sys.exit(1)

    # --- TEST 4: Read Bit Input Register %IX0.0.0 (Auto Mode) ---
    # Header: Length=1600 (22 body)
    # Body: Slot=00, Station=00, Checksum=00, Reserved=00, Cmd=5400 (Read), DataType=0000 (Bit), Reserved=0000, Blocks=0100, VarLen=0800, VarName="%IX0.0.0"
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x04\x00\x16\x00"
    body = b"\x00\x00\x00\x00\x54\x00\x00\x00\x00\x00\x01\x00\x08\x00%IX0.0.0"
    req = header + body
    
    print_hex("\n[Test 4] Read Bit Input %IX0.0.0 Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 4] Response Received", resp)
    
    if len(resp) >= 30:
        var_len = resp[32] | (resp[33] << 8)
        data_len_offset = 34 + var_len
        data_len = resp[data_len_offset] | (resp[data_len_offset+1] << 8)
        val = resp[data_len_offset+2]
        
        print(f"  -> Data Length: {data_len} (Expected: 1)")
        print(f"  -> Bit Value Read: {val} (0 or 1)")
        assert data_len == 1, "Bit read data length should be 1 byte"
        assert val in (0, 1), "Value should be 0 or 1"
        print("  [SUCCESS] Test 4 Passed.")
    else:
        print("  [FAILURE] Short response for Test 4.")
        sys.exit(1)

    # --- TEST 5: Write Bit Coil %QX0.0 (Inlet Valve Actuator) to True ---
    # Header: Length=1800 (24 body)
    # Body: Slot=00, Station=00, Checksum=00, Reserved=00, Cmd=5800 (Write), DataType=0000 (Bit), Reserved=0000, Blocks=0100, VarLen=0600, VarName="%QX0.0", WriteDataLen=0100, WriteData=01
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x05\x00\x18\x00"
    body = b"\x00\x00\x00\x00\x58\x00\x00\x00\x00\x00\x01\x00\x06\x00%QX0.0\x01\x00\x01"
    req = header + body
    
    print_hex("\n[Test 5] Write Bit Coil %QX0.0 to True Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 5] Response Received", resp)
    
    if len(resp) >= 30:
        status = resp[30] | (resp[31] << 8)
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
