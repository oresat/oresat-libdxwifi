'''
    FILE: test_tx_rx.py

    DESCRIPTION: Black box testing for the tx/rx programs

    https://github.com/oresat/oresat-dxwifi-software

    NOTES: This test program runs the Tx/Rx programs in `offline` mode and 
    only works with the `TestDebug` and `TestRel` configurations. 

    By default, it will assume the binaries are installed in `bin/TestDebug`. 
    If they are installed elsewhere please define the `DXWIFI_INSTALL_DIR` 
    environment variable with the correct install location. 

    If the environment variable `DXWIFI_MEMORY_CHECK` is defined then this test
    program will run all tests cases through the `valgrind` utility and output
    the results to stderr
'''

import os
import signal
import shutil
import filecmp
import unittest
import subprocess
from time import sleep
from test.genbytes import genbytes

FEC_SYMBOL_SIZE = 1099

INSTALL_DIR = os.environ.get('DXWIFI_INSTALL_DIR', default='bin/TestDebug')
TEMP_DIR    = '__temp'

TX_INSTALL = f'./{INSTALL_DIR}/tx'
RX_INSTALL = f'./{INSTALL_DIR}/tx'

if 'DXWIFI_MEMORY_CHECK' in os.environ: 
    TX = f'valgrind --leak-check=full ./{INSTALL_DIR}/tx'
    RX = f'valgrind --leak-check=full ./{INSTALL_DIR}/rx'
else:
    TX = f'./{INSTALL_DIR}/tx'
    RX = f'./{INSTALL_DIR}/rx'


class TestTxRx(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        '''Setup test environment'''

        if(os.path.isdir(TEMP_DIR)): 
            print("Warning: Temp dir was not removed correctly on last run, removing now")
            shutil.rmtree(TEMP_DIR)

        if(not os.access(TX_INSTALL, os.X_OK) or not os.access(RX_INSTALL, os.X_OK)):
            print(f'Error! Verify that the Tx/Rx programs are installed in `{INSTALL_DIR}`')
            exit(1)

    def setUp(self):
        '''Create a directory to store test data'''
        os.mkdir(TEMP_DIR)


    def tearDown(self):
        '''Remove files created during previous test'''
        shutil.rmtree(TEMP_DIR)

    
    def testStreamTransmission(self):
        '''Tx reads from stdin should match Rx writes to stdout'''

        test_data   = bytes([1 for i in range(1275)])
        tx_out      = f'{TEMP_DIR}/tx.raw'

        tx_command = f'{TX} -q -t 1 --savefile {tx_out}'
        rx_command = f'{RX} -q -t 5 --savefile {tx_out}'

        # Open tx to read from stdin
        tx_proc = subprocess.Popen(tx_command.split(), stdin=subprocess.PIPE)

        # Write test data to tx
        tx_proc.communicate(test_data)

        # Wait for tx to timeout and close
        tx_proc.wait()

        # Verify tx exited cleanly
        self.assertEqual(tx_proc.returncode, 0)

        # Open rx to write to stdout
        rx_proc = subprocess.Popen(rx_command.split(), stdout=subprocess.PIPE)

        # Read rx output data
        rx_out = rx_proc.communicate()[0] # Read test data from rx

        # Wait for rx to clean up and close down
        rx_proc.wait()

        # Verify rx exited cleanly
        self.assertEqual(rx_proc.returncode, 0)

        # Verify test data matches rx output
        self.assertEqual(test_data, rx_out)


    def testSingleFileTransmission(self):
        '''Transmitting a single file is succesfully received and unpackaged'''

        test_file   = f'{TEMP_DIR}/test.raw'
        tx_out      = f'{TEMP_DIR}/tx.raw'
        rx_out      = f'{TEMP_DIR}/rx.raw'

        tx_command = f'{TX} {test_file} -q --savefile {tx_out}'
        rx_command = f'{RX} {rx_out} -q -t 2 --savefile {tx_out}'

        # Create a single test file
        genbytes(test_file, 10, FEC_SYMBOL_SIZE) # Create test file

        # Transmit the test file
        subprocess.run(tx_command.split()).check_returncode()

        # Receive the test file
        subprocess.run(rx_command.split()).check_returncode()

        # Verify both files match
        status = filecmp.cmp(test_file, rx_out)

        self.assertEqual(status, True)


    def testMultiFileTransmission(self):
        '''Sending a list of files results in each file being received'''

        # Create a bunch of test files
        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(10)]
        for file in test_files:
            genbytes(file, 10, FEC_SYMBOL_SIZE)


        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(10)]
        tx_command = f'{TX} {" ".join(test_files)} -q --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -c 1 -t 2 --prefix rx --extension raw --savefile {tx_out}'

        # Transmit all files the test files
        subprocess.run(tx_command.split()).check_returncode()

        # Receive all the test files
        subprocess.run(rx_command.split()).check_returncode()

        # Verify all the test files match the received files
        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


    def testDirectoryTransmission(self):
        '''Tx can send all files currently in a directory'''

        # Create a bunch of files in a directory
        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(10)]
        for file in test_files:
            genbytes(file, 10, FEC_SYMBOL_SIZE)

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(10)]
        tx_command = f'{TX} {TEMP_DIR} -q --filter test_*.raw --include-all --no-listen --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -t 2 --prefix rx --extension raw --savefile {tx_out}'

        # Transmit all files in the directory
        subprocess.run(tx_command.split()).check_returncode()

        # Receive all the test files
        subprocess.run(rx_command.split()).check_returncode()

        # Verify all the test files match the received files
        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


    def testWatchDirectory(self):
        '''Tx can watch for new files in a directory and transmit them'''

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(10)]
        tx_command = f'{TX_INSTALL} {TEMP_DIR} -q --watch-timeout 2 --filter=test_*.raw --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -c 1 -t 2 --prefix rx --extension raw --savefile {tx_out}'


        # Open tx to listen for new files in a directory
        proc = subprocess.Popen(tx_command.split())

        sleep(0.05) # Give tx time to get set up

        # Create a bunch of test files in the directory, causing them to be transmitted
        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(10)]
        for file in test_files:
            genbytes(file, 10, FEC_SYMBOL_SIZE)

        # Wait for tx to timeout and close
        proc.wait()

        # Verify tx exited cleanly
        self.assertEqual(proc.returncode, 0)

        # Receive all the transmitted test files
        subprocess.run(rx_command.split())

        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)

    """
    This test will fail until issue #44 is fixed and merged.
    https://github.com/oresat/oresat-dxwifi-software/issues/44
    
    Since the test image is not divisible by the DXWIFI_PAYLOAD_SIZE the final 
    packet is zero filled. This causes the filecmp to fail since the received
    file has extra zeroes at the end.

    def testSmallImageTransmission(self):
        '''Small (~1mb), uncompressed images can be transmitted and recieved'''

        test_file   = f'test/data/daisy.bmp'
        tx_out      = f'{TEMP_DIR}/tx.raw'
        rx_out      = f'{TEMP_DIR}/rx.raw'

        tx_command = f'{TX} {test_file} -q --savefile {tx_out} --ordered'
        rx_command = f'{RX} {rx_out} -q -t 2 --savefile {tx_out} --ordered'

        # Transmit the test file
        subprocess.run(tx_command.split()).check_returncode()

        # Receive the test file
        subprocess.run(rx_command.split()).check_returncode()

        # Verify both files match
        status = filecmp.cmp(test_file, rx_out)

        self.assertEqual(status, True)
    """

    def testForwardErrorCorrection(self):
        '''Tx and Rx can recover and correct for bit errors'''

        test_file   = f'{TEMP_DIR}/test.raw'
        tx_out      = f'{TEMP_DIR}/tx.raw'
        rx_out      = f'{TEMP_DIR}/rx.raw'

        tx_command = f'{TX} {test_file} -q --error-rate 0.004 --savefile {tx_out}'
        rx_command = f'{RX} {rx_out} -q -t 2 --savefile {tx_out}'

        # Create a single test file
        genbytes(test_file, 10, FEC_SYMBOL_SIZE) # Create test file

        # Transmit the test file
        subprocess.run(tx_command.split()).check_returncode()

        # Receive the test file
        subprocess.run(rx_command.split()).check_returncode()

        # Verify both files match
        status = filecmp.cmp(test_file, rx_out)

        self.assertEqual(status, True)

    def testForwardErasureCorrection(self):
        '''Tx and Rx can recover and correct for packet loss'''

        test_file   = f'{TEMP_DIR}/test.raw'
        tx_out      = f'{TEMP_DIR}/tx.raw'
        rx_out      = f'{TEMP_DIR}/rx.raw'

        tx_command = f'{TX} {test_file} -q --packet-loss 0.1 --savefile {tx_out}'
        rx_command = f'{RX} {rx_out} -q --savefile {tx_out}'

        # Create a single test file
        genbytes(test_file, 10, FEC_SYMBOL_SIZE) # Create test file

        # Transmit the test file
        subprocess.run(tx_command.split()).check_returncode()

        # Receive the test file
        subprocess.run(rx_command.split()).check_returncode()

        # Verify both files match
        status = filecmp.cmp(test_file, rx_out)

        self.assertEqual(status, True)

    def testForwardErasureAndErrorCorrection(self):
        '''Tx and Rx can recover and correct for bit errors and packet loss'''

        test_file   = f'{TEMP_DIR}/test.raw'
        tx_out      = f'{TEMP_DIR}/tx.raw'
        rx_out      = f'{TEMP_DIR}/rx.raw'

        tx_command = f'{TX} {test_file} -q --packet-loss 0.1 --error-rate 0.004 --savefile {tx_out}'
        rx_command = f'{RX} {rx_out} -q --savefile {tx_out}'

        # Create a single test file
        genbytes(test_file, 10, FEC_SYMBOL_SIZE) # Create test file

        # Transmit the test file
        subprocess.run(tx_command.split()).check_returncode()

        # Receive the test file
        subprocess.run(rx_command.split()).check_returncode()

        # Verify both files match
        status = filecmp.cmp(test_file, rx_out)

        self.assertEqual(status, True)


if __name__ == '__main__':
    unittest.main()
