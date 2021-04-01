'''
    FILE: test_tx_rx.py

    DESCRIPTION: Black box testing for the tx/rx programs

    https://github.com/oresat/oresat-dxwifi-software

'''

import os
import shutil
import filecmp
import unittest
import subprocess
from time import sleep
from test.genbytes import genbytes


INSTALL_DIR = os.environ.get('DXWIFI_INSTALL_DIR', default='bin/TestDebug')
TEMP_DIR    = '__temp'
TX          = f'./{INSTALL_DIR}/tx'
RX          = f'./{INSTALL_DIR}/rx'


class TestTxRx(unittest.TestCase):


    @classmethod
    def setUpClass(cls):
        '''Setup test environment'''
        if(os.path.isdir(TEMP_DIR)): # Temp dir was not removed correctly on last run
            shutil.rmtree(TEMP_DIR)

        if(not os.access(TX, os.X_OK) or not os.access(RX, os.X_OK)):
            print(f'Error! Verify that the Tx/Rx programs are installed in `{INSTALL_DIR}`')
            exit(1)


    def setUp(self):
        '''Create a directory to store test data'''
        os.mkdir(TEMP_DIR)


    def tearDown(self):
        '''Remove files created during previous test'''
        shutil.rmtree(TEMP_DIR)


    def test_stream_transmission(self):
        '''Tx reads from stdin should match Rx writes to stdout'''

        test_data   = bytes([1 for i in range(1024)])
        tx_out      = f'{TEMP_DIR}/tx.raw'

        tx_command = f'{TX} -q -t 1 -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} -q -t 5 --savefile {tx_out}'

        # Open tx to read from stdin
        tx_proc = subprocess.Popen(tx_command.split(), stdin=subprocess.PIPE)

        # Write test data to tx
        tx_proc.communicate(test_data)

        # Wait for tx to timeout and close
        tx_proc.wait()

        # Open rx to write to stdout
        rx_proc = subprocess.Popen(rx_command.split(), stdout=subprocess.PIPE)

        # Read rx output data
        rx_out = rx_proc.communicate()[0] # Read test data from rx

        # Wait for rx to clean up and close down
        rx_proc.wait()

        # Verify test data matches rx output
        self.assertEqual(test_data, rx_out)


    def test_single_file_transmission(self):
        '''Transmitting a single file is succesfully received and unpackaged'''

        test_file   = f'{TEMP_DIR}/test.raw'
        tx_out      = f'{TEMP_DIR}/tx.raw'
        rx_out      = f'{TEMP_DIR}/rx.raw'

        tx_command = f'{TX} {test_file} -q -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {rx_out} -q -t 2 --savefile {tx_out}'

        # Create a single test file
        genbytes(test_file, 10, 1024) # Create test file

        # Transmit the test file
        subprocess.run(tx_command.split())

        # Receive the test file
        subprocess.run(rx_command.split())

        # Verify both files match
        status = filecmp.cmp(test_file, rx_out)

        self.assertEqual(status, True)


    def test_multi_file_transmission(self):
        '''Sending a list of files results in each file being received'''

        # Create a bunch of test files
        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(10)]
        for file in test_files:
            genbytes(file, 10, 1024)


        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(10)]
        tx_command = f'{TX} {" ".join(test_files)} -q -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -c 1 -t 2 --prefix rx --extension raw --savefile {tx_out}'

        # Transmit all files the test files
        subprocess.run(tx_command.split())

        # Receive all the test files
        subprocess.run(rx_command.split())

        # Verify all the test files match the received files
        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


    def test_directory_transmission(self):
        '''Tx can send all files currently in a directory'''

        # Create a bunch of files in a directory
        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(10)]
        for file in test_files:
            genbytes(file, 10, 1024)

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(10)]
        tx_command = f'{TX} {TEMP_DIR} -q --filter test_*.raw --include-all --no-listen -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -t 2 --prefix rx --extension raw --savefile {tx_out}'

        # Transmit all files in the directory
        subprocess.run(tx_command.split())

        # Receive all the test files
        subprocess.run(rx_command.split())

        # Verify all the test files match the received files
        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


    def test_watch_directory(self):
        '''Tx can watch for new files in a directory and transmit them'''

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(10)]
        tx_command = f'{TX} {TEMP_DIR} -q --watch-timeout 1 --filter test_*.raw -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -c 1 -t 2 --prefix rx --extension raw --savefile {tx_out}'


        # Open tx to listen for new files in a directory
        proc = subprocess.Popen(tx_command.split())

        sleep(0.05) # Give tx time to get set up

        # Create a bunch of test files in the directory, causing them to be transmitted
        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(10)]
        for file in test_files:
            genbytes(file, 10, 1024)

        # Wait for tx to timeout and close
        proc.wait()

        # Receive all the transmitted test files
        subprocess.run(rx_command.split())

        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


if __name__ == '__main__':
    unittest.main()
