"""
    genbytes.py

    DESCRIPTION: Simple script to generate sequential bytes of data for testing
    tx/rx programs

"""

import argparse

def genbytes(filename, numpackets, blocksize):
    with open(filename, 'wb') as f:
        for i in range(numpackets):
            for j in range(blocksize):
                f.write(str.encode(f'{i % 10}'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Generate sequential bytes of data into a file"
    )
    parser.add_argument( 
        '-f', 
        '--file',
        default='test.raw',
        help='Output file'
        )
    parser.add_argument(
        '-n',
        '--numpackets',
        default=10,
        type=int,
        help='Number of packets to generate'
    )
    parser.add_argument(
        '-b',
        '--blocksize',
        default=1024,
        type=int,
        help='Size of each packets data'
    )
    args = parser.parse_args()
    genbytes(args.file, args.numpackets, args.blocksize)
