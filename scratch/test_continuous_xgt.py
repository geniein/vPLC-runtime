import socket
import time
import sys

def print_hex(label, data):
    print(f"{label}: {' '.join(f'{b:02X}' for b in data)}")

def run_tests():
    server_ip = "127.0.0.1"
    server_port = 2004
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((server_ip, server_port))
        print(f"[Client] Connected to vPLC XGT Server at {server_ip}:{server_port}")
    except Exception as e:
        print(f"[Error] Failed to connect to XGT Server: {e}")
        sys.exit(1)

    # =========================================================================
    # Test 1: Continuous Read %MW0, size 20 bytes (10 Words)
    # Variable "%MW0" has length 4.
    # Body has 18 bytes: 2 (BCC) + 2 (Cmd) + 2 (Type) + 2 (Reserved) + 2 (Blocks) + 2 (VarLen) + 4 (VarName) + 2 (ReadSize)
    # Header length field: 18 = 0x0012
    # =========================================================================
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x01\x00\x12\x00"
    body = b"\x00\x00\x54\x00\x14\x00\x00\x00\x01\x00\x04\x00%MW0\x14\x00"
    req = header + body
    print_hex("\n[Test 1] Continuous Read Request", req)
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("[Test 1] Response Received", resp)
    
    assert len(resp) >= 30, "Short response"
    status = resp[26] | (resp[27] << 8)
    assert status == 0, f"Error status: {status:04X}"
    data_size = resp[30] | (resp[31] << 8)
    print(f"  -> Data size returned: {data_size} bytes (Expected: 20)")
    assert data_size == 20
    
    # =========================================================================
    # Test 2: Continuous Write %MW0, size 20 bytes (10 Words) with values [10, 20, ..., 100]
    # Body has 38 bytes: 18 (as above) + 20 (write data)
    # Header length field: 38 = 0x0026
    # =========================================================================
    write_header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x02\x00\x26\x00"
    write_body = b"\x00\x00\x58\x00\x14\x00\x00\x00\x01\x00\x04\x00%MW0\x14\x00"
    for i in range(1, 11):
        val = i * 10
        write_body += bytes([val & 0xFF, (val >> 8) & 0xFF])
    
    write_req = write_header + write_body
    print_hex("\n[Test 2] Continuous Write Request", write_req)
    s.sendall(write_req)
    
    resp = s.recv(1024)
    print_hex("[Test 2] Response Received", resp)
    status = resp[26] | (resp[27] << 8)
    assert status == 0, f"Write error status: {status:04X}"
    print("  -> Continuous Write Success!")

    # =========================================================================
    # Test 3: Continuous Read %MW0 Again to Verify Values
    # =========================================================================
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x03\x00\x12\x00"
    body = b"\x00\x00\x54\x00\x14\x00\x00\x00\x01\x00\x04\x00%MW0\x14\x00"
    req = header + body
    s.sendall(req)
    
    resp = s.recv(1024)
    print_hex("\n[Test 3] Continuous Read Back", resp)
    data_size = resp[30] | (resp[31] << 8)
    assert data_size == 20
    
    print("  -> Values read back:")
    for i in range(10):
        val = resp[32 + i*2] | (resp[32 + i*2 + 1] << 8)
        print(f"     %MW{i}: {val} (Expected: {(i+1)*10})")
        assert val == (i+1)*10, f"Value mismatch at %MW{i}: {val}"
    print("  -> [SUCCESS] Continuous values verified!")

    # =========================================================================
    # Test 4: Individual Multi-Block Read of %MW0 and %MW1 (Block Count = 2)
    # "%MW0" (4 chars) and "%MW1" (4 chars)
    # Body has 22 bytes: 2 (BCC) + 2 (Cmd) + 2 (Type) + 2 (Reserved) + 2 (Blocks) + 
    #                   2 (VarLen1) + 4 (Var1) + 2 (VarLen2) + 4 (Var2)
    # Header length field: 22 = 0x0016
    # =========================================================================
    multi_header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x04\x00\x16\x00"
    multi_body = b"\x00\x00\x54\x00\x02\x00\x00\x00\x02\x00\x04\x00%MW0\x04\x00%MW1"
    multi_req = multi_header + multi_body
    print_hex("\n[Test 4] Multi-Block Individual Read Request", multi_req)
    s.sendall(multi_req)
    
    resp = s.recv(1024)
    print_hex("[Test 4] Response Received", resp)
    status = resp[26] | (resp[27] << 8)
    assert status == 0
    block_cnt = resp[28] | (resp[29] << 8)
    print(f"  -> Block Count: {block_cnt} (Expected: 2)")
    assert block_cnt == 2
    
    # Block 0: Size=2, Value=10
    sz0 = resp[30] | (resp[31] << 8)
    val0 = resp[32] | (resp[33] << 8)
    print(f"  -> Block 0 Size: {sz0}, Value: {val0} (Expected: 2, 10)")
    assert sz0 == 2
    assert val0 == 10
    
    # Block 1: Size=2, Value=20
    sz1 = resp[34] | (resp[35] << 8)
    val1 = resp[36] | (resp[37] << 8)
    print(f"  -> Block 1 Size: {sz1}, Value: {val1} (Expected: 2, 20)")
    assert sz1 == 2
    assert val1 == 20
    
    print("  -> [SUCCESS] Multi-Block Individual Read verified!")
    s.close()
    
    print("\n================ All LS Electric XGT Server Tests Passed! ================")

if __name__ == "__main__":
    run_tests()
