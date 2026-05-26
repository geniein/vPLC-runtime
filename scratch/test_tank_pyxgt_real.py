import sys
import time
from PyXGT.LS import plc_ls

def main():
    print("=== PyXGT Integration Verification Test ===")
    
    server_ip = "127.0.0.1"
    server_port = 2014
    
    try:
        print(f"[Client] Connecting to LS XGT Server at {server_ip}:{server_port}...")
        client = plc_ls(server_ip, server_port)
        print("[Client] Connected successfully!\n")
    except Exception as e:
        print(f"[Error] Connection failed: {e}")
        sys.exit(1)

    try:
        # 1. Word Read Test (%MW1)
        print("--- 1. Word Read Test (%MW1 - Target Setpoint) ---")
        read_vals_m1 = client.command('XGT', 'read', 'word', 'M1')
        print(f"[Read Success] M1 value: {read_vals_m1}")
        assert isinstance(read_vals_m1, list) and len(read_vals_m1) > 0, "M1 read failed!"
        print("[Verify OK]\n")

        # 2. Word Write Test (%MW2)
        print("--- 2. Word Write Test (%MW2 <- 450) ---")
        client.command('XGT', 'write', 'word', 'M2', '450')
        print("[Write Sent] Word 450 written to M2.")
        
        # Read back to verify
        time.sleep(0.1)
        read_vals_m2 = client.command('XGT', 'read', 'word', 'M2')
        print(f"[Read Back] M2 value: {read_vals_m2}")
        assert read_vals_m2 == [450], f"M2 write verification failed! Expected [450], got {read_vals_m2}"
        print("[Verify OK]\n")

        # 3. Bit Read Test (%QX0.0)
        print("--- 3. Bit Read Test (%QX0.0 - Inlet Valve Act) ---")
        read_bit_m0 = client.command('XGT', 'read', 'bit', 'Q0.0')
        print(f"[Read Success] Q0.0 value: {read_bit_m0}")
        assert isinstance(read_bit_m0, list) and len(read_bit_m0) > 0, "Q0.0 read failed!"
        print("[Verify OK]\n")

        # 4. Bit Write Test (%QX0.0 <- 1)
        print("--- 4. Bit Write Test (%QX0.0 <- 1) ---")
        client.command('XGT', 'write', 'bit', 'Q0.0', '1')
        print("[Write Sent] Bit 1 written to Q0.0.")
        
        # Read back to verify
        time.sleep(0.1)
        read_bit_verify_on = client.command('XGT', 'read', 'bit', 'Q0.0')
        print(f"[Read Back] Q0.0 value: {read_bit_verify_on}")
        assert read_bit_verify_on == [1], f"Q0.0 write verification failed! Expected [1], got {read_bit_verify_on}"
        print("[Verify OK]\n")

        # 5. Bit Write Test (%QX0.0 <- 0)
        print("--- 5. Bit Write Test (%QX0.0 <- 0) ---")
        client.command('XGT', 'write', 'bit', 'Q0.0', '0')
        print("[Write Sent] Bit 0 written to Q0.0.")
        
        # Read back to verify
        time.sleep(0.1)
        read_bit_verify_off = client.command('XGT', 'read', 'bit', 'Q0.0')
        print(f"[Read Back] Q0.0 value: {read_bit_verify_off}")
        assert read_bit_verify_off == [0], f"Q0.0 write verification failed! Expected [0], got {read_bit_verify_off}"
        print("[Verify OK]\n")

        print("==================================================")
        print("ALL TESTS PASSED SUCCESSFULLY! PyXGT INTEGRATION WORKS!")
        print("==================================================")

    except AssertionError as ae:
        print(f"\n[Assertion Failed] {ae}")
        sys.exit(1)
    except Exception as e:
        print(f"\n[Unexpected Error] {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
