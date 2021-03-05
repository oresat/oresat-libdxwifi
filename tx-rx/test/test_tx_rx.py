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
from test.data.genbytes import genbytes


INSTALL_DIR = 'bin/TestDebug'
TEMP_DIR    = '__temp'
TX          = f'./{INSTALL_DIR}/tx'
RX          = f'./{INSTALL_DIR}/rx'


class TestTxRx(unittest.TestCase):


    @classmethod
    def setUpClass(cls):
        if(os.path.isdir(TEMP_DIR)): # Temp dir was not removed correctly on last run
            shutil.rmtree(TEMP_DIR)

        if(not os.access(TX, os.X_OK) or not os.access(RX, os.X_OK)):
            print(f'Error! Verify that the Tx/Rx programs are installed in `{INSTALL_DIR}`')
            exit(1)


    def setUp(self):
        os.mkdir(TEMP_DIR)


    def tearDown(self):
        shutil.rmtree(TEMP_DIR)


    def test_stream_transmission(self):
        pass


    def test_single_file_transmission(self):

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

        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(100)]
        for file in test_files:
            genbytes(file, 10, 1024)

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(100)]
        tx_command = f'{TX} {" ".join(test_files)} -q -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -t 2 --prefix rx --extension raw --savefile {tx_out}'

        subprocess.run(tx_command.split())
        subprocess.run(rx_command.split())

        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


    def test_directory_transmission(self):

        test_files = [f'{TEMP_DIR}/test_{x}.raw' for x in range(100)]
        for file in test_files:
            genbytes(file, 10, 1024)

        tx_out     = f'{TEMP_DIR}/tx.raw'
        rx_out     = [f'{TEMP_DIR}/rx_{x}.raw' for x in range(100)]
        tx_command = f'{TX} {TEMP_DIR} --filter "test_*.raw" --include-all --no-listen -q -b 1024 --savefile {tx_out}'
        rx_command = f'{RX} {TEMP_DIR} -q -t 2 --prefix rx --extension raw --savefile {tx_out}'

        subprocess.run(tx_command.split())
        subprocess.run(rx_command.split())

        sleep(100)

        results = [filecmp.cmp(src, copy) for src, copy in zip(test_files, rx_out)]

        self.assertEqual(all(results), True)


    def test_watch_directory(self):
        pass


if __name__ == '__main__':
    unittest.main()
