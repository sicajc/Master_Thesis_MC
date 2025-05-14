import random
from math import log2

def generate_st_ld_trace(filename,filename2,pattern_type,num_lines,gen_stall,load_store_switch_threshold,gen_load_store_pattern = False):
    address = 0
    switch_cnt = 0
    threshold = 5
    gen_row_bits = 0

    # operation = random.choice(['ST', 'LD'])
    operation = 'ST'
    # generate marching pattern for it, increment the address
    num_of_channels = 4
    row_size = 2**16    # 64K rows due to 1Gb of memory
    colmun_size = 2**14 # 2K Bytes 2**11 * 2**3
    data_channel_size = 1024 # IO channel siz
    channel_tx_size = int(data_channel_size/8) # Byte addressable
    column_partitions = int(colmun_size/data_channel_size)

    # Generate the address base on these information
    # Ch row col word
    channel_bits = int(log2(num_of_channels))
    row_bits = int(log2(row_size))
    column_bits = int(log2(column_partitions))
    word_bits = int(log2(channel_tx_size))

    # Generate the address
    print("Word bits: ",word_bits)
    print("Column bits: ",column_bits)
    print("Row bits: ",row_bits)
    print("Channel bits: ",channel_bits)

    with open(filename, 'w') as file,open(filename2,'w') as file2:
        for line in range(num_lines):
            if pattern_type != 'read_write_interleave_same_row' and gen_load_store_pattern == True:
                if operation == 'LD' and line != 0:
                    if (line % load_store_switch_threshold*5) == 0:
                        operation = 'ST'
                elif operation == 'ST' and line != 0:
                    if (line % load_store_switch_threshold) == 0:
                        operation = 'LD'

            # Their concatentation
            # Randomly picks different channel
            if(pattern_type == 'worst_case'):
                gen_channel_num = 0
                gen_row_bits    = line
                # Make it a walking pattern
                gen_column_bits = 0
                gen_byte_bits   = 0
            elif(pattern_type == 'random_sequential'):
                gen_channel_num = 0
                # Marching pattern for a small amount of time, then jumps row
                if(switch_cnt == threshold):
                    gen_row_bits = random.randint(0, row_size-1)
                    switch_cnt = 0
                else:
                    gen_row_bits = gen_row_bits
                    switch_cnt += 1

                gen_column_bits = line % column_partitions
                gen_byte_bits   = 0
            elif(pattern_type=='read_write_interleave_same_row'):
                if line % 2 == 0:
                    operation = 'LD'
                else:
                    operation = 'ST'

                # Randomly picks different channel, pick from 0~3
                gen_channel_num = 0
                gen_row_bits    = 0
                # Make it a walking pattern
                gen_column_bits = 0
                gen_byte_bits   = 0
            elif(pattern_type=='ideal_sequential_gen_channel'): # 
                gen_channel_num = line % num_of_channels
                gen_row_bits    = 0
                gen_column_bits = 0
                gen_byte_bits   = 0
            else: #Ideal sequential
                gen_channel_num = 0
                gen_row_bits    = (line // column_partitions) % row_size
                # Walking pattern
                gen_column_bits = line % column_partitions
                gen_byte_bits   = 0


            address = (gen_channel_num << (row_bits + column_bits + word_bits)) | (gen_row_bits << (column_bits + word_bits)) | (gen_column_bits << word_bits) | gen_byte_bits
            # Generate the walking column pattern from col 0~ col100
            # address = (gen_channel_num << (row_bits + column_bits + word_bits)) | (gen_row_bits << (column_bits + word_bits)) | (line % column_partitions << word_bits) | gen_byte_bits

            if(gen_stall==True):
                # stall_cycles = random.randint(0, 10)
                stall_cycles = 0
                data_type = random.randint(0, 1)
                file.write(f"{operation} {address} {stall_cycles} {data_type}\n")
                # Write the value of channel,row,column,word
                file2.write("{0} {1} {2} {3}\n".format(gen_channel_num,gen_row_bits,gen_column_bits,gen_byte_bits))
            else:
                data_type = random.randint(0, 1)
                file.write(f"{operation} {address} {data_type}\n")
                # Write the value of channel,row,column,word
                file2.write("{0} {1} {2} {3}\n".format(gen_channel_num,gen_row_bits,gen_column_bits,gen_byte_bits))

# Parameters
num_traces = 1
num_lines = 2048
trace_file_dir = "../traces/"
gen_stall = True
pattern_type = ''
load_store_switch_threshold = num_lines//2
gen_load_store_pattern = True

random.seed(0)

for i in range(num_traces):
    for no_of_types in range(1):
        if no_of_types == 0:
            pattern_type = 'worst_case'
        # elif no_of_types == 1:
        #     pattern_type = 'random_sequential'
        # elif no_of_types == 2:
        #     pattern_type = 'ideal_sequential'
        # elif no_of_types == 3:
        #     pattern_type = 'read_write_interleave_same_row'
        # elif no_of_types == 4:
        #     pattern_type = 'ideal_sequential_gen_channel'

        filename = f"{trace_file_dir}_{pattern_type}_trace_{i}.txt"
        filename2 = f"{trace_file_dir}_{pattern_type}_trace_{i}_address.txt"
        generate_st_ld_trace(filename,filename2,pattern_type,num_lines,gen_stall,load_store_switch_threshold = load_store_switch_threshold,gen_load_store_pattern = gen_load_store_pattern)
        print(f"Generated trace file: {filename}")
        print(f"Generated trace file: {filename2}")