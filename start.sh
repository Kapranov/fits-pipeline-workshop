#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "Usage: $0 [--local | --private | --all]"
    exit 1
fi

case "$1" in
    --local|-local)
        echo "Starting up local network..."
        jupyter notebook create_fits_image.ipynb
        ;;

    --private|-private)
        echo "Starting up private network..."
        jupyter notebook --ip=192.168.2.157 --port 4001
        ;;

    --all|-all)
        echo "Starting up both networks..."
        jupyter notebook create_fits_image.ipynb &
        sleep 2
        jupyter notebook --ip=192.168.2.157 --port 4001
        ;;

    *)
        echo "Invalid option: $1"
        echo "Usage: $0 [--local | --private | --all]"
        exit 1
        ;;
esac
