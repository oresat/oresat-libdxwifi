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


INSTALL_DIR = 'bin/TestDebug'
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

        tx_proc = subprocess.Popen(tx_command.split(), stdin=subprocess.PIPE)

        tx_proc.communicate(test_data) # Write test data to tx

        tx_proc.wait()

        rx_proc = subprocess.Popen(rx_command.split(), stdout=subprocess.PIPE)

        rx_out = rx_proc.communicate()[0] # Read test data from rx

        rx_proc.wait()

        self.assertEqual(test_data, rx_out)


    def test_single_file_transmission(self):
        '''Transmitting a single file is succesfully received and unpackaged'''

        test_file   = f'{TEMP_DIR}/test.raw'
        tx_out      = f'{TEMP_DIR}/tx.raw'
        rx_out      = f'{TEMP_DIR}/rx.raw'

        tx_command = f'{TX} {test_file} -q -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {rx_out} -q -t 2 --savefile {tx_out}'

        genbytes(test_file, 10, 1024) # Create test file

        subprocess.run(tx_command.split())
        subprocess.run(rx_command.split())

        status = filecmp.cmp(test_file, rx_out)

        self.assertEqual(status, True)


    def test_multi_file_transmission(self):
        '''Sending a list of files results in each file being received'''

        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(100)]
        for file in test_files:
            genbytes(file, 10, 1024)

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(100)]
        tx_command = f'{TX} {" ".join(test_files)} -q -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -c 1 -t 2 --prefix rx --extension raw --savefile {tx_out}'

        subprocess.run(tx_command.split())
        subprocess.run(rx_command.split())

        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


    def test_directory_transmission(self):
        '''Tx can send all files currently in a directory'''

        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(100)]
        for file in test_files:
            genbytes(file, 10, 1024)

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(100)]
        tx_command = f'{TX} {TEMP_DIR} -q --filter test_*.raw --include-all --no-listen -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -c 1 -t 2 --prefix rx --extension raw --savefile {tx_out}'

        subprocess.run(tx_command.split())
        subprocess.run(rx_command.split())


        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


    def test_watch_directory(self):
        '''Tx can watch for new files in a directory and transmit them'''

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(100)]
        tx_command = f'{TX} {TEMP_DIR} -q --watch-timeout 1 --filter test_*.raw -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -c 1 -t 2 --prefix rx --extension raw --savefile {tx_out}'

        proc = subprocess.Popen(tx_command.split())

        sleep(0.05) # Give tx time to get set up

        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(10)]
        for file in test_files:
            genbytes(file, 10, 1024)
            # There's an issue with inotify where the file name does not accompany the event if we
            # create files really fast. Slow it down here until we figure out a fix. 
            sleep(0.05) 

        proc.wait()

        subprocess.run(rx_command.split())

        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


if __name__ == '__main__':
    unittest.main()
