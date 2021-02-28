"""
    genbytes.py

    DESCRIPTION: Simple script to generate sequential bytes of data for testing
    tx/rx programs

"""

import argparse

def main(args):
    f = open(args.file, 'wb')

    for i in range(args.numpackets):
        for j in range(args.blocksize):
            f.write(str.encode(f'{i}'))
    return 0


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
    main(parser.parse_args())
