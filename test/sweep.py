'''
    FILE: sweep.py

    DESCRIPTION: Automatically run through FEC and error parameters.

    NOTES: This script only works with the `TestDebug` and `TestRel`
    configurations. By default, it will assume the binaries are
    installed in `bin/TestDebug`. If they are installed elsewhere
    please define the `DXWIFI_INSTALL_DIR` environment variable with
    the correct install location.
'''

# Imports
import os
import sys
import shutil
import argparse
import subprocess
import numpy as np

# Generate inclusive arange list from given parameters
def inclusive_arange(start, stop, step):
    return np.arange(start, stop + step / 10, step, dtype = np.float64)

# Get binary paths
INSTALL_DIR = os.environ.get('DXWIFI_INSTALL_DIR', default='bin/TestDebug')
TX = f'./{INSTALL_DIR}/tx'
RX = f'./{INSTALL_DIR}/rx'
ENCODE = f'./{INSTALL_DIR}/encode'
DECODE = f'./{INSTALL_DIR}/decode'

# Verify binaries exist
if not all([os.access(binary, os.X_OK) for binary in (TX, RX, ENCODE, DECODE)]):
    print(f"Error! Please verify all programs available at {INSTALL_DIR}.")
    sys.exit(1)

# Parse arguments
metavar_names = ("MIN", "MAX", "STEP")
parser = argparse.ArgumentParser(description = "Automatically sweep through FEC / error parameters.")
parser.add_argument("--code-rate", "-c", type = float, nargs = 3, required = True,
                    help = "FEC code rate", metavar = metavar_names)
parser.add_argument("--error-rate", "-e", type = float, nargs = 3, required = True,
                    help = "bit error rate", metavar = metavar_names)
parser.add_argument("--packet-loss", "-p", type = float, nargs = 3, required = True,
                    help = "packet loss", metavar = metavar_names)
parser.add_argument("--source", "-s", required = True, help = "source file path")
parser.add_argument("--output", "-o", required = True, help = "output directory path")
args = parser.parse_args()

# Generate sweep lists
code_rates = inclusive_arange(*args.code_rate)
error_rates = inclusive_arange(*args.error_rate)
packet_losses = inclusive_arange(*args.packet_loss)

# Make (or overwrite) the output directory
if os.path.exists(args.output):
    shutil.rmtree(args.output)
os.mkdir(args.output)

# Iterate over code rates
for cr in code_rates:

    # Make a directory for this code rate
    cr_dir = os.path.join(args.output, f"CodeRate{cr}")
    os.mkdir(cr_dir)

    # Perform encoding
    cr_output = os.path.join(cr_dir, "file.encoded")
    encode_command = f"{ENCODE} {args.source} -o {cr_output} -c {cr}"
    with open(os.path.join(cr_dir, "encode_output.txt"), "w") as f:
        subprocess.run(encode_command.split(), stdout = f, stderr = f)

    # Iterate over error rates
    for er in error_rates:

        # Iterate over packet losses
        for pl in packet_losses:

            # Make a directory for this combination
            er_pl_dir = os.path.join(cr_dir, f"ErrorRate{er}_PacketLoss{pl}")
            os.mkdir(er_pl_dir)

            # Perform "transmission"
            tx_output = os.path.join(er_pl_dir, "file.sent")
            tx_command = f"{TX} -e {er} -p {pl} --savefile {tx_output} {cr_output}"
            with open(os.path.join(er_pl_dir, "tx_output.txt"), "w") as f:
                subprocess.run(tx_command.split(), stdout = f, stderr = f)

            # Perform "reception"
            rx_output = os.path.join(er_pl_dir, "file.received")
            rx_command = f"{RX} --savefile {tx_output} {rx_output}"
            with open(os.path.join(er_pl_dir, "rx_output.txt"), "w") as f:
                subprocess.run(rx_command.split(), stdout = f, stderr = f)

            # Perform decoding
            decode_output = os.path.join(er_pl_dir, "file.decoded")
            decode_command = f"{DECODE} {rx_output} -o {decode_output}"
            with open(os.path.join(er_pl_dir, "decode_output.txt"), "w") as f:
                subprocess.run(decode_command.split(), stdout = f, stderr = f)

            # Notify user
            print(f"Finished packet loss rate {pl} for error rate {er} with code rate {cr}.")