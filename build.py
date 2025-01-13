import os
import shutil

os.system('make clean')

# Replace the content of link.lds
link_lds_path = 'link.lds'

with open(link_lds_path, 'r', encoding='utf-8') as file:
    link_file_data = file.read()


link_file_data = link_file_data.replace('0x80080000', '0x40080000')
with open(link_lds_path, 'w', encoding='utf-8') as file:
    file.write(link_file_data)

# Execute the make command
os.system('make')

shutil.copy('build/arm_tiny.bin', 'arm_tiny_vm_0x40080000.bin')

link_file_data = link_file_data.replace('0x40080000', '0x80080000')
with open(link_lds_path, 'w', encoding='utf-8') as file:
    file.write(link_file_data)

os.system('make clean')

# Execute the make command
os.system('make')

shutil.copy('build/arm_tiny.bin', 'arm_tiny_vm_0x80080000.bin')



