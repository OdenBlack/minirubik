.data
times:       .word 0           # Number of bytes to write
             .global main      # Make the main symbol visible to the linker
start_posi:  .byte 0           # Define the starting byte in writable data memory
 
.text
main:
        lw  a0, times          # Load the write count into a0
        li  a1, 0x0F           # Set the byte value to write
        li  t0, 0              # Initialize the number of completed writes
        la  t1, start_posi     # Load the starting address into t1
        jal ra, write_in       # Call the write loop and save the return address
        li  a7, 10             # Select the environment call for program exit
        ecall                  # environment call, Exit the program
                    
write_in:
        bge  t0, a0, end       # Stop when the number of writes reaches the limit
        sb   a1, 0(t1)         # Store one byte at the address held in t1
        addi t1, t1, 1         # Advance to the next byte address
        addi t0, t0, 1         # Count this completed write
        j    write_in          # Repeat the loop
   
end:
        ret                    # Return to the instruction after the jal call