import sys
import socket
import time

def test_pyxgt():
    print("=== PyXGT Raw Byte Inspection Test ===")
    
    server_ip = "127.0.0.1"
    server_port = 2014
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((server_ip, server_port))
        print(f"[Client] Connected to LS XGT Server at {server_ip}:{server_port}")
    except Exception as e:
        print(f"[Error] Connection failed: {e}")
        sys.exit(1)

    # We manually construct PyXGT request for reading M1 (Target Setpoint)
    # Header: "LSIS-XGT\x00\x00\x00\x00\xA0\x33\x01\x00\x10\x00" (18 bytes)
    # len_header = bytes([len(app_instruction), 0])
    # app_instruction: Cmd (2) + Dtype (2) + Reserved (2) + Num (2) + VarLen (2) + VarName
    # VarName = "%MW1" -> len = 4.
    # app_instruction = "\x54\x00\x02\x00\x00\x00\x01\x00\x04\x00%MW1" (14 bytes)
    # total len_header = 14 -> "\x0E\x00"
    # fenet + reserved = "\x00\x00"
    # full packet = Header (18 bytes) + len_header (2) + fenet (1) + reserved (1) + app_instruction (14) = 36 bytes.
    # Wait, in LS.py:
    # Header (16 bytes) = "LSIS-XGT" (8) + "\x00\x00" (2) + "\x00\x00" (2) + "\x00" (1) + "\x33" (1) + "\x00\x00" (2) = 16 bytes!
    # updateHeader: len_header (2) + fenet (1) + reserved (1) = 4 bytes.
    # So header + updateHeader = 20 bytes!
    # app_instruction = 14 bytes.
    # Total request size = 34 bytes!
    
    company_header = bytearray(b'LSIS-XGT\x00\x00')
    plc_header = bytearray(b'\x00\x00')
    cpu_header = bytearray(b'\x00')
    src_frame_header = bytearray(b'\x33')
    invoke_id_header = bytearray(b'\x00\x00')
    header = company_header + plc_header + cpu_header + src_frame_header + invoke_id_header # 16 bytes
    
    app_instruction = bytearray(b'\x54\x00\x02\x00\x00\x00\x01\x00\x04\x00%MW1') # 14 bytes
    
    len_header = bytes([len(app_instruction), 0]) # 2 bytes
    fenet_header = bytearray(b'\x00') 
    reserved_header = bytearray(b'\x00') 
    
    req = header + len_header + fenet_header + reserved_header + app_instruction
    
    print(f"Request (len={len(req)}): {' '.join(f'{b:02X}' for b in req)}")
    
    try:
        s.sendall(req)
        resp = s.recv(1024)
        print(f"Response (len={len(resp)}): {' '.join(f'{b:02X}' for b in resp)}")
        
        if len(resp) >= 30:
            print(f"resp[20:30]: {' '.join(f'{b:02X}' for b in resp[20:30])}")
            if len(resp) > 30:
                print(f"resp[30:]: {' '.join(f'{b:02X}' for b in resp[30:])}")
        else:
            print("Response is too short!")
            
    except Exception as e:
        print(f"Error during socket communication: {e}")

if __name__ == "__main__":
    test_pyxgt()
