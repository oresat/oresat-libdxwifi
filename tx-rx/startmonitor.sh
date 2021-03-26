#!/bin/bash
# Script attempts to enable a network interface into monitor mode.
# Note: Must be run with super user privileges


set -e 

function cleanup()
{
    if [[ $? -ne 0 ]]; then
        echo -e "Usage: sudo ./startmonitor.sh [interface:mon0] [channel:3] [txpower:20]"
    else
        echo -e "Enabled $dev in monitor mode"
    fi
}

# keep track of the last executed command
trap 'last_command=$current_command; current_command=$BASH_COMMAND' DEBUG
# echo message before exiting
trap cleanup EXIT

dev=${1:-mon0}

ip link set $dev down
iw dev $dev set monitor fcsfail otherbss 
ip link set $dev up
iw dev $dev set channel ${2:-3}
iw dev $dev info
exit 0
